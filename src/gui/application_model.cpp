#include "application_model.h"
#include "audio/alsa_mixer.h"
#include "audio/filter_chain.h"
#include "devices/inventory.h"
#include "translation_manager.h"
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#ifdef Q_OS_LINUX
#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>
#endif

namespace open_icue {
namespace {
constexpr int DEVICE_SCAN_INTERVAL_MS = 5000;
constexpr int PROCESS_TIMEOUT_MS = 1500;
constexpr int AUDIO_EDIT_DEBOUNCE_MS = 200;
constexpr int VOICE_PROMPT_RATE = 48000;
constexpr int MAX_DOCUMENT_FIELD_BYTES = 1024 * 1024;
constexpr int MAX_LIGHTING_COLORS = 12;
constexpr int HARDWARE_POLL_INTERVAL_MS = 30000;
constexpr int HARDWARE_FRAME_INTERVAL_MS = 50;
constexpr double MAX_LIGHTING_SPEED = 2.0;
constexpr double MIN_LIGHTING_DURATION = 0.1;
constexpr double MAX_LIGHTING_DURATION = 60.0;
constexpr double MAX_LIGHTING_OPACITY = 100.0;
QByteArray json(const QVariant& value) {
    return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
}

/** One serialized hardware transaction replays all connected routes of an active profile. */
QVariantMap applyLightingFrames(const std::shared_ptr<HardwareBackend>& backend,
                                const QVariantList& frames, const QString& selectedModel,
                                double seconds) {
    QVariantList results;
    QVariantMap selected;
    for (const auto& value : frames) {
        const auto frame = value.toMap();
        const auto model = frame.value("modelId").toString();
        auto result = backend->applyLighting(model, frame.value("layers").toList(),
                                             frame.value("zones").toList(), seconds);
        if (!result.value("isOk").toBool())
            return result;
        results.append(result);
        if (model == selectedModel)
            selected = result;
    }
    selected.insert("isOk", true);
    selected.insert("isLightingActive", !results.isEmpty());
    selected.insert("profileDeviceResults", results);
    return selected;
}
} // namespace

ApplicationModel::ApplicationModel(const QString& stateDirectory, QObject* parent)
    : QObject(parent), workspace_(stateDirectory) {
    scanTimer_.setInterval(DEVICE_SCAN_INTERVAL_MS);
    connect(&scanTimer_, &QTimer::timeout, this, &ApplicationModel::refresh);
    audioUpdateTimer_.setSingleShot(true);
    audioUpdateTimer_.setInterval(AUDIO_EDIT_DEBOUNCE_MS);
    connect(&audioUpdateTimer_, &QTimer::timeout, this, [this] {
        if (isAudioClientRunning())
            startAudioForDevice(audioTarget_, audioModelId_);
        else if (hardware_ && selectedModelId_ == "virtuoso_black") {
            const auto target = headsetAudioTarget();
            if (!target.isEmpty())
                startAudioForDevice(target, selectedModelId_);
        }
    });
    connect(&filter_, &QProcess::started, this, &ApplicationModel::stateChanged);
    connect(&filter_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setNotice("PipeWire: " + filter_.errorString());
        emit stateChanged();
    });
    connect(&filter_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (audioRoute_.isActive()) {
            try {
                audioRoute_.deactivate();
            } catch (const std::exception& error) {
                setNotice(QString::fromUtf8(error.what()));
            }
        }
        if (code != 0 && status == QProcess::NormalExit)
            setNotice(QString::fromUtf8(filter_.readAllStandardError()).left(512));
        emit stateChanged();
    });
    connect(&voicePrompt_, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus status) {
                voicePromptState_["isFinished"] = true;
                voicePromptState_["exitCode"] = code;
                voicePromptState_["isOk"] = status == QProcess::NormalExit && code == 0;
                if (status == QProcess::NormalExit && code != 0)
                    setNotice("Sprachansage konnte nicht gestartet werden.");
                emit stateChanged();
            });
#ifdef Q_OS_LINUX
    const auto ownerPid = ::getpid();
    filter_.setChildProcessModifier([this, ownerPid] {
        if (::prctl(PR_SET_PDEATHSIG, SIGTERM) != 0)
            filter_.failChildProcessModifier("Cannot bind filter lifetime to its owner");
        if (::getppid() != ownerPid)
            ::_exit(1);
    });
#endif
    refresh();
    for (const auto& value : profiles()) {
        const auto profile = value.toMap();
        if (profile.value("id").toString() == activeProfileId())
            isProfileLightingRequested_ = profile.value("devices").toMap().size() > 1;
    }
    scanTimer_.start();
}
ApplicationModel::~ApplicationModel() {
    hardwareTimer_.stop();
    lightingTimer_.stop();
    hardwareFuture_.waitForFinished();
    if (hardware_)
        hardware_->stopLighting();
    voicePrompt_.terminate();
    voicePrompt_.waitForFinished(PROCESS_TIMEOUT_MS);
    stopAudio();
}

void ApplicationModel::enableHardware(std::shared_ptr<HardwareBackend> injectedBackend) {
    if (hardware_)
        return;
    hardware_ = injectedBackend ? std::move(injectedBackend)
                                : std::make_shared<HardwareBackend>(workspace_.directory());
    connect(&hardwareFuture_, &QFutureWatcher<QVariantMap>::finished, this, [this] {
        isHardwarePending_ = false;
        auto result = hardwareFuture_.result();
        if (hardwareAction_ == "apply" || hardwareAction_ == "frame") {
            hardwarePreviewColors_ = result.take("previewColors").toMap();
            lightingState_ = result;
            lightingState_["revision"] = QVariant::fromValue(submittedLightingRevision_);
            emit lightingStateChanged();
        }
        if (result.contains("devices"))
            hardwareState_ = result;
        if (hardwareAction_ == "dpi")
            dpiState_ = result;
        if ((hardwareAction_ == "stop" || hardwareAction_ == "pair") ||
            ((hardwareAction_ == "apply" || hardwareAction_ == "frame") &&
             !result.value("isOk").toBool()) ||
            (result.contains("canApplyDpi") && !result.value("canApplyDpi").toBool()))
            dpiState_["isDpiActive"] = false;
        if (hardwareAction_ != "poll" && hardwareAction_ != "frame") {
            lastOperation_ = result;
            if (!result.value("isOk").toBool())
                setNotice(result.value("error").toString());
            else if (hardwareAction_ == "pair")
                setNotice(
                    result.value("isAlreadyPaired").toBool()
                        ? "Darkstar ist bereits mit diesem Empfänger verbunden."
                        : "Kopplungsdaten bestätigt. USB-Kabel entfernen und 2,4 GHz testen.");
            else if (hardwareAction_ == "apply") {
                lightingModel_ = submittedLightingModel_;
                setNotice("RGB-Daten vom Gerät bestätigt. Software-Beleuchtung aktiv.");
                lightingClock_.restart();
                if (isLightingEditorActive_ && desiredLightingModel_ == lightingModel_)
                    lightingTimer_.start();
            } else if (hardwareAction_ == "dpi")
                setNotice("Aktiver DPI-Wert vom Gerät bestätigt und zurückgelesen.");
        }
        if ((hardwareAction_ == "apply" || hardwareAction_ == "frame") &&
            !result.value("isOk").toBool()) {
            lastOperation_ = result;
            lightingTimer_.stop();
            lightingModel_.clear();
            setNotice(result.value("error").toString());
            if (lightingRevision_ == submittedLightingRevision_) {
                hasLightingUpdate_ = false;
                desiredLightingModel_.clear();
            }
        }
        if (hardwareAction_ == "stop") {
            lightingModel_.clear();
            hardwarePreviewColors_.clear();
            emit lightingStateChanged();
        }
        if (hardwareAction_ != "frame" || !result.value("isOk").toBool())
            emit stateChanged();
        if (hardwareAction_ == "poll" && isLightingEditorActive_ && lightingModel_.isEmpty())
            requestLiveLighting();
        else if (shouldReleaseControl_ || hasLightingUpdate_ ||
                 desiredLightingModel_ != lightingModel_)
            QTimer::singleShot(0, this, &ApplicationModel::synchronizeLighting);
    });
    hardwareTimer_.setInterval(HARDWARE_POLL_INTERVAL_MS);
    connect(&hardwareTimer_, &QTimer::timeout, this, &ApplicationModel::probeHardware);
    hardwareTimer_.start();
    lightingTimer_.setInterval(HARDWARE_FRAME_INTERVAL_MS);
    connect(&lightingTimer_, &QTimer::timeout, this, [this] {
        if (isHardwareBusy() || lightingModel_.isEmpty() || !isLightingEditorActive_)
            return;
        if (hasLightingUpdate_ || lightingModel_ != desiredLightingModel_) {
            synchronizeLighting();
            return;
        }
        const auto backend = hardware_;
        const auto model = lightingModel_;
        const auto frames = appliedLightingFrames_;
        const double seconds = lightingClock_.elapsed() / 1000.0;
        scheduleHardware("frame",
                         [=] { return applyLightingFrames(backend, frames, model, seconds); });
    });
    const auto backend = hardware_;
    scheduleHardware("poll", [backend] {
        backend->restoreInterruptedPairing();
        return backend->probe();
    });
}
void ApplicationModel::scheduleHardware(const QString& action,
                                        const std::function<QVariantMap()>& operation) {
    if (!hardware_ || isHardwareBusy()) {
        lastOperation_ = {{"isOk", false}, {"error", "Hardware operation unavailable or busy"}};
        setNotice(lastOperation_.value("error").toString());
        return;
    }
    hardwareAction_ = action;
    if (action == "apply" || action == "frame") {
        submittedLightingRevision_ = lightingRevision_;
        submittedLightingModel_ = desiredLightingModel_;
    }
    isHardwarePending_ = true;
    if (action != "poll" && action != "frame")
        lastOperation_ = {{"isOk", true}, {"isPending", true}, {"error", QString()}};
    hardwareFuture_.setFuture(QtConcurrent::run([operation, backend = hardware_, action] {
        try {
            return operation();
        } catch (const std::exception& error) {
            if (action == "frame" || action == "apply")
                backend->stopLighting();
            return QVariantMap{{"isOk", false}, {"error", QString::fromUtf8(error.what())}};
        }
    }));
    if (action != "frame")
        emit stateChanged();
}
void ApplicationModel::probeHardware() {
    if (!hardware_ || isHardwareBusy())
        return;
    const auto backend = hardware_;
    scheduleHardware("poll", [backend] { return backend->probe(); });
}
void ApplicationModel::pairDarkstar() {
    if (!hardware_ || isHardwareBusy()) {
        setNotice("Hardware operation unavailable or busy");
        return;
    }
    lightingTimer_.stop();
    lightingModel_.clear();
    desiredLightingModel_.clear();
    hasLightingUpdate_ = false;
    const auto backend = hardware_;
    scheduleHardware("pair", [backend] { return backend->pairDarkstar(); });
}
void ApplicationModel::stopHardwareLighting() {
    lightingTimer_.stop();
    isLightingEditorActive_ = false;
    desiredLightingModel_.clear();
    hasLightingUpdate_ = true;
    shouldReleaseControl_ = true;
    synchronizeLighting();
}
void ApplicationModel::setLightingEditorActive(bool isActive) {
    isLightingEditorActive_ = isActive;
    requestLiveLighting();
}
void ApplicationModel::requestLiveLighting() {
    if (!hardware_)
        return;
    const auto layers = document().value("lightingLayers").toList();
    const auto selected = device();
    QVariantList frames;
    if (isLightingEditorActive_ && !selected.value("isReceiver").toBool()) {
        if (isProfileLightingRequested_) {
            for (const auto& value : devices()) {
                const auto entry = value.toMap();
                if (entry.value("isReceiver").toBool() || !entry.value("isConnected").toBool())
                    continue;
                const auto id = entry.value("id").toString();
                frames.append(QVariantMap{
                    {"modelId", id},
                    {"layers",
                     workspace_.device(id, catalog_.initialDocument(id)).value("lightingLayers")},
                    {"zones", catalog_.lightingKeys(id)}});
            }
        } else if (selected.value("isConnected").toBool() &&
                   (!layers.isEmpty() || lightingModel_ == selectedModelId_ ||
                    desiredLightingModel_ == selectedModelId_)) {
            frames.append(QVariantMap{
                {"modelId", selectedModelId_}, {"layers", layers}, {"zones", lightingKeys()}});
        }
    }
    const bool canSend = !frames.isEmpty();
    desiredLightingModel_ = canSend ? selectedModelId_ : QString();
    appliedLightingFrames_ = frames;
    ++lightingRevision_;
    hasLightingUpdate_ = true;
    synchronizeLighting();
}
void ApplicationModel::synchronizeLighting() {
    if (!hardware_ || isHardwareBusy())
        return;
    const auto backend = hardware_;
    if (shouldReleaseControl_) {
        shouldReleaseControl_ = false;
        scheduleHardware("stop", [backend] { return backend->releaseControl(); });
        return;
    }
    if (desiredLightingModel_ != lightingModel_ && !lightingModel_.isEmpty()) {
        lightingTimer_.stop();
        scheduleHardware("stop", [backend] { return backend->releaseControl(); });
        return;
    }
    if (desiredLightingModel_.isEmpty()) {
        lightingTimer_.stop();
        hasLightingUpdate_ = false;
        return;
    }
    if (!hasLightingUpdate_)
        return;
    const auto model = desiredLightingModel_;
    const auto frames = appliedLightingFrames_;
    const bool isFirstFrame = lightingModel_ != model;
    const double seconds = isFirstFrame ? 0.0 : lightingClock_.elapsed() / 1000.0;
    hasLightingUpdate_ = false;
    scheduleHardware(isFirstFrame ? "apply" : "frame",
                     [=] { return applyLightingFrames(backend, frames, model, seconds); });
}

QVariantList ApplicationModel::devices() const {
    auto result = catalog_.models();
    for (auto& value : result) {
        auto item = value.toMap();
        for (const auto& usb : usbDevices_)
            if (usb.toMap().value("modelId") == item.value("id")) {
                item["isConnected"] = true;
                item["transport"] = usb;
                break;
            }
        value = item;
        for (const auto& telemetry : hardwareState_.value("devices").toList()) {
            const auto state = telemetry.toMap();
            if (state.value("modelId") == item.value("id") && state.value("isVerified").toBool()) {
                item["telemetry"] = state;
                item["isConnected"] = true;
            }
        }
        value = item;
    }
    return result;
}
QVariantList ApplicationModel::profiles() const {
    return workspace_.snapshot().value("profiles").toList();
}
namespace {
QVariantList translatedLabels(QVariantList values) {
    for (auto& value : values) {
        auto item = value.toMap();
        const auto source = item.value("label").toString().toUtf8();
        item["label"] = QCoreApplication::translate("OpenICUE", source.constData());
        value = item;
    }
    return values;
}
} // namespace
QVariantList ApplicationModel::navigation() const {
    return translatedLabels(catalog_.navigation(selectedModelId_));
}
QVariantList ApplicationModel::keys() const {
    return translatedLabels(catalog_.keys(selectedModelId_));
}
QVariantList ApplicationModel::lightingKeys() const {
    return translatedLabels(catalog_.lightingKeys(selectedModelId_));
}
QVariantList ApplicationModel::lightingTypes() const {
    return translatedLabels(catalog_.lightingTypes(selectedModelId_));
}
QString ApplicationModel::notice() const {
    auto result = translateOperationMessage(notice_);
    for (const auto& argument : noticeArguments_)
        result = result.arg(argument);
    return result;
}
void ApplicationModel::retranslate() {
    emit stateChanged();
    emit noticeChanged();
}
QVariantMap ApplicationModel::device() const {
    for (const auto& item : devices())
        if (item.toMap().value("id") == selectedModelId_)
            return item.toMap();
    return {};
}
QVariantMap ApplicationModel::document() const {
    return workspace_.device(selectedModelId_, catalog_.initialDocument(selectedModelId_));
}
QVariantList ApplicationModel::usbInventory() const {
    QVariantList result;
    for (const auto& entry : usbDevices_) {
        const auto model = entry.toMap().value("modelId").toString();
        if (model == selectedModelId_ ||
            (selectedModelId_ == "virtuoso_black" && model == "virtuoso_black_dongle") ||
            (selectedModelId_ == "darkstar_wireless" && model == "slipstream_wireless_dongle_ss"))
            result.append(entry);
    }
    return result;
}
QVariantList ApplicationModel::audioTargets() const {
    if (selectedModelId_ != "virtuoso_black")
        return {};
    return headsetAudioTargetsForUsb(audioTargets_, selectedUsbPath());
}
QString ApplicationModel::headsetAudioTarget() const {
    const auto targets = audioTargets();
    for (const auto& entry : targets)
        if (entry.toMap().value("id").toString() == audioTarget_ ||
            entry.toMap().value("isDefault").toBool())
            return entry.toMap().value("id").toString();
    return targets.isEmpty() ? QString() : targets.first().toMap().value("id").toString();
}
QString ApplicationModel::activeProfileId() const {
    return workspace_.snapshot().value("activeProfileId").toString();
}
QString ApplicationModel::selectedUsbPath() const {
    const auto direct = device().value("transport").toMap().value("sysPath").toString();
    if (!direct.isEmpty())
        return direct;
    if (selectedModelId_ == "virtuoso_black") {
        for (const auto& usb : usbDevices_)
            if (usb.toMap().value("modelId") == "virtuoso_black_dongle")
                return usb.toMap().value("sysPath").toString();
    }
    return {};
}

void ApplicationModel::perform(const std::function<void()>& operation, const QString& success) {
    try {
        operation();
        lastOperation_ = {{"isOk", true}, {"error", QString()}};
        if (!success.isEmpty())
            setNotice(success);
        emit stateChanged();
    } catch (const std::exception& error) {
        const auto message = QString::fromUtf8(error.what());
        lastOperation_ = {{"isOk", false}, {"error", message}};
        setNotice(message);
    }
}
void ApplicationModel::setNotice(const QString& text, const QStringList& arguments) {
    notice_ = text;
    noticeArguments_ = arguments;
    emit noticeChanged();
}
void ApplicationModel::persist(const QString& key, const QVariant& value) {
    workspace_.updateDevice(selectedModelId_, key, value,
                            catalog_.initialDocument(selectedModelId_));
}
void ApplicationModel::persistFields(const QVariantMap& fields) {
    workspace_.updateDeviceFields(selectedModelId_, fields,
                                  catalog_.initialDocument(selectedModelId_));
}
void ApplicationModel::selectDevice(const QString& id) {
    perform([&] {
        if (catalog_.definition(id).isEmpty())
            throw std::invalid_argument("Unknown device model");
        selectedModelId_ = id;
        mixer_.clear();
        refresh();
    });
    if (lastOperation_.value("isOk").toBool())
        requestLiveLighting();
}
void ApplicationModel::selectProfile(const QString& id) {
    perform([&] {
        workspace_.selectProfile(id);
        isProfileLightingRequested_ = true;
        lightingClock_.restart();
        if (isAudioClientRunning())
            audioUpdateTimer_.start();
    });
    if (lastOperation_.value("isOk").toBool())
        requestLiveLighting();
}
QString ApplicationModel::iconSource(const QString& category, const QString& id,
                                     const QString& fallback) const {
    static const QRegularExpression name("^[a-z0-9-]{1,80}$");
    if (!QStringList{"lighting", "assignments", "controls"}.contains(category) ||
        !name.match(id).hasMatch())
        return fallback;
    const QString path = category + '/' + id;
    const auto fallbackPath = fallback.startsWith("qrc:/") ? fallback.mid(3) : fallback;
    return QFile::exists(":/app/icons/" + path + ".svg") || !QFile::exists(fallbackPath)
               ? "image://icons/" + path
               : fallback;
}
void ApplicationModel::createProfile(const QString& name) {
    perform([&] {
        workspace_.createProfile(name);
        isProfileLightingRequested_ = true;
        lightingClock_.restart();
    });
    if (lastOperation_.value("isOk").toBool())
        requestLiveLighting();
}
void ApplicationModel::renameProfile(const QString& id, const QString& name) {
    perform([&] { workspace_.renameProfile(id, name); });
}
void ApplicationModel::removeProfile(const QString& id) {
    perform([&] {
        workspace_.removeProfile(id);
        isProfileLightingRequested_ = true;
        lightingClock_.restart();
    });
    if (lastOperation_.value("isOk").toBool())
        requestLiveLighting();
}
void ApplicationModel::duplicateProfile(const QString& id, const QString& name) {
    perform([&] {
        workspace_.duplicateProfile(id, name);
        isProfileLightingRequested_ = true;
        lightingClock_.restart();
    });
    if (lastOperation_.value("isOk").toBool())
        requestLiveLighting();
}
void ApplicationModel::importProfile(const QString& path) {
    perform([&] {
        workspace_.importProfile(QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path);
        isProfileLightingRequested_ = true;
        lightingClock_.restart();
        for (const auto& value : profiles()) {
            const auto profile = value.toMap();
            if (profile.value("id").toString() != activeProfileId())
                continue;
            const auto summary =
                profile.value("sourceMetadata").toMap().value("conversion").toMap();
            setNotice("Profil importiert: %1 Beleuchtungsebenen übernommen; %2 im Original-XML "
                      "erhalten. Live-Vorschau im Beleuchtungseditor.",
                      {QString::number(summary.value("convertedLightingLayers").toInt()),
                       QString::number(summary.value("preservedLightingLayers").toInt())});
        }
    });
    if (lastOperation_.value("isOk").toBool())
        requestLiveLighting();
}
void ApplicationModel::setDocumentValue(const QString& key, const QVariant& input) {
    perform([&] {
        // QML JavaScript arrays and objects arrive wrapped in QVariant as QJSValue.
        const QVariant value = input.metaType().id() == qMetaTypeId<QJSValue>()
                                   ? input.value<QJSValue>().toVariant()
                                   : input;
        static const QStringList keys = {"isLinkedEq",     "isSurroundEnabled", "isMultipleTilt",
                                         "lightingLayers", "assignments",       "indicatorColors",
                                         "settings"};
        if (!keys.contains(key))
            throw std::invalid_argument("Unsupported document field");
        if (json(value).size() > MAX_DOCUMENT_FIELD_BYTES)
            throw std::invalid_argument("Document field exceeds size limit");
        if ((key == "isLinkedEq" || key == "isMultipleTilt" || key == "isSurroundEnabled") &&
            value.metaType().id() != QMetaType::Bool)
            throw std::invalid_argument("Boolean setting requires a boolean value");
        if ((key == "lightingLayers" || key == "assignments" || key == "indicatorColors") &&
            value.metaType().id() != QMetaType::QVariantList)
            throw std::invalid_argument("Document list requires an array");
        if (key == "indicatorColors") {
            const auto colors = value.toList();
            if (colors.size() != 3)
                throw std::invalid_argument("Three indicator colors are required");
            for (const auto& color : colors)
                if (!QColor(color.toString()).isValid())
                    throw std::invalid_argument("Invalid indicator color");
        }
        if (key == "lightingLayers")
            validateLightingLayers(value.toList());
        if (key == "assignments")
            validateAssignments(value.toList());
        persist(key, value);
        if (key == "isSurroundEnabled" && isAudioClientRunning())
            audioUpdateTimer_.start();
    });
    if (key == "lightingLayers" && lastOperation_.value("isOk").toBool())
        requestLiveLighting();
}
void ApplicationModel::validateLightingLayers(const QVariantList& layers) const {
    const int maximum =
        catalog_.definition(selectedModelId_).value("MaxSwLightingLayersCount").toInt(32);
    if (layers.size() > maximum)
        throw std::invalid_argument("Lighting layer count exceeds device capability");
    QSet<QString> zoneIds, typeIds;
    for (const auto& entry : lightingKeys())
        zoneIds.insert(entry.toMap().value("id").toString());
    for (const auto& entry : lightingTypes())
        typeIds.insert(entry.toMap().value("id").toString());
    for (const auto& layer : layers) {
        if (layer.metaType().id() != QMetaType::QVariantMap)
            throw std::invalid_argument("Lighting layer requires an object");
        const auto map = layer.toMap();
        if (!typeIds.contains(map.value("type").toString()))
            throw std::invalid_argument("Lighting type is not supported by this device");
        const auto colors = map.value("colors").toList();
        if (colors.isEmpty() || colors.size() > MAX_LIGHTING_COLORS)
            throw std::invalid_argument("Invalid lighting color count");
        for (const auto& color : colors)
            if (!QColor(color.toString()).isValid())
                throw std::invalid_argument("Invalid lighting color");
        const auto zones = map.value("zones").toStringList();
        QSet<QString> seen;
        for (const auto& zone : zones) {
            if ((zone != "all" && !zoneIds.contains(zone)) || seen.contains(zone) ||
                (zone == "all" && zones.size() != 1))
                throw std::invalid_argument("Invalid or duplicate lighting zone");
            seen.insert(zone);
        }
        const auto validateRange = [&map](const QString& field, double lower, double upper) {
            if (!map.contains(field))
                return;
            bool isNumber = false;
            const double value = map.value(field).toDouble(&isNumber);
            if (!isNumber || !std::isfinite(value) || value < lower || value > upper)
                throw std::invalid_argument(("Invalid lighting " + field).toStdString());
        };
        validateRange("opacity", 0, MAX_LIGHTING_OPACITY);
        validateRange("speed", 0, MAX_LIGHTING_SPEED);
        validateRange("duration", MIN_LIGHTING_DURATION, MAX_LIGHTING_DURATION);
        validateRange("direction", 0, 3);
        if (map.contains("isRandom") && map.value("isRandom").metaType().id() != QMetaType::Bool)
            throw std::invalid_argument("Random lighting mode requires a boolean");
        if (map.contains("transitions"))
            (void)lightingTimelineColor(map, 0);
    }
}
void ApplicationModel::validateAssignments(const QVariantList& assignments) const {
    QSet<QString> inputs, outputs, seen;
    for (const auto& entry : keys())
        inputs.insert(entry.toMap().value("id").toString());
    const auto keyboard = assignmentKeyboard();
    for (const auto& group : {"extendedRows", "mainRows", "navigationRows", "numpadRows"}) {
        for (const auto& row : keyboard.value(group).toList()) {
            for (const auto& key : row.toList()) {
                outputs.insert(key.toMap().value("id").toString());
                outputs.insert(key.toMap()
                                   .value("label")
                                   .toString()); // Accept earlier local label-based profiles.
            }
        }
    }
    const QStringList types{"Keyboard", "Language", "Mouse",   "Keystroke", "Text",
                            "Media",    "Launch",   "Disable", "Profile",   "Macro"};
    for (const auto& value : assignments) {
        if (value.metaType().id() != QMetaType::QVariantMap)
            throw std::invalid_argument("Assignment requires an object");
        const auto assignment = value.toMap();
        const auto input = assignment.value("input").toString();
        if (!inputs.contains(input) || seen.contains(input))
            throw std::invalid_argument("Invalid or duplicate assignment input");
        seen.insert(input);
        const auto type = assignment.value("type").toString();
        if (!types.contains(type))
            throw std::invalid_argument("Unknown assignment category");
        const auto params = assignment.value("params").toMap();
        const auto key = params.value("key").toString();
        if (QStringList{"Keyboard", "Language", "Keystroke"}.contains(type) && !key.isEmpty() &&
            !outputs.contains(key))
            throw std::invalid_argument("Unknown virtual keyboard key");
    }
}
QList<double> ApplicationModel::currentGains(const QString& modelId) const {
    const auto data = modelId.isEmpty()
                          ? document()
                          : workspace_.device(modelId, catalog_.initialDocument(modelId));
    const auto presets = data.value("eqPresets").toList();
    const int index = data.value("activeEq").toInt();
    if (index < 0 || index >= presets.size())
        throw std::runtime_error("Selected EQ preset is unavailable");
    QList<double> gains;
    for (const auto& value : presets.at(index).toMap().value("gains").toList())
        gains.append(value.toDouble());
    validateEq(gains);
    return gains;
}
void ApplicationModel::setEqGain(int index, double gain) {
    perform([&] {
        if (index < 0 || index >= EQ_BAND_COUNT || !std::isfinite(gain))
            throw std::invalid_argument("Invalid EQ band");
        auto data = document();
        auto presets = data.value("eqPresets").toList();
        const int active = data.value("activeEq").toInt();
        if (active < 0 || active >= presets.size())
            throw std::invalid_argument("Unknown EQ preset");
        auto preset = presets.at(active).toMap();
        auto gains = currentGains();
        if (data.value("isLinkedEq").toBool()) {
            const double delta = gain - gains.at(index);
            for (auto& value : gains)
                value = std::clamp(value + delta, -12.0, 12.0);
        } else
            gains[index] = gain;
        validateEq(gains);
        QVariantList values;
        for (const auto value : gains)
            values.append(value);
        preset["gains"] = values;
        presets[active] = preset;
        persist("eqPresets", presets);
        if (isAudioClientRunning() || (hardware_ && selectedModelId_ == "virtuoso_black"))
            audioUpdateTimer_.start();
    });
}
void ApplicationModel::selectEqPreset(int index) {
    perform([&] {
        if (index < 0 || index >= document().value("eqPresets").toList().size())
            throw std::invalid_argument("Unknown EQ preset");
        persist("activeEq", index);
        if (isAudioClientRunning() || (hardware_ && selectedModelId_ == "virtuoso_black"))
            audioUpdateTimer_.start();
    });
}
void ApplicationModel::addEqPreset(const QString& name) {
    perform([&] {
        if (name.trimmed().isEmpty() || name.size() > 128)
            throw std::invalid_argument("Invalid EQ preset name");
        auto presets = document().value("eqPresets").toList();
        if (presets.size() >= 128)
            throw std::invalid_argument("EQ preset limit reached");
        QVariantList gains;
        for (int i = 0; i < EQ_BAND_COUNT; ++i)
            gains.append(0.0);
        presets.append(QVariantMap{{"id", QUuid::createUuid().toString()},
                                   {"name", name.trimmed()},
                                   {"gains", gains},
                                   {"isFactory", false}});
        persistFields({{"eqPresets", presets}, {"activeEq", presets.size() - 1}});
    });
}
void ApplicationModel::removeEqPreset(int index) {
    perform([&] {
        auto presets = document().value("eqPresets").toList();
        if (index < 0 || index >= presets.size() || presets.size() <= 1)
            throw std::invalid_argument("Cannot remove preset");
        presets.removeAt(index);
        persistFields({{"eqPresets", presets}, {"activeEq", 0}});
    });
}
void ApplicationModel::resetEqPreset() {
    perform([&] {
        auto data = document();
        auto presets = data.value("eqPresets").toList();
        const int active = data.value("activeEq").toInt();
        if (active < 0 || active >= presets.size())
            throw std::invalid_argument("Unknown EQ preset");
        auto preset = presets.at(active).toMap();
        QVariantList gains;
        for (int index = 0; index < EQ_BAND_COUNT; ++index)
            gains.append(0.0);
        for (const auto& original : catalog_.factoryEqPresets())
            if (original.toMap().value("id") == preset.value("id"))
                gains = original.toMap().value("gains").toList();
        preset["gains"] = gains;
        presets[active] = preset;
        persist("eqPresets", presets);
        if (isAudioClientRunning() || (hardware_ && selectedModelId_ == "virtuoso_black"))
            audioUpdateTimer_.start();
    });
}
void ApplicationModel::addDpiPreset(const QString& name) {
    perform([&] {
        if (name.trimmed().isEmpty() || name.size() > 128)
            throw std::invalid_argument("Invalid DPI preset name");
        auto presets = document().value("dpiPresets").toList();
        if (presets.size() >= 128)
            throw std::invalid_argument("DPI preset limit reached");
        const auto stages = catalog_.initialDocument(selectedModelId_).value("dpiStages").toList();
        if (stages.isEmpty())
            throw std::invalid_argument("Device has no DPI settings");
        presets.append(QVariantMap{{"name", name.trimmed()}, {"stages", stages}});
        persistFields({{"dpiPresets", presets},
                       {"activeDpiPreset", presets.size() - 1},
                       {"dpiStages", stages},
                       {"activeDpi", 2}});
    });
}
void ApplicationModel::selectDpiPreset(int index) {
    perform([&] {
        const auto presets = document().value("dpiPresets").toList();
        if (index < 0 || index >= presets.size())
            throw std::invalid_argument("Unknown DPI preset");
        persistFields(
            {{"activeDpiPreset", index}, {"dpiStages", presets.at(index).toMap().value("stages")}});
    });
}
void ApplicationModel::removeDpiPreset(int index) {
    perform([&] {
        auto presets = document().value("dpiPresets").toList();
        if (index < 0 || index >= presets.size() || presets.size() <= 1)
            throw std::invalid_argument("Cannot remove DPI preset");
        presets.removeAt(index);
        persistFields({{"dpiPresets", presets},
                       {"activeDpiPreset", 0},
                       {"dpiStages", presets.first().toMap().value("stages")}});
    });
}
void ApplicationModel::resetDpiPreset() {
    perform([&] {
        auto presets = document().value("dpiPresets").toList();
        const int index = document().value("activeDpiPreset").toInt();
        if (index < 0 || index >= presets.size())
            throw std::invalid_argument("Unknown DPI preset");
        const auto stages = catalog_.initialDocument(selectedModelId_).value("dpiStages").toList();
        auto preset = presets.at(index).toMap();
        preset["stages"] = stages;
        presets[index] = preset;
        persistFields({{"dpiPresets", presets}, {"dpiStages", stages}, {"activeDpi", 2}});
    });
}
void ApplicationModel::selectDpiStage(int index) {
    perform([&] {
        const auto stages = document().value("dpiStages").toList();
        if (index < 0 || index >= stages.size() - 1 ||
            !stages.at(index).toMap().value("enabled").toBool())
            throw std::invalid_argument("DPI stage is disabled or unavailable");
        persist("activeDpi", index);
    });
}
void ApplicationModel::setDpiStage(int index, const QString& field, const QVariant& value) {
    perform([&] {
        auto stages = document().value("dpiStages").toList();
        if (index < 0 || index >= stages.size())
            throw std::invalid_argument("Unknown DPI stage");
        auto stage = stages.at(index).toMap();
        if (field == "dpiX" || field == "dpiY") {
            const auto limits = catalog_.definition(selectedModelId_);
            const int dpi = value.toInt();
            if (dpi < limits.value("MinDpi").toInt() || dpi > limits.value("MaxDpi").toInt())
                throw std::invalid_argument("DPI is outside device range");
            stage[field] = dpi;
            if (!stage.value("independentXY").toBool()) {
                stage["dpiX"] = dpi;
                stage["dpiY"] = dpi;
            }
        } else if (field == "enabled" || field == "independentXY") {
            if (field == "independentXY" && value.toBool() &&
                !device().value("hasIndependentDpiAxes").toBool())
                throw std::invalid_argument("This device uses one DPI value for both axes");
            stage[field] = value.toBool();
            if (field == "independentXY" && !value.toBool())
                stage["dpiY"] = stage.value("dpiX");
        } else if (field == "color" && QColor(value.toString()).isValid())
            stage[field] = value.toString();
        else
            throw std::invalid_argument("Unknown DPI field");
        stages[index] = stage;
        auto presets = document().value("dpiPresets").toList();
        const int activePreset = document().value("activeDpiPreset").toInt();
        if (activePreset < 0 || activePreset >= presets.size())
            throw std::invalid_argument("Unknown DPI preset");
        auto preset = presets.at(activePreset).toMap();
        preset["stages"] = stages;
        presets[activePreset] = preset;
        persistFields({{"dpiStages", stages}, {"dpiPresets", presets}});
    });
}
void ApplicationModel::setTiltAngle(int index, int angle) {
    perform([&] {
        if (index < 0 || index >= 4 || angle < 10 || angle > 80 || angle % 10 != 0)
            throw std::invalid_argument("Tilt angles use 10 degree steps between 10 and 80");
        auto angles = document().value("tiltAngles").toList();
        if (angles.size() != 4)
            throw std::invalid_argument("Invalid saved tilt angles");
        angles[index] = angle;
        persist("tiltAngles", angles);
    });
}
void ApplicationModel::setMixerControl(const QString& control, int percent, bool isMuted) {
    perform([&] {
        const auto value = setHeadsetControl(selectedUsbPath(), control, percent, isMuted);
        mixer_[control] = value;
    });
}
void ApplicationModel::startAudio(const QString& target) {
    startAudioForDevice(target, selectedModelId_);
}
void ApplicationModel::playSurroundPrompt(bool isEnabled) {
    const auto target = headsetAudioTarget();
    if (target.isEmpty())
        return;
    QFile source(isEnabled ? ":/vendor/voice/SurroundOn.pcm" : ":/vendor/voice/SurroundOff.pcm");
    if (!source.open(QIODevice::ReadOnly)) {
        setNotice("Originale Sprachansage ist nicht verfügbar.");
        return;
    }
    if (voicePrompt_.state() != QProcess::NotRunning) {
        voicePrompt_.terminate();
        voicePrompt_.waitForFinished(PROCESS_TIMEOUT_MS);
    }
    voicePromptState_ = {{"isSurroundEnabled", isEnabled},
                         {"isOriginal", true},
                         {"target", target},
                         {"sampleRate", VOICE_PROMPT_RATE},
                         {"channels", 2},
                         {"format", "s16"},
                         {"isFinished", false},
                         {"isOk", false}};
    voicePrompt_.start("pw-play",
                       {"--raw", "--format", "s16", "--rate", QString::number(VOICE_PROMPT_RATE),
                        "--channels", "2", "--target", target, "--media-role", "Notification",
                        "--properties", "{\"node.dont-fallback\":true}", "-"});
    if (!voicePrompt_.waitForStarted(PROCESS_TIMEOUT_MS)) {
        setNotice("Sprachansage konnte nicht gestartet werden.");
        return;
    }
    voicePrompt_.write(source.readAll());
    voicePrompt_.closeWriteChannel();
    voicePromptState_["hasStarted"] = true;
}
void ApplicationModel::toggleSurround() {
    if (selectedModelId_ != "virtuoso_black")
        return;
    const bool wasEnabled = document().value("isSurroundEnabled").toBool();
    const auto target = headsetAudioTarget();
    // Model-only sessions still exercise the visible state control without touching host audio.
    if (!hardware_ && target.isEmpty()) {
        setDocumentValue("isSurroundEnabled", !wasEnabled);
        return;
    }
    if (target.isEmpty()) {
        perform([] { throw std::runtime_error("Selected Virtuoso audio output is unavailable"); });
        return;
    }
    perform([&] { persist("isSurroundEnabled", !wasEnabled); });
    if (!lastOperation_.value("isOk").toBool())
        return;
    startAudioForDevice(target, selectedModelId_);
    if (!lastOperation_.value("isOk").toBool()) {
        persist("isSurroundEnabled", wasEnabled);
        emit stateChanged();
        return;
    }
    playSurroundPrompt(!wasEnabled);
}
void ApplicationModel::startAudioForDevice(const QString& target, const QString& modelId) {
    perform(
        [&] {
            bool isPresent = false;
            for (const auto& item : audioTargets_)
                if (item.toMap().value("id").toString() == target)
                    isPresent = true;
            if (!isPresent)
                throw std::invalid_argument("Selected audio output is no longer available");
            if (hardware_) {
                bool isSelectedHeadset = false;
                for (const auto& entry : usbDevices_)
                    if (modelId == "virtuoso_black" &&
                        QStringList{"virtuoso_black", "virtuoso_black_dongle"}.contains(
                            entry.toMap().value("modelId").toString()))
                        for (const auto& sink : headsetAudioTargetsForUsb(
                                 audioTargets_, entry.toMap().value("sysPath").toString()))
                            if (sink.toMap().value("id").toString() == target)
                                isSelectedHeadset = true;
                if (!isSelectedHeadset)
                    throw std::invalid_argument("Selected Virtuoso audio output is unavailable");
            }
            const auto data = workspace_.device(modelId, catalog_.initialDocument(modelId));
            const auto hrir = data.value("isSurroundEnabled").toBool()
                                  ? ensureKemarHrir(workspace_.directory())
                                  : QString();
            const auto config = equalizerConfig(currentGains(modelId), target, hrir);
            const auto path = QDir(workspace_.directory()).filePath("equalizer.conf");
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(config) != config.size() ||
                !file.commit())
                throw std::runtime_error("Cannot write EQ configuration");
            stopAudio();
            audioTarget_ = target;
            audioModelId_ = modelId;
            filter_.setProgram("pipewire");
            filter_.setArguments({"-c", path});
            filter_.start();
            if (!filter_.waitForStarted(PROCESS_TIMEOUT_MS))
                throw std::runtime_error("Cannot start PipeWire filter client");
            if (hardware_) {
                QString usbPath;
                for (const auto& sink : audioTargets_)
                    if (sink.toMap().value("id").toString() == target)
                        usbPath = sink.toMap().value("usbSysPath").toString();
                try {
                    audioRoute_.activate(usbPath, target, filter_.processId());
                } catch (...) {
                    stopAudio();
                    throw;
                }
            }
        },
        "Audio auf Virtuoso aktiviert.");
}
void ApplicationModel::stopAudio() {
    audioUpdateTimer_.stop();
    try {
        audioRoute_.deactivate();
    } catch (const std::exception& error) {
        setNotice(QString::fromUtf8(error.what()));
    }
    if (filter_.state() == QProcess::NotRunning)
        return;
    filter_.terminate();
    if (!filter_.waitForFinished(PROCESS_TIMEOUT_MS)) {
        filter_.kill();
        filter_.waitForFinished(PROCESS_TIMEOUT_MS);
    }
    emit stateChanged();
}
void ApplicationModel::applyHardware() {
    if (!hardware_) {
        perform([] { throw std::runtime_error("Hardware access is disabled in this session"); });
        return;
    }
    if (document().value("lightingLayers").toList().isEmpty()) {
        perform([] { throw std::runtime_error("Add a lighting layer before applying it"); });
        return;
    }
    isLightingEditorActive_ = true;
    requestLiveLighting();
}
void ApplicationModel::applyDpiSettings() {
    quint32 x = 0, y = 0;
    QString model;
    perform([&] {
        if (!canApplyDpi())
            throw std::runtime_error("Connect Darkstar with its USB cable to apply DPI");
        if (isHardwareBusy())
            throw std::runtime_error("Hardware operation unavailable or busy");
        const auto current = document();
        const auto stages = current.value("dpiStages").toList();
        const int active = current.value("activeDpi").toInt();
        if (active < 0 || active >= stages.size())
            throw std::invalid_argument("Unknown DPI stage");
        const auto stage = stages.at(active).toMap();
        if (!stage.value("enabled").toBool())
            throw std::invalid_argument("Enable the selected DPI stage before applying it");
        model = selectedModelId_;
        x = stage.value("dpiX").toUInt();
        y = stage.value("dpiY").toUInt();
    });
    if (!lastOperation_.value("isOk").toBool())
        return;
    const auto backend = hardware_;
    scheduleHardware("dpi", [=] { return backend->applyDpi(model, x, y); });
}
void ApplicationModel::refresh() {
    perform([&] {
        usbDevices_ = discoverUsb(catalog_);
        audioTargets_ = queryAudioTargets();
        mixer_.clear();
        if (device().value("kind") == "Headset" && !selectedUsbPath().isEmpty()) {
            try {
                mixer_ = readHeadsetMixer(selectedUsbPath());
            } catch (const std::exception& error) {
                mixer_["error"] = QString::fromUtf8(error.what());
            }
        }
    });
    if (!lightingModel_.isEmpty() && !device().value("isConnected").toBool())
        requestLiveLighting();
}
QString ApplicationModel::GetState() const {
    return QString::fromUtf8(json(
        QVariantMap{{"devices", devices()},
                    {"resourceMode", isUsingLocalCorsairResources() ? "local-corsair" : "open"},
                    {"usbInventory", usbDevices_},
                    {"isPairingImplemented", isPairingImplemented()},
                    {"hardware", hardwareState_},
                    {"isHardwareBusy", isHardwareBusy()},
                    {"isHardwareLightingActive", isHardwareLightingActive()},
                    {"isLightingEditorActive", isLightingEditorActive_},
                    {"lightingModelId", lightingModel_},
                    {"lightingRevision", QVariant::fromValue(lightingRevision_)},
                    {"lightingState", lightingState_},
                    {"scopedUsbInventory", usbInventory()},
                    {"canApplyDpi", canApplyDpi()},
                    {"dpiState", dpiState_},
                    {"workspace", workspace_.snapshot()},
                    {"selectedModelId", selectedModelId_},
                    {"document", document()},
                    {"mixer", mixer_},
                    {"isAudioClientRunning", isAudioClientRunning()},
                    {"audioClientPid", filter_.processId()},
                    {"audioModelId", audioModelId_},
                    {"audioTarget", audioTarget_},
                    {"audioTargets", audioTargets()},
                    {"isAudioRouteActive", audioRoute_.isActive()},
                    {"voicePrompt", voicePromptState_},
                    {"notice", notice()},
                    {"lastOperation", lastOperation_}}));
}
QString ApplicationModel::ListDevices() const {
    return QString::fromUtf8(json(devices()));
}
bool ApplicationModel::exportEqualizer(const QString& path, const QString& target) {
    bool isSaved = false;
    perform([&] {
        const auto hrir = document().value("isSurroundEnabled").toBool()
                              ? ensureKemarHrir(workspace_.directory())
                              : QString();
        const auto config = equalizerConfig(currentGains(), target, hrir);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(config) != config.size() ||
            !file.commit())
            throw std::runtime_error("Cannot export EQ configuration");
        isSaved = true;
    });
    return isSaved;
}
} // namespace open_icue
