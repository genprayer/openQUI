#include "hardware_backend.h"
#include "catalog.h"
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <unistd.h>

namespace open_icue {
namespace {
constexpr auto PAIRING_JOURNAL_NAME = "pairing-1bdc.json";
constexpr auto OWNER_PERMISSIONS = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
constexpr int K70_LED_COUNT = 123;
constexpr quint8 BRAGI_ACTIVE_DPI = 0x20;
constexpr quint8 BRAGI_ACTIVE_DPI_INDEX = 0x1e;
constexpr int MAX_LIGHTING_TRANSITIONS = 256;

std::optional<QColor> validatedTimelineColor(const QVariantMap& layer, double seconds) {
    if (!layer.contains("transitions"))
        return std::nullopt;
    if (!layer.value("type").toString().startsWith("GradientLightingTypeData"))
        throw std::invalid_argument("A lighting timeline requires a gradient effect");
    bool isDurationValid = false;
    const auto duration = layer.value("duration").toDouble(&isDurationValid);
    const auto transitions = layer.value("transitions").toList();
    if (!isDurationValid || !std::isfinite(duration) || duration <= 0 || !std::isfinite(seconds) ||
        seconds < 0 || transitions.size() < 2 || transitions.size() > MAX_LIGHTING_TRANSITIONS ||
        layer.value("isLooping").metaType().id() != QMetaType::Bool ||
        !layer.value("isLooping").toBool())
        throw std::invalid_argument("Invalid continuous lighting timeline");
    QList<double> times;
    QList<QColor> colors;
    for (const auto& transition : transitions) {
        const auto point = transition.toMap();
        bool isTimeValid = false;
        const auto time = point.value("time").toDouble(&isTimeValid);
        const QColor color(point.value("color").toString());
        if (!isTimeValid || !std::isfinite(time) || time < 0 || time > 1 ||
            (!times.isEmpty() && time <= times.last()) || !color.isValid())
            throw std::invalid_argument("Invalid lighting transition");
        times.append(time);
        colors.append(color);
    }
    if (times.first() != 0 || times.last() != 1)
        throw std::invalid_argument("Lighting transitions must span the complete timeline");
    const double phase = std::fmod(seconds / duration, 1.0);
    for (qsizetype index = 1; index < times.size(); ++index) {
        if (phase > times[index])
            continue;
        const double blend = (phase - times[index - 1]) / (times[index] - times[index - 1]);
        const auto& first = colors[index - 1];
        const auto& last = colors[index];
        return QColor::fromRgbF(first.redF() * (1 - blend) + last.redF() * blend,
                                first.greenF() * (1 - blend) + last.greenF() * blend,
                                first.blueF() * (1 - blend) + last.blueF() * blend,
                                first.alphaF() * (1 - blend) + last.alphaF() * blend);
    }
    throw std::invalid_argument("Lighting phase is outside its timeline");
}
QByteArray randomBytes(int length) {
    QByteArray bytes(length, 0);
    for (auto& value : bytes)
        value = char(QRandomGenerator::system()->generate() & 255);
    return bytes;
}
HidEndpoint requireEndpoint(const QList<HidEndpoint>& endpoints, quint16 product) {
    QList<HidEndpoint> matches;
    for (const auto& endpoint : endpoints)
        if (endpoint.product == product)
            matches.append(endpoint);
    if (matches.size() != 1)
        throw std::runtime_error("Connect exactly one supported mouse and one selected receiver");
    return matches.first();
}
void saveJournal(const QString& path, const QJsonObject& journal) {
    QSaveFile file(path);
    const auto bytes = QJsonDocument(journal).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(OWNER_PERMISSIONS) ||
        file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("Cannot save the private pairing recovery journal");
}
QJsonObject loadJournal(const QString& path) {
    const QFileInfo info(path);
    QFile file(path);
    if (info.isSymLink() || info.ownerId() != ::getuid() ||
        (info.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                               QFileDevice::ReadOther | QFileDevice::WriteOther)) ||
        !file.open(QIODevice::ReadOnly) || file.size() > 4096)
        throw std::runtime_error("Pairing journal ownership or permissions are invalid");
    const auto journal = QJsonDocument::fromJson(file.readAll()).object();
    if (journal.value("schemaVersion").toInt() != 1)
        throw std::runtime_error("Pairing journal schema is invalid");
    return journal;
}
void restoreRecordedMode(HidSession& session, const QJsonObject& journal, const QString& field) {
    const int mode = journal.value(field).toInt();
    if (mode != 1 && mode != 2)
        throw std::runtime_error("Pairing recovery control mode is invalid");
    if (session.get(8, 3) == quint32(mode))
        return;
    session.set(8, 3, mode);
    if (session.get(8, 3) != quint32(mode))
        throw std::runtime_error("Firmware mode restoration failed");
}
QVariantMap battery(HidSession& session, quint8 endpoint, bool isV2w) {
    quint32 level = 0, status = 0;
    if (isV2w) {
        const auto response = session.v2w(endpoint, 2, QByteArray(1, char(0x0f)));
        if (response[3] != 0)
            throw std::runtime_error("Headset battery query rejected");
        level = qFromLittleEndian<quint16>(response.constData() + 4);
        const auto charging = session.v2w(endpoint, 2, QByteArray(1, char(0x10)));
        if (charging[3] != 0)
            throw std::runtime_error("Headset charge query rejected");
        status = qFromLittleEndian<quint16>(charging.constData() + 4);
    } else {
        level = session.get(endpoint, 0x0f);
        status = session.get(endpoint, 0x10);
    }
    if (level > 1000 || status < 1 || status > 3)
        throw std::runtime_error("Invalid hardware battery telemetry");
    return {{"batteryPercent", int(level / 10)},
            {"batteryStatus", status},
            {"isCharging", status == 1},
            {"hasBattery", true},
            {"batteryReadAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
}
} // namespace

std::optional<QColor> lightingTimelineColor(const QVariantMap& layer, double seconds) {
    return validatedTimelineColor(layer, seconds);
}

HardwareBackend::HardwareBackend(QString stateDirectory, EndpointDiscovery discovery,
                                 SessionFactory sessions,
                                 std::shared_ptr<KeyboardLightingInput> keyboardInput)
    : stateDirectory_(std::move(stateDirectory)), discovery_(std::move(discovery)),
      keyboardInput_(keyboardInput ? std::move(keyboardInput)
                     : sessions    ? nullptr
                                   : std::make_shared<KeyboardLightingInput>()),
      sessions_(sessions ? std::move(sessions) : [](const HidEndpoint& endpoint) {
          return std::make_unique<HidSession>(endpoint);
      }) {}
HardwareBackend::~HardwareBackend() = default;
QString HardwareBackend::restoreInterruptedPairing() {
    const auto endpoints = discovery_();
    recoveryError_.clear();
    try {
        const auto path = stateDirectory_ + "/hardware/" + PAIRING_JOURNAL_NAME;
        if (QFile::exists(path)) {
            const auto journal = loadJournal(path);
            if (journal.value("phase").toString() != "credentials-written") {
                QLockFile lock(stateDirectory_ + "/hardware/pairing.lock");
                if (!lock.tryLock(0))
                    throw std::runtime_error("Another pairing operation is running");
                for (const auto& endpoint : endpoints) {
                    const bool isMouse = endpoint.product == DARKSTAR_PRODUCT_ID;
                    if (!isMouse && endpoint.product != SLIPSTREAM_PRODUCT_ID)
                        continue;
                    if (lightingSessions_.contains(endpoint.path))
                        continue;
                    const auto serialField = isMouse ? "mouseSerial" : "receiverSerial";
                    if (endpoint.serial != journal.value(serialField).toString())
                        continue;
                    auto session = sessions_(endpoint);
                    if (session->get(8, 0x11) != CORSAIR_VENDOR_ID ||
                        session->get(8, 0x12) != endpoint.product)
                        throw std::runtime_error("Pairing protocol identity check failed");
                    restoreRecordedMode(*session, journal,
                                        isMouse ? "mouseOriginalMode" : "receiverOriginalMode");
                }
            }
        }
    } catch (const std::exception& error) {
        recoveryError_ = QString::fromUtf8(error.what());
    }
    return recoveryError_;
}
QVariantMap HardwareBackend::probe() {
    QVariantList devices;
    const auto endpoints = discovery_();
    bool hasMouse = false, hasReceiver = false;
    int verifiedWiredMice = 0;
    const auto wiredMouseCount =
        std::count_if(endpoints.begin(), endpoints.end(),
                      [](const auto& endpoint) { return endpoint.product == DARKSTAR_PRODUCT_ID; });
    for (const auto& endpoint : endpoints) {
        QVariantMap item{{"path", endpoint.path},
                         {"product", endpoint.product},
                         {"serial", endpoint.serial},
                         {"hasBattery", false},
                         {"isVerified", false}};
        try {
            std::unique_ptr<HidSession> temporary;
            auto active = lightingSessions_.find(endpoint.path);
            if (active == lightingSessions_.end())
                temporary = sessions_(endpoint);
            auto& session = temporary ? *temporary : *active->second;
            if (endpoint.isV2w) {
                const auto firmware = session.v2w(8, 2, QByteArray(1, char(0x13)));
                if (firmware[3] != 0)
                    throw std::runtime_error("Receiver firmware query rejected");
                item["modelId"] = "virtuoso_black";
                item.insert(battery(session, 9, true));
                item["isWireless"] = true;
            } else {
                if (session.get(8, 0x11) != CORSAIR_VENDOR_ID ||
                    session.get(8, 0x12) != endpoint.product)
                    throw std::runtime_error("USB and protocol identities disagree");
                item["mode"] = session.get(8, 3);
                if (endpoint.product == DARKSTAR_PRODUCT_ID) {
                    item["modelId"] = "darkstar_wireless";
                    item.insert(battery(session, 8, false));
                    hasMouse = true;
                    try {
                        const auto dpi = session.get(8, BRAGI_ACTIVE_DPI);
                        const auto index = session.get(8, BRAGI_ACTIVE_DPI_INDEX);
                        const auto definition = Catalog().definition("darkstar_wireless");
                        if (dpi < quint32(definition.value("MinDpi").toInt()) ||
                            dpi > quint32(definition.value("MaxDpi").toInt()))
                            throw std::runtime_error(
                                "Firmware returned an invalid active DPI value");
                        item["activeDpiValue"] = dpi;
                        item["activeDpiIndex"] = index;
                        item["canApplyDpi"] = true;
                    } catch (const std::exception& error) {
                        item["canApplyDpi"] = false;
                        item["dpiError"] = QString::fromUtf8(error.what());
                    }
                } else if (endpoint.product == SLIPSTREAM_PRODUCT_ID) {
                    item["modelId"] = "slipstream_wireless_dongle_ss";
                    const auto children = session.get(8, 0x36);
                    item["radioSlots"] = children;
                    item["isRadioConnected"] = false;
                    if (children & 2) {
                        item["wirelessProduct"] = session.get(9, 0x12);
                        item["isRadioConnected"] =
                            item.value("wirelessProduct").toUInt() == DARKSTAR_PRODUCT_ID;
                        if (item.value("isRadioConnected").toBool()) {
                            auto remote = battery(session, 9, false);
                            remote["modelId"] = "darkstar_wireless";
                            remote["isWireless"] = true;
                            remote["isVerified"] = true;
                            devices.append(remote);
                        }
                    }
                    hasReceiver = true;
                } else
                    item["modelId"] = "k70_core_rgb";
            }
            item["isVerified"] = true;
            if (endpoint.product == DARKSTAR_PRODUCT_ID && item.value("canApplyDpi").toBool())
                ++verifiedWiredMice;
        } catch (const std::exception& error) {
            item["error"] = QString::fromUtf8(error.what());
        }
        devices.append(item);
    }
    return {{"devices", devices},
            {"canPairDarkstar", hasMouse && hasReceiver},
            {"canApplyDpi", wiredMouseCount == 1 && verifiedWiredMice == 1},
            {"recoveryError", recoveryError_},
            {"isOk", true},
            {"readAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
}

QVariantMap HardwareBackend::pairDarkstar() {
    // Two guarded sessions and a process lock span the entire ordered transaction.
    const auto released = releaseControl();
    if (!released.value("isOk").toBool())
        throw std::runtime_error(released.value("error").toString().toStdString());
    const auto endpoints = discovery_();
    const auto mouse = requireEndpoint(endpoints, DARKSTAR_PRODUCT_ID);
    const auto receiver = requireEndpoint(endpoints, SLIPSTREAM_PRODUCT_ID);
    if (mouse.serial.isEmpty() || receiver.serial.isEmpty())
        throw std::runtime_error("Pairing requires stable USB serial numbers for recovery");
    const auto directory = stateDirectory_ + "/hardware";
    if (!QDir().mkpath(directory))
        throw std::runtime_error("Cannot create pairing recovery directory");
    QLockFile lock(directory + "/pairing.lock");
    if (!lock.tryLock(0))
        throw std::runtime_error("Another pairing operation is running");
    {
        const auto receiverOwner = sessions_(receiver), mouseOwner = sessions_(mouse);
        auto& receiverSession = *receiverOwner;
        auto& mouseSession = *mouseOwner;
        for (auto* session : {&receiverSession, &mouseSession}) {
            if (session->get(8, 0x11) != CORSAIR_VENDOR_ID ||
                session->get(8, 0x12) != session->endpoint().product)
                throw std::runtime_error("Pairing protocol identity check failed");
        }
        const auto radioSlots = receiverSession.get(8, 0x36);
        if (radioSlots != 0 &&
            (radioSlots != 2 || receiverSession.get(9, 0x11) != CORSAIR_VENDOR_ID ||
             receiverSession.get(9, 0x12) != DARKSTAR_PRODUCT_ID))
            throw std::runtime_error(
                "Selected receiver already has a live radio device; disconnect it before pairing");
        if (radioSlots == 2) {
            receiverSession.enterSoftwareMode();
            mouseSession.enterSoftwareMode();
            if (receiverSession.readResource(5, 8) != mouseSession.readResource(5, 8))
                throw std::runtime_error(
                    "The live radio device belongs to a different wired mouse");
            mouseSession.restoreMode();
            receiverSession.restoreMode();
            return {{"isOk", true},
                    {"isAlreadyPaired", true},
                    {"isCredentialsWritten", true},
                    {"requiresCableRemoval", false}};
        }
        const auto path = directory + '/' + PAIRING_JOURNAL_NAME;
        QJsonObject journal;
        if (QFile::exists(path)) {
            journal = loadJournal(path);
            if (journal.value("mouseSerial").toString() != mouse.serial ||
                journal.value("receiverSerial").toString() != receiver.serial)
                throw std::runtime_error("Recovery journal belongs to different physical devices");
        } else {
            journal = {{"schemaVersion", 1},
                       {"mouseSerial", mouse.serial},
                       {"receiverSerial", receiver.serial},
                       {"pairingId", QString::fromLatin1(randomBytes(8).toHex())},
                       {"encryptionKey", QString::fromLatin1(randomBytes(16).toHex())},
                       {"phase", "prepared"}};
        }
        auto pairingId = QByteArray::fromHex(journal.value("pairingId").toString().toLatin1());
        auto encryptionKey =
            QByteArray::fromHex(journal.value("encryptionKey").toString().toLatin1());
        if (pairingId.size() != 8 || encryptionKey.size() != 16)
            throw std::runtime_error("Pairing recovery material has invalid lengths");
        try {
            if (journal.value("phase").toString() != "credentials-written" &&
                journal.contains("receiverOriginalMode") && journal.contains("mouseOriginalMode")) {
                restoreRecordedMode(receiverSession, journal, "receiverOriginalMode");
                restoreRecordedMode(mouseSession, journal, "mouseOriginalMode");
            }
            journal["receiverOriginalMode"] = int(receiverSession.get(8, 3));
            journal["mouseOriginalMode"] = int(mouseSession.get(8, 3));
            journal["phase"] = "prepared";
            saveJournal(path, journal);
            for (auto* session : {&receiverSession, &mouseSession}) {
                session->enterSoftwareMode();
                // The key MUST immediately follow the ID on each physical device.
                session->writeResource(5, pairingId);
                session->writeResource(6, encryptionKey);
                if (session->readResource(5, 8) != pairingId)
                    throw std::runtime_error("Pairing identifier readback failed");
                journal["phase"] =
                    session == &receiverSession ? "receiver-written" : "both-written";
                saveJournal(path, journal);
            }
            mouseSession.restoreMode();
            receiverSession.restoreMode();
            journal["phase"] = "credentials-written";
            saveJournal(path, journal);
            explicit_bzero(encryptionKey.data(), encryptionKey.size());
            explicit_bzero(pairingId.data(), pairingId.size());
        } catch (...) {
            explicit_bzero(encryptionKey.data(), encryptionKey.size());
            explicit_bzero(pairingId.data(), pairingId.size());
            throw;
        }
    }
    auto result = probe();
    result["isCredentialsWritten"] = true;
    result["requiresCableRemoval"] = true;
    return result;
}

int k70LedIndex(const QString& id) {
    if (id.size() == 1 && id[0] >= 'A' && id[0] <= 'Z')
        return 4 + id[0].unicode() - 'A';
    if (id.size() == 1 && id[0] >= '1' && id[0] <= '9')
        return 30 + id[0].unicode() - '1';
    if (id == "0")
        return 39;
    if (id.startsWith('F')) {
        bool isNumber = false;
        const int number = id.mid(1).toInt(&isNumber);
        if (isNumber && number >= 1 && number <= 12)
            return 57 + number;
    }
    if (id.startsWith("Keypad")) {
        bool isNumber = false;
        const int number = id.mid(6).toInt(&isNumber);
        if (isNumber && number >= 0 && number <= 9)
            return number ? 88 + number : 98;
    }
    static const QMap<QString, int> specials{{"Enter", 40},
                                             {"Escape", 41},
                                             {"Backspace", 42},
                                             {"Tab", 43},
                                             {"Space", 44},
                                             {"MinusAndUnderscore", 45},
                                             {"EqualsAndPlus", 46},
                                             {"BracketLeft", 47},
                                             {"BracketRight", 48},
                                             {"Backslash", 49},
                                             {"NonUsHash", 50},
                                             {"SemicolonAndColon", 51},
                                             {"ApostropheAndDoubleQuote", 52},
                                             {"GraveAccentAndTilde", 53},
                                             {"CommaAndLessThan", 54},
                                             {"PeriodAndBiggerThan", 55},
                                             {"SlashAndQuestionMark", 56},
                                             {"CapsLock", 57},
                                             {"PrintScreen", 70},
                                             {"ScrollLock", 71},
                                             {"PauseBreak", 72},
                                             {"Insert", 73},
                                             {"Home", 74},
                                             {"PageUp", 75},
                                             {"Delete", 76},
                                             {"End", 77},
                                             {"PageDown", 78},
                                             {"RightArrow", 79},
                                             {"LeftArrow", 80},
                                             {"DownArrow", 81},
                                             {"UpArrow", 82},
                                             {"NumLock", 83},
                                             {"KeypadSlash", 84},
                                             {"KeypadAsterisk", 85},
                                             {"KeypadMinus", 86},
                                             {"KeypadPlus", 87},
                                             {"KeypadEnter", 88},
                                             {"KeypadPeriodAndDelete", 99},
                                             {"NonUsBackslash", 100},
                                             {"Application", 101},
                                             {"LeftCtrl", 105},
                                             {"LeftShift", 106},
                                             {"LeftAlt", 107},
                                             {"LeftGui", 108},
                                             {"RightCtrl", 109},
                                             {"RightShift", 110},
                                             {"RightAlt", 111},
                                             {"RightGui", 112},
                                             {"Fn", 122}};
    return specials.value(id, -1);
}

QList<QRgb> lightingFrame(const QVariantList& layers, const QVariantList& zones, double seconds,
                          const QMap<int, double>* presses) {
    QList<QRgb> frame;
    QList<std::optional<QColor>> timelineColors;
    for (const auto& layer : layers)
        timelineColors.append(lightingTimelineColor(layer.toMap(), seconds));
    for (int index = 0; index < zones.size(); ++index) {
        const auto zone = zones[index].toMap();
        QColor color(Qt::black);
        for (qsizetype layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
            const auto layer = layers[layerIndex].toMap();
            const auto selected = layer.value("zones").toStringList();
            if (!selected.contains("all") && !selected.contains(zone.value("id").toString()))
                continue;
            const auto type = layer.value("type").toString().section("LightingTypeData", 0, 0);
            if (!QStringList{"StaticCustom", "Solid", "ColorPulse", "ColorShift", "Rainbow",
                             "RainbowWave", "ColorWave", "Gradient", "Wave", "Visor",
                             "SpiralRainbow"}
                     .contains(type) &&
                !(type == "Type" && presses))
                throw std::runtime_error("This effect needs an input or audio trigger and is not "
                                         "implemented for hardware");
            const auto colors = layer.value("colors").toList();
            const double phase = seconds * (0.25 + layer.value("speed", 1).toDouble() * 0.5) /
                                 std::max(0.1, layer.value("duration", 1).toDouble());
            const int direction = layer.value("direction", 0).toInt();
            double position = double(index) / std::max(qsizetype(1), zones.size());
            if (direction % 2)
                position = 1 - position;
            const bool isRandom =
                layer.value("isRandom").toBool() &&
                QStringList{"ColorPulse", "ColorShift", "ColorWave", "Visor"}.contains(type);
            QColor next(colors.isEmpty() ? "#ffffff" : colors.first().toString());
            if (timelineColors[layerIndex])
                next = *timelineColors[layerIndex];
            else if (isRandom)
                next = QColor::fromHsvF(std::fmod(phase * 0.15 + position, 1.0), 0.9, 1);
            else if (type.startsWith("Rainbow") || type == "SpiralRainbow")
                next = QColor::fromHsvF(std::fmod(phase * 0.25 + position, 1.0), 1, 1);
            else if ((type == "ColorShift" || type == "ColorWave" || type == "Gradient") &&
                     colors.size() > 1) {
                const double blend = (std::sin((phase + (type == "ColorShift" ? 0 : position)) * 2 *
                                               std::numbers::pi) +
                                      1) /
                                     2;
                const double segment = blend * (colors.size() - 1);
                const int lower = std::min(int(segment), int(colors.size()) - 2);
                next = QColor(colors.at(lower).toString());
                const auto last = QColor(colors.at(lower + 1).toString());
                const double weight = segment - lower;
                next = QColor::fromRgbF(next.redF() * (1 - weight) + last.redF() * weight,
                                        next.greenF() * (1 - weight) + last.greenF() * weight,
                                        next.blueF() * (1 - weight) + last.blueF() * weight);
            }
            double opacity = std::clamp(layer.value("opacity", 100).toDouble() / 100.0, 0.0, 1.0) *
                             next.alphaF();
            if (type == "ColorPulse" || type == "Wave" || type == "Visor")
                opacity *= (std::sin((phase + position) * 2 * std::numbers::pi) + 1) / 2;
            if (type == "Type") {
                const auto press = presses->constFind(k70LedIndex(zone.value("id").toString()));
                const double age = press == presses->cend() ? -1 : seconds - press.value();
                const double duration = std::max(0.1, layer.value("duration", 1).toDouble());
                opacity *= age >= 0 ? std::max(0.0, 1 - age / duration) : 0;
                if (age >= 0 && layer.value("isRandom").toBool())
                    next = QColor::fromHsvF(
                        std::fmod(press.value() * 0.61803398875 + press.key() * 0.38196601125, 1.0),
                        1, 1);
            }
            color = QColor::fromRgbF(next.redF() * opacity + color.redF() * (1 - opacity),
                                     next.greenF() * opacity + color.greenF() * (1 - opacity),
                                     next.blueF() * opacity + color.blueF() * (1 - opacity));
        }
        frame.append(color.rgb());
    }
    return frame;
}
HidSession& HardwareBackend::lightingSession(const HidEndpoint& endpoint, quint8 commandEndpoint) {
    auto found = lightingSessions_.find(endpoint.path);
    if (found == lightingSessions_.end()) {
        auto session = sessions_(endpoint);
        if (endpoint.isV2w) {
            const auto firmware = session->v2w(8, 2, QByteArray(1, char(0x13)));
            if (firmware[3] != 0)
                throw std::runtime_error("Headset receiver identity check failed");
        } else {
            if (session->get(8, 0x11) != CORSAIR_VENDOR_ID ||
                session->get(8, 0x12) != endpoint.product)
                throw std::runtime_error("Lighting identity check failed");
            if (commandEndpoint != 8 &&
                (session->get(commandEndpoint, 0x11) != CORSAIR_VENDOR_ID ||
                 session->get(commandEndpoint, 0x12) != DARKSTAR_PRODUCT_ID))
                throw std::runtime_error("Wireless mouse lighting identity check failed");
            session->enterSoftwareMode(commandEndpoint);
        }
        found = lightingSessions_.emplace(endpoint.path, std::move(session)).first;
    }
    return *found->second;
}
QVariantMap HardwareBackend::applyLighting(const QString& model, const QVariantList& layers,
                                           const QVariantList& zones, double seconds) {
    const bool isKeyboard = model == "k70_core_rgb";
    const bool isHeadset = model == "virtuoso_black";
    if (!isKeyboard && !isHeadset && model != "darkstar_wireless")
        throw std::runtime_error("Hardware lighting is not supported for this model");
    // An empty edited layer stack clears every LED instead of retaining the previous frame.
    const bool hasKeyTrigger = std::any_of(layers.cbegin(), layers.cend(), [](const auto& value) {
        return value.toMap().value("type").toString().startsWith("TypeLightingTypeData");
    });
    if (hasKeyTrigger && (!isKeyboard || !keyboardInput_))
        throw std::runtime_error("This effect needs an input or audio trigger and is not "
                                 "implemented for hardware");
    QMap<int, double> presses;
    auto colors = lightingFrame(layers, zones, seconds,
                                hasKeyTrigger && isKeyboard && keyboardInput_ ? &presses : nullptr);
    const auto endpoints = discovery_();
    const bool hasWiredMouse =
        std::any_of(endpoints.begin(), endpoints.end(),
                    [](const auto& endpoint) { return endpoint.product == DARKSTAR_PRODUCT_ID; });
    const bool isWirelessMouse = !isKeyboard && !isHeadset && !hasWiredMouse;
    const auto endpoint = requireEndpoint(endpoints, isKeyboard  ? K70_CORE_PRODUCT_ID
                                                     : isHeadset ? VIRTUOSO_RECEIVER_PRODUCT_ID
                                                     : isWirelessMouse ? SLIPSTREAM_PRODUCT_ID
                                                                       : DARKSTAR_PRODUCT_ID);
    if (hasKeyTrigger) {
        presses = keyboardInput_->poll(endpoint.sysPath, seconds);
        colors = lightingFrame(layers, zones, seconds, &presses);
    } else if (keyboardInput_)
        keyboardInput_->close();
    const int ledCount = isKeyboard ? K70_LED_COUNT : isHeadset ? 3 : 13;
    static const QStringList mouseSlots{
        "MouseFrontLeft",       "MouseFrontRight",      "MouseScrollLed",
        "MouseTopPalmGlow1Led", "MouseTopPalmGlow2Led", "MouseTopPalmGlow3Led",
        "MouseTopPalmGlow4Led", "MouseTopPalmGlow5Led", "MouseTopPalmGlow6Led",
        "MouseLogoLed",         "MouseDpiIndicator1",   "MouseDpiIndicator2",
        "MouseDpiIndicator3"};
    QByteArray data(ledCount * 3, 0);
    for (int i = 0; i < zones.size(); ++i) {
        const auto id = zones[i].toMap().value("id").toString();
        const int slot =
            isKeyboard ? k70LedIndex(id)
            : isHeadset
                ? QStringList{"Led_HeadsetLogo", "Led_HeadsetStatus", "Led_HeadsetMicMute"}.indexOf(
                      id)
                : mouseSlots.indexOf(id);
        if (slot < 0)
            continue;
        const auto rgb = colors[i];
        data[slot] = char(qRed(rgb));
        data[slot + ledCount] = char(qGreen(rgb));
        data[slot + 2 * ledCount] = char(qBlue(rgb));
    }
    const quint8 commandEndpoint = isWirelessMouse ? 9 : 8;
    auto& session = lightingSession(endpoint, commandEndpoint);
    if (isHeadset) {
        // Do not seize the headset's mic-mute button by entering software mode.
        // Try the documented RGB endpoint in firmware mode, and require both ACKs.
        const auto opened = session.v2w(9, 0x0d, QByteArray::fromHex("0001"));
        if (opened[3] != 0)
            throw std::runtime_error("Headset lighting endpoint rejected firmware-mode control");
        QByteArray arguments(5, 0);
        arguments[1] = 9;
        arguments += data;
        try {
            const auto written = session.v2w(9, 6, arguments);
            if (written[3] != 0)
                throw std::runtime_error("Headset rejected the RGB frame");
            const auto closed = session.v2w(9, 5, QByteArray::fromHex("0100"));
            if (closed[3] != 0)
                throw std::runtime_error("Headset lighting handle close failed");
        } catch (...) {
            try {
                const auto closed = session.v2w(9, 5, QByteArray::fromHex("0100"));
                if (closed[3] != 0)
                    qWarning("Headset lighting cleanup rejected");
            } catch (const std::exception& error) {
                qWarning("Headset lighting cleanup: %s", error.what());
            }
            throw;
        }
    } else
        session.writeLighting(data, 1, commandEndpoint);
    QVariantMap previewColors;
    for (int index = 0; index < zones.size(); ++index)
        previewColors.insert(zones[index].toMap().value("id").toString(),
                             QColor(colors[index]).name());
    return {{"isOk", true},
            {"modelId", model},
            {"isLightingActive", true},
            {"hasAcknowledgement", true},
            {"frameBytes", data.size()},
            {"reactiveEventCount",
             QVariant::fromValue(keyboardInput_ ? keyboardInput_->eventCount() : 0)},
            {"previewColors", previewColors}};
}
void HardwareBackend::stopLighting() {
    const auto released = releaseControl();
    if (!released.value("isOk").toBool())
        qWarning("Hardware control release: %s", qPrintable(released.value("error").toString()));
}
QVariantMap HardwareBackend::releaseControl() {
    if (keyboardInput_)
        keyboardInput_->close();
    QVariantList errors;
    int restoredModes = 0;
    const int releasedSessions = int(lightingSessions_.size());
    for (const auto& [path, session] : lightingSessions_) {
        try {
            session->restoreMode();
            if (!session->endpoint().isV2w)
                ++restoredModes;
        } catch (const std::exception& error) {
            errors.append(QVariantMap{{"path", path}, {"error", QString::fromUtf8(error.what())}});
        }
    }
    lightingSessions_.clear();
    const bool isOk = errors.isEmpty();
    return {{"isOk", isOk},
            {"hasModeReadback", isOk && restoredModes > 0},
            {"releasedSessions", releasedSessions},
            {"restoredModes", restoredModes},
            {"errors", errors},
            {"error", isOk ? QString() : QString("Hardware control release failed")}};
}

QVariantMap HardwareBackend::applyDpi(const QString& model, quint32 x, quint32 y) {
    if (model != "darkstar_wireless")
        throw std::invalid_argument("Hardware DPI is not supported for this model");
    const auto definition = Catalog().definition(model);
    const auto minDpi = quint32(definition.value("MinDpi").toInt());
    const auto maxDpi = quint32(definition.value("MaxDpi").toInt());
    if (minDpi == 0 || maxDpi < minDpi || maxDpi > std::numeric_limits<quint16>::max() ||
        x < minDpi || x > maxDpi || y < minDpi || y > maxDpi)
        throw std::invalid_argument("DPI is outside device range");
    // This model advertises no independent axes. Its original driver uses property 0x20.
    if (x != y)
        throw std::invalid_argument("This device requires equal X and Y DPI");

    const auto endpoints = discovery_();
    const bool hasWiredMouse =
        std::any_of(endpoints.begin(), endpoints.end(),
                    [](const auto& endpoint) { return endpoint.product == DARKSTAR_PRODUCT_ID; });
    const auto endpoint =
        requireEndpoint(endpoints, hasWiredMouse ? DARKSTAR_PRODUCT_ID : SLIPSTREAM_PRODUCT_ID);
    const quint8 commandEndpoint = hasWiredMouse ? 8 : 9;
    auto existing = lightingSessions_.find(endpoint.path);
    std::unique_ptr<HidSession> temporary;
    if (existing == lightingSessions_.end())
        temporary = sessions_(endpoint);
    auto& session = temporary ? *temporary : *existing->second;
    if (session.get(8, 0x11) != CORSAIR_VENDOR_ID || session.get(8, 0x12) != endpoint.product)
        throw std::runtime_error("DPI protocol identity check failed");
    if (!hasWiredMouse &&
        (!(session.get(8, 0x36) & 2) || session.get(9, 0x11) != CORSAIR_VENDOR_ID ||
         session.get(9, 0x12) != DARKSTAR_PRODUCT_ID))
        throw std::runtime_error("Selected receiver has no connected Darkstar mouse");

    // Unsupported properties and unusable previous values fail before any SET command.
    const auto previousDpi = session.get(commandEndpoint, BRAGI_ACTIVE_DPI);
    const auto previousIndex = session.get(commandEndpoint, BRAGI_ACTIVE_DPI_INDEX);
    if (previousDpi < minDpi || previousDpi > maxDpi)
        throw std::runtime_error("Firmware returned an invalid active DPI value");
    bool hasAttemptedDpiWrite = false;
    try {
        session.enterSoftwareMode(commandEndpoint);
        if (session.get(commandEndpoint, 3) != 2)
            throw std::runtime_error("Firmware did not enter software mode for DPI");
        hasAttemptedDpiWrite = true;
        session.set(commandEndpoint, BRAGI_ACTIVE_DPI, quint16(x));
        if (session.get(commandEndpoint, BRAGI_ACTIVE_DPI) != x)
            throw std::runtime_error("Firmware DPI readback does not match the requested value");
        if (session.get(commandEndpoint, BRAGI_ACTIVE_DPI_INDEX) != previousIndex)
            throw std::runtime_error("Firmware changed the active DPI index unexpectedly");
    } catch (const std::exception& error) {
        QString failure = QString::fromUtf8(error.what());
        if (hasAttemptedDpiWrite) {
            try {
                session.set(commandEndpoint, BRAGI_ACTIVE_DPI, quint16(previousDpi));
                if (session.get(commandEndpoint, BRAGI_ACTIVE_DPI) != previousDpi)
                    throw std::runtime_error("Previous DPI readback does not match");
            } catch (const std::exception& rollbackError) {
                failure += "; DPI rollback failed: " + QString::fromUtf8(rollbackError.what());
            }
        }
        if (temporary) {
            try {
                session.restoreMode();
            } catch (const std::exception& restoreError) {
                failure += "; mode restoration failed: " + QString::fromUtf8(restoreError.what());
            }
        }
        throw std::runtime_error(failure.toStdString());
    }
    if (temporary)
        lightingSessions_.emplace(endpoint.path, std::move(temporary));
    return {{"isOk", true},
            {"modelId", model},
            {"path", endpoint.path},
            {"commandEndpoint", commandEndpoint},
            {"dpiX", x},
            {"dpiY", y},
            {"activeDpiIndex", previousIndex},
            {"property", BRAGI_ACTIVE_DPI},
            {"storage", "software"},
            {"isWireless", !hasWiredMouse},
            {"isDpiActive", true},
            {"hasAcknowledgement", true},
            {"hasReadback", true}};
}
} // namespace open_icue
