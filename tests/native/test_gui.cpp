/** Exercise native QML controls through Qt window input and committed profile readback. */
#include "gui/application_model.h"
#include "gui/device_images.h"
#include "gui/device_selection.h"
#include "gui/icon_images.h"
#include "gui/translation_manager.h"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <memory>
#include <stdexcept>

namespace {
constexpr int GUI_TIMEOUT_MS = 2500;
constexpr int MAX_CONTROL_LATENCY_MS = 1000;
constexpr int LIVE_TIMEOUT_MS = 15000;

/** A deterministic protocol peer for the visible DPI button; never opens hidraw. */
struct GuiDpiPeer {
    quint32 mode = 1, dpi = 800;
    quint16 product = open_icue::DARKSTAR_PRODUCT_ID;
    int dpiWrites = 0;
    bool shouldRejectRelease = false;
    QMutex rgbMutex;
    QByteArray rgb;
    QList<QByteArray> completedFrames;
    int rgbSize = 39;
    bool shouldRejectRgb = false;
    QByteArray lightingPayload() {
        QMutexLocker lock(&rgbMutex);
        return rgb;
    }
    bool hasRedFrame(int slot, int lowerBound) {
        QMutexLocker lock(&rgbMutex);
        return std::any_of(completedFrames.cbegin(), completedFrames.cend(),
                           [slot, lowerBound](const auto& frame) {
                               return frame.size() == 369 && quint8(frame[slot]) > lowerBound;
                           });
    }
    void recordFrame() {
        if (rgb.size() != rgbSize)
            return;
        completedFrames.append(rgb);
        constexpr qsizetype MAX_RECORDED_FRAMES = 64;
        if (completedFrames.size() > MAX_RECORDED_FRAMES)
            completedFrames.removeFirst();
    }
    QByteArray exchange(const QByteArray& request) {
        if (product == open_icue::VIRTUOSO_RECEIVER_PRODUCT_ID) {
            QByteArray response(64, 0);
            response[0] = 1;
            response[1] = char(quint8(request[1]) & 7);
            response[2] = request[2];
            const auto command = quint8(request[2]);
            if (command == 2) {
                const auto property = quint8(request[3]);
                qToLittleEndian<quint16>(property == 0x0f ? 950 : 2, response.data() + 4);
            } else if (command == 6) {
                QMutexLocker lock(&rgbMutex);
                rgb = request.mid(8, 9);
            } else if (command != 0x0d && command != 5)
                throw std::runtime_error("Unexpected GUI headset command");
            return response;
        }
        QByteArray response(64, 0);
        response[1] = request[2];
        const auto command = quint8(request[2]), property = quint8(request[3]);
        quint32 value = 0;
        if (command == 2) {
            switch (property) {
            case 0x11:
                value = open_icue::CORSAIR_VENDOR_ID;
                break;
            case 0x12:
                value = product;
                break;
            case 3:
                value = mode;
                break;
            case 0x20:
                value = dpi;
                break;
            case 0x1e:
                value = 2;
                break;
            case 0x0f:
                value = 1000;
                break;
            case 0x10:
                value = 2;
                break;
            default:
                throw std::runtime_error("Unexpected GUI DPI query");
            }
            qToLittleEndian(value, response.data() + 3);
        } else if (command == 1) {
            value = qFromLittleEndian<quint32>(request.constData() + 5);
            if (property == 3) {
                if (shouldRejectRelease && value == 1)
                    response[2] = 5;
                else
                    mode = value;
            } else if (property == 0x20) {
                dpi = value;
                ++dpiWrites;
            } else
                throw std::runtime_error("Unexpected GUI DPI write");
        } else if (command == 6) {
            QTest::qSleep(30);
            QMutexLocker lock(&rgbMutex);
            if (shouldRejectRgb)
                response[2] = 5;
            else {
                rgbSize = int(qFromLittleEndian<quint32>(request.constData() + 4));
                rgb = request.mid(8, std::min(57, rgbSize));
                recordFrame();
            }
        } else if (command == 7) {
            QMutexLocker lock(&rgbMutex);
            rgb += request.mid(4, std::min(61, rgbSize - int(rgb.size())));
            recordFrame();
        } else if (command != 0x0d && command != 5)
            throw std::runtime_error("Unexpected GUI DPI command");
        return response;
    }
};

QQuickItem* findItem(QQuickItem* parent, const QString& name) {
    if (parent->objectName() == name)
        return parent;
    for (auto* child : parent->childItems())
        if (auto* found = findItem(child, name))
            return found;
    return nullptr;
}
} // namespace

class NativeGuiTest : public QObject {
    Q_OBJECT
  private:
    std::unique_ptr<QTemporaryDir> state_;
    std::unique_ptr<open_icue::ApplicationModel> model_;
    std::unique_ptr<open_icue::TranslationManager> translations_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QQuickWindow* window_ = nullptr;
    QJsonArray events_;
    QString artifacts_;
    bool open(const QString& device, const QString& panel) {
        model_->selectDevice(device);
        engine_ = std::make_unique<QQmlApplicationEngine>();
        engine_->addImageProvider("devices", new open_icue::DeviceImages);
        engine_->addImageProvider("icons", new open_icue::IconImages);
        engine_->rootContext()->setContextProperty("icue", model_.get());
        engine_->rootContext()->setContextProperty("i18n", translations_.get());
        connect(translations_.get(), &open_icue::TranslationManager::languageChanged, engine_.get(),
                &QQmlApplicationEngine::retranslate);
        engine_->rootContext()->setContextProperty("initialPanel", panel);
        engine_->load(QUrl("qrc:/qml/Main.qml"));
        if (engine_->rootObjects().isEmpty())
            return false;
        window_ = qobject_cast<QQuickWindow*>(engine_->rootObjects().first());
        return window_ && QTest::qWaitForWindowExposed(window_, GUI_TIMEOUT_MS);
    }
    QQuickItem* item(const QString& name) const {
        return findItem(window_->contentItem(), name);
    }
    bool click(const QString& name, QPointF local = {},
               Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        // Model replacement can rebuild delegates before Row positioning is polished.
        // Render the actual window before deriving coordinates from its current controls.
        if (window_->grabWindow().isNull())
            return false;
        auto* target = item(name);
        if (!target || !target->isVisible() || !target->isEnabled())
            return false;
        if (local.isNull())
            local = QPointF(target->width() / 2, target->height() / 2);
        const auto point = target->mapToScene(local);
        if (!QRectF(QPointF(), window_->size()).contains(point))
            return false;
        QElapsedTimer clock;
        clock.start();
        const auto inputTimestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        const auto before = QJsonDocument::fromJson(model_->GetState().toUtf8()).object();
        QTest::mouseClick(window_, Qt::LeftButton, modifiers, point.toPoint());
        QCoreApplication::processEvents();
        events_.append(
            QJsonObject{{"action", name},
                        {"dispatch", "QTest::mouseClick(QQuickWindow)"},
                        {"timestampUtc", inputTimestamp},
                        {"modifiers", static_cast<int>(modifiers)},
                        {"windowX", point.x()},
                        {"windowY", point.y()},
                        {"devicePixelRatio", window_->devicePixelRatio()},
                        {"elapsedMs", clock.elapsed()},
                        {"before", before},
                        {"after", QJsonDocument::fromJson(model_->GetState().toUtf8()).object()}});
        return clock.elapsed() < MAX_CONTROL_LATENCY_MS;
    }
    open_icue::DeviceSelection* selection() const {
        return qobject_cast<open_icue::DeviceSelection*>(item("deviceSelection"));
    }
    QStringList zones() const {
        return item("lightingPanel")->property("selectedZoneIds").toStringList();
    }
    bool chooseStaticColor(const QString& color, int index = 0) {
        if (!click("lighting-color-" + QString::number(index)))
            return false;
        if (!QTest::qWaitFor(
                [this] { return item("color-confirm") && item("color-confirm")->isVisible(); },
                GUI_TIMEOUT_MS))
            return false;
        auto* dialog = engine_->rootObjects().first()->findChild<QObject*>("colorEditorPopup");
        if (!dialog)
            return false;
        QSignalSpy closed(dialog, SIGNAL(closed()));
        return click("color-preset-" + color) && click("color-confirm") &&
               QTest::qWaitFor([&closed] { return !closed.isEmpty(); }, GUI_TIMEOUT_MS);
    }
    bool chooseLightingType(const QString& type) {
        auto* list = item("lighting-type-list");
        auto* button = item("lighting-type-" + type);
        if (!list || !button)
            return false;
        constexpr int MAX_SCROLL_ATTEMPTS = 20;
        constexpr int WHEEL_EVENT_INTERVAL_MS = 20;
        for (int attempt = 0; attempt < MAX_SCROLL_ATTEMPTS; ++attempt) {
            const auto center =
                button->mapToItem(list, QPointF(button->width() / 2, button->height() / 2));
            if (center.y() >= 0 && center.y() <= list->height())
                return click("lighting-type-" + type);
            const auto position = list->mapToScene(QPointF(list->width() / 2, list->height() / 2));
            const auto timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            const double before = list->property("contentY").toDouble();
            const int delta = center.y() < 0 ? 120 : -120;
            // Flickable ignores wheel samples with zero elapsed timestamp; mimic distinct user
            // ticks.
            QTest::qWait(WHEEL_EVENT_INTERVAL_MS);
            QTest::wheelEvent(window_, position, QPoint(0, delta));
            if (!QTest::qWaitFor([list] { return !list->property("moving").toBool(); },
                                 GUI_TIMEOUT_MS))
                return false;
            events_.append(QJsonObject{{"action", "lighting effect list wheel"},
                                       {"timestampUtc", timestamp},
                                       {"windowX", position.x()},
                                       {"windowY", position.y()},
                                       {"delta", delta},
                                       {"contentYBefore", before},
                                       {"contentYAfter", list->property("contentY").toDouble()}});
        }
        return false;
    }
    bool dragSelection(QPointF start, QPointF end,
                       Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        auto* surface = selection();
        if (!surface)
            return false;
        const auto first = surface->mapToScene(start), last = surface->mapToScene(end);
        QJsonArray samples;
        QTest::mousePress(window_, Qt::LeftButton, modifiers, first.toPoint());
        for (int index = 1; index <= 8; ++index) {
            const auto point = first + (last - first) * (index / 8.0);
            QTest::mouseMove(window_, point.toPoint(), 10);
            samples.append(QJsonObject{
                {"timestampUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                {"windowX", point.x()},
                {"windowY", point.y()},
                {"isDragging", surface->isDragging()}});
        }
        const bool isFrameCaptured = surface->isDragging() && capture("keyboard-drag-in-progress");
        QTest::mouseRelease(window_, Qt::LeftButton, modifiers, last.toPoint());
        events_.append(
            QJsonObject{{"action", "device rectangle selection"},
                        {"dispatch", "QTest::mousePress; eight mouseMove samples; mouseRelease"},
                        {"modifiers", static_cast<int>(modifiers)},
                        {"samples", samples},
                        {"selectedIds", QJsonArray::fromStringList(surface->selectedIds())}});
        return isFrameCaptured;
    }
    bool capture(const QString& name) {
        QSignalSpy frames(window_, &QQuickWindow::frameSwapped);
        window_->requestUpdate();
        // Some offscreen scenes have no scheduled damage after input; force a frame.
        window_->update();
        if (!QTest::qWaitFor([&frames] { return !frames.isEmpty(); }, GUI_TIMEOUT_MS))
            return false;
        const auto image = window_->grabWindow();
        if (image.isNull() || !image.save(artifacts_ + '/' + name + ".png"))
            return false;
        events_.append(QJsonObject{
            {"capture", name + ".png"},
            {"type", "app-owned QQuickWindow capture"},
            {"frameSwappedCount", frames.size()},
            {"width", image.width()},
            {"height", image.height()},
            {"timestampUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
        return true;
    }
    QVariantList gains() const {
        const auto presets = model_->document().value("eqPresets").toList();
        return presets.at(model_->document().value("activeEq").toInt())
            .toMap()
            .value("gains")
            .toList();
    }
    bool saveState(const QString& name) {
        QSaveFile file(artifacts_ + '/' + name + ".json");
        return file.open(QIODevice::WriteOnly) && file.write(model_->GetState().toUtf8()) > 0 &&
               file.commit();
    }
    bool hasLatestLightingAck(const QString& modelId) const {
        const auto state = QJsonDocument::fromJson(model_->GetState().toUtf8()).object();
        const auto ack = state.value("lightingState").toObject();
        return ack.value("hasAcknowledgement").toBool() && ack.value("isOk").toBool() &&
               ack.value("modelId").toString() == modelId &&
               ack.value("revision") == state.value("lightingRevision");
    }
    QJsonArray pipeWireGraph() const {
        QProcess dump;
        dump.start("pw-dump");
        if (!dump.waitForFinished(LIVE_TIMEOUT_MS) || dump.exitCode() != 0)
            return {};
        return QJsonDocument::fromJson(dump.readAllStandardOutput()).array();
    }
    QString configuredAudioSink() const {
        for (const auto& entry : pipeWireGraph()) {
            const auto object = entry.toObject();
            if (object.value("type") != "PipeWire:Interface:Metadata" ||
                object.value("props").toObject().value("metadata.name") != "default")
                continue;
            for (const auto& row : object.value("metadata").toArray()) {
                const auto value = row.toObject();
                if (value.value("subject").toInt() == 0 &&
                    value.value("key") == "default.configured.audio.sink") {
                    const auto sink = value.value("value");
                    return sink.isObject() ? sink.toObject().value("name").toString()
                                           : QJsonDocument::fromJson(sink.toString().toUtf8())
                                                 .object()
                                                 .value("name")
                                                 .toString();
                }
            }
        }
        return {};
    }
  private slots:
    void initTestCase() {
        if (QFile::exists(":/vendor/fonts/OpenSans-Regular.ttf")) {
            for (const auto& name : {"Regular", "SemiBold", "Bold"})
                QFontDatabase::addApplicationFont(
                    QString(":/vendor/fonts/OpenSans-%1.ttf").arg(name));
            QGuiApplication::setFont(QFont("Open Sans", 10));
        } else
            QGuiApplication::setFont(QFont("sans-serif", 10));
        artifacts_ = qEnvironmentVariable("OPENQUI_GUI_ARTIFACTS");
        if (artifacts_.isEmpty())
            artifacts_ = QDir::tempPath() + "/open-icue-gui-" +
                         QString::number(QCoreApplication::applicationPid());
        QVERIFY(QDir().mkpath(artifacts_));
    }
    void init() {
        state_ = std::make_unique<QTemporaryDir>();
        QVERIFY(state_->isValid());
        model_ = std::make_unique<open_icue::ApplicationModel>(state_->path());
        translations_ = std::make_unique<open_icue::TranslationManager>(state_->path(), "de");
        connect(translations_.get(), &open_icue::TranslationManager::languageChanged, model_.get(),
                &open_icue::ApplicationModel::retranslate);
    }
    void cleanup() {
        engine_.reset();
        window_ = nullptr;
        translations_.reset();
        model_.reset();
        state_.reset();
    }
    void equalizerControls() {
        QVERIFY(open("virtuoso_black", "Equalizer"));
        const auto originalGains =
            model_->document().value("eqPresets").toList().at(4).toMap().value("gains").toList();
        QVERIFY(capture("equalizer-before-input"));
        QVERIFY(click("eq-preset-4"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->document().value("activeEq").toInt(), 4, GUI_TIMEOUT_MS);
        QCOMPARE(gains(), originalGains);
        QVERIFY(click("eq-linked"));
        QTRY_VERIFY_WITH_TIMEOUT(model_->document().value("isLinkedEq").toBool(), GUI_TIMEOUT_MS);
        auto* slider = item("eq-band-0");
        QVERIFY(slider);
        const auto before = gains();
        QVERIFY(click("eq-band-0", QPointF(slider->width() / 2, slider->height() * 0.25)));
        QTRY_VERIFY_WITH_TIMEOUT(gains() != before, GUI_TIMEOUT_MS);
        QCOMPARE(slider->property("value").toDouble(), gains().first().toDouble());
        QVERIFY(capture("equalizer-after-input"));
        QVERIFY(click("eq-reset"));
        QTRY_COMPARE_WITH_TIMEOUT(gains(), originalGains, GUI_TIMEOUT_MS);
    }
    void dpiControls() {
        QVERIFY(open("darkstar_wireless", "Dpi"));
        QVERIFY(capture("dpi-before-input"));
        QVERIFY(click("dpi-select-4"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->document().value("activeDpi").toInt(), 4, GUI_TIMEOUT_MS);
        QVERIFY(click("dpi-x-0"));
        QTest::keyClick(window_, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : {Qt::Key_1, Qt::Key_7, Qt::Key_5, Qt::Key_0})
            QTest::keyClick(window_, key);
        QTest::keyClick(window_, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(
            model_->document().value("dpiStages").toList().first().toMap().value("dpiX").toInt(),
            1750, GUI_TIMEOUT_MS);
        QCOMPARE(
            model_->document().value("dpiStages").toList().first().toMap().value("dpiY").toInt(),
            1750);
        QVERIFY(capture("dpi-after-input"));
        QVERIFY(click("dpi-reset"));
        QTRY_COMPARE_WITH_TIMEOUT(
            model_->document().value("dpiStages").toList().first().toMap().value("dpiX").toInt(),
            400, GUI_TIMEOUT_MS);
    }
    void activeDpiButtonUsesAcknowledgementAndReadback_data() {
        QTest::addColumn<bool>("shouldRejectRelease");
        QTest::newRow("confirmed-release") << false;
        QTest::newRow("failed-release") << true;
    }
    void activeDpiButtonUsesAcknowledgementAndReadback() {
        QFETCH(bool, shouldRejectRelease);
        QVERIFY(open("darkstar_wireless", "Dpi"));
        QVERIFY(item("dpi-apply"));
        QVERIFY(!item("dpi-apply")->isEnabled());
        auto peer = std::make_shared<GuiDpiPeer>();
        const open_icue::HidEndpoint endpoint{
            "fixture-mouse", {}, "fixture-serial", open_icue::DARKSTAR_PRODUCT_ID, {}, false};
        auto backend = std::make_shared<open_icue::HardwareBackend>(
            state_->path(), [endpoint] { return QList{endpoint}; },
            [peer](const open_icue::HidEndpoint& device) {
                return std::make_unique<open_icue::HidSession>(
                    device, [peer](const QByteArray& request) { return peer->exchange(request); });
            });
        model_->enableHardware(backend);
        QTRY_VERIFY_WITH_TIMEOUT(model_->canApplyDpi(), GUI_TIMEOUT_MS);
        QVERIFY(click("dpi-select-0"));
        QVERIFY(click("dpi-x-0"));
        QTest::keyClick(window_, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : {Qt::Key_1, Qt::Key_7, Qt::Key_5, Qt::Key_0})
            QTest::keyClick(window_, key);
        QTest::keyClick(window_, Qt::Key_Return);
        QCOMPARE(peer->dpiWrites, 0);
        QCOMPARE(peer->dpi, 800U);
        QVERIFY(click("dpi-apply"));
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareBusy(), GUI_TIMEOUT_MS);
        QVERIFY(model_->lastOperation().value("isOk").toBool());
        QVERIFY(model_->dpiState().value("hasAcknowledgement").toBool());
        QVERIFY(model_->dpiState().value("hasReadback").toBool());
        QCOMPARE(model_->dpiState().value("dpiX").toUInt(), 1750U);
        QCOMPARE(peer->dpi, 1750U);
        QCOMPARE(peer->mode, 2U);
        QCOMPARE(peer->dpiWrites, 1);
        QVERIFY(capture("dpi-hid-acknowledged"));
        peer->shouldRejectRelease = shouldRejectRelease;
        auto* scroll = item("dpi-settings-scroll");
        QVERIFY(scroll);
        QTest::qWait(60);
        QTest::wheelEvent(window_,
                          scroll->mapToScene(QPointF(scroll->width() - 15, scroll->height() / 2)),
                          QPoint(0, -360));
        QTest::qWait(80);
        QVERIFY(click("dpi-release"));
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareBusy(), GUI_TIMEOUT_MS);
        QCOMPARE(model_->lastOperation().value("isOk").toBool(), !shouldRejectRelease);
        QCOMPARE(model_->lastOperation().value("hasModeReadback").toBool(), !shouldRejectRelease);
        QVERIFY(!model_->dpiState().value("isDpiActive").toBool());
        if (!shouldRejectRelease)
            QCOMPARE(peer->mode, 1U);
        else
            QVERIFY(!model_->notice().isEmpty());
        peer->shouldRejectRelease = false;
        QVERIFY(capture("dpi-control-released"));
    }
    void allGestureDirections() {
        QVERIFY(open("darkstar_wireless", "Gestures"));
        QVERIFY(capture("gestures-before-input"));
        for (int index = 0; index < 4; ++index) {
            QVERIFY(click("tilt-plus-" + QString::number(index)));
            QTRY_COMPARE_WITH_TIMEOUT(
                model_->document().value("tiltAngles").toList().at(index).toInt(), 30,
                GUI_TIMEOUT_MS);
        }
        QVERIFY(click("tilt-multiple"));
        QTRY_VERIFY_WITH_TIMEOUT(model_->document().value("isMultipleTilt").toBool(),
                                 GUI_TIMEOUT_MS);
        QVERIFY(capture("gestures-after-input"));
    }
    void lightingControls() {
        QVERIFY(open("k70_core_rgb", "Lightings"));
        QVERIFY(capture("lighting-before-input"));
        QVERIFY(click("lighting-add"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->document().value("lightingLayers").toList().size(), 1,
                                  GUI_TIMEOUT_MS);
        auto* list = item("lighting-type-list");
        QVERIFY(list);
        QTest::wheelEvent(window_, list->mapToScene(QPointF(list->width() / 2, list->height() / 2)),
                          QPoint(0, -480));
        QTRY_VERIFY_WITH_TIMEOUT(list->property("contentY").toDouble() > 0, GUI_TIMEOUT_MS);
        QTRY_VERIFY_WITH_TIMEOUT(!list->property("moving").toBool(), GUI_TIMEOUT_MS);
        QVERIFY(capture("lighting-after-scroll"));
        QVERIFY(click("lighting-type-StaticCustom"));
        QString staticType;
        for (const auto& type : model_->lightingTypes())
            if (type.toMap().value("key") == "StaticCustom")
                staticType = type.toMap().value("id").toString();
        QVERIFY(!staticType.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(model_->document()
                                      .value("lightingLayers")
                                      .toList()
                                      .first()
                                      .toMap()
                                      .value("type")
                                      .toString(),
                                  staticType, GUI_TIMEOUT_MS);
        QTRY_VERIFY_WITH_TIMEOUT(item("lighting-opacity") && item("lighting-opacity")->isVisible(),
                                 GUI_TIMEOUT_MS);
        auto* slider = item("lighting-opacity");
        QVERIFY(click("lighting-opacity", QPointF(8, slider->height() / 2)));
        QTRY_COMPARE_WITH_TIMEOUT(model_->document()
                                      .value("lightingLayers")
                                      .toList()
                                      .first()
                                      .toMap()
                                      .value("opacity")
                                      .toInt(),
                                  0, GUI_TIMEOUT_MS);
        QCOMPARE(slider->property("value").toInt(), 0);
        QVERIFY(capture("lighting-after-input"));
    }
    void colorPickerStreamsBeforeOkAndCancelRestoresHardware() {
        QVERIFY(open("darkstar_wireless", "Lightings"));
        auto peer = std::make_shared<GuiDpiPeer>();
        const open_icue::HidEndpoint endpoint{"fixture-live-mouse",           {}, "fixture-serial",
                                              open_icue::DARKSTAR_PRODUCT_ID, {}, false};
        auto backend = std::make_shared<open_icue::HardwareBackend>(
            state_->path(), [endpoint] { return QList{endpoint}; },
            [peer](const open_icue::HidEndpoint& device) {
                return std::make_unique<open_icue::HidSession>(
                    device, [peer](const QByteArray& request) { return peer->exchange(request); });
            });
        model_->enableHardware(backend);
        QTRY_VERIFY_WITH_TIMEOUT(model_->device().value("isConnected").toBool(), GUI_TIMEOUT_MS);
        QVERIFY(click("lighting-add"));
        QVERIFY(chooseLightingType("StaticCustom"));
        QTRY_VERIFY_WITH_TIMEOUT(model_->isHardwareLightingActive(), GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload().mid(9, 1), QByteArray(1, char(255)),
                                  GUI_TIMEOUT_MS);
        QVERIFY(click("lighting-color-0"));
        QTRY_VERIFY_WITH_TIMEOUT(item("color-confirm") && item("color-confirm")->isVisible(),
                                 GUI_TIMEOUT_MS);
        QVERIFY(click("color-preset-0000ff"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->document()
                                      .value("lightingLayers")
                                      .toList()
                                      .first()
                                      .toMap()
                                      .value("colors")
                                      .toList()
                                      .first()
                                      .toString(),
                                  QString("#0000ff"), GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload().mid(35, 1), QByteArray(1, char(255)),
                                  GUI_TIMEOUT_MS);
        QCOMPARE(peer->lightingPayload().mid(9, 1), QByteArray(1, char(0)));
        QVERIFY(item("color-confirm")->isVisible());
        QCOMPARE(item("lighting-color-0")->parentItem()->property("color").value<QColor>(),
                 QColor("#0000ff"));
        QVERIFY(capture("color-live-before-ok"));
        QVERIFY(click("color-preset-00ff00"));
        QVERIFY(click("color-preset-ffff00"));
        QVERIFY(click("color-preset-ff00ff"));
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload().mid(35, 1), QByteArray(1, char(255)),
                                  GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload().mid(9, 1), QByteArray(1, char(255)),
                                  GUI_TIMEOUT_MS);
        QCOMPARE(peer->lightingPayload().mid(22, 1), QByteArray(1, char(0)));
        QVERIFY(click("color-cancel"));
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload().mid(9, 1), QByteArray(1, char(255)),
                                  GUI_TIMEOUT_MS);
        QVERIFY(click("lighting-clear-selection"));
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload(), QByteArray(39, 0), GUI_TIMEOUT_MS);
        QVERIFY(click("lighting-select-all"));
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload().mid(9, 1), QByteArray(1, char(255)),
                                  GUI_TIMEOUT_MS);
        QVERIFY(click("navigation-Dpi"));
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareLightingActive(), GUI_TIMEOUT_MS);
        const auto previous = peer->lightingPayload();
        auto layers = model_->document().value("lightingLayers").toList();
        auto layer = layers.first().toMap();
        layer["colors"] = QVariantList{"#00ff00"};
        layers[0] = layer;
        model_->setDocumentValue("lightingLayers", layers);
        QTest::qWait(120);
        QCOMPARE(peer->lightingPayload(), previous);
        QVERIFY(click("navigation-Lightings"));
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload().mid(22, 1), QByteArray(1, char(255)),
                                  GUI_TIMEOUT_MS);
        model_->setDocumentValue("lightingLayers", QVariantList{});
        QTRY_COMPARE_WITH_TIMEOUT(peer->lightingPayload(), QByteArray(39, 0), GUI_TIMEOUT_MS);
        model_->setLightingEditorActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareLightingActive(), GUI_TIMEOUT_MS);
    }
    void rainColorAndOpacityEditsReachTheOutgoingFrame() {
        QVERIFY(open("k70_core_rgb", "Lightings"));
        auto peer = std::make_shared<GuiDpiPeer>();
        peer->product = open_icue::K70_CORE_PRODUCT_ID;
        const open_icue::HidEndpoint endpoint{
            "rain-keyboard", {}, "rain-serial", open_icue::K70_CORE_PRODUCT_ID, {}, false};
        auto backend = std::make_shared<open_icue::HardwareBackend>(
            state_->path(), [endpoint] { return QList{endpoint}; },
            [peer](const auto& device) {
                return std::make_unique<open_icue::HidSession>(
                    device, [peer](const QByteArray& request) { return peer->exchange(request); });
            });
        model_->enableHardware(backend);
        QTRY_VERIFY_WITH_TIMEOUT(model_->device().value("isConnected").toBool(), GUI_TIMEOUT_MS);
        QVERIFY(click("lighting-add"));
        QVERIFY(chooseLightingType("Rain"));
        QVERIFY(click("lighting-fixed-colors"));
        QVERIFY(chooseStaticColor("00ff00"));
        QVERIFY(chooseStaticColor("00ff00", 1));
        QTRY_VERIFY2_WITH_TIMEOUT(model_->isHardwareLightingActive(), qPrintable(model_->notice()),
                                  GUI_TIMEOUT_MS);
        const auto maximum = [peer](int channel) {
            const auto bytes = peer->lightingPayload();
            int result = 0;
            if (bytes.size() != 369)
                return result;
            for (int slot = 0; slot < 123; ++slot)
                result = std::max(result, int(quint8(bytes[slot + channel * 123])));
            return result;
        };
        QTRY_VERIFY_WITH_TIMEOUT(maximum(1) > 100, GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(maximum(0), 0, GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(maximum(2), 0, GUI_TIMEOUT_MS);
        QVERIFY(item("lighting-opacity")->isVisible());
        auto* opacity = item("lighting-opacity");
        QVERIFY(click("lighting-opacity", QPointF(opacity->width() * 0.15, opacity->height() / 2)));
        QTRY_VERIFY_WITH_TIMEOUT(model_->document()
                                         .value("lightingLayers")
                                         .toList()
                                         .first()
                                         .toMap()
                                         .value("opacity")
                                         .toInt() < 30,
                                 GUI_TIMEOUT_MS);
        QTRY_VERIFY_WITH_TIMEOUT(maximum(1) <= 77, GUI_TIMEOUT_MS);
        QVERIFY(capture("rain-visible-color-opacity"));
    }
    void keyboardTriggersUseIncomingFramesInDevicePreview_data() {
        QTest::addColumn<QString>("effect");
        QTest::newRow("per-key-flash") << QString("Type");
        QTest::newRow("ripple-from-key") << QString("Ripple");
    }
    void keyboardTriggersUseIncomingFramesInDevicePreview() {
        QFETCH(QString, effect);
        QVERIFY(open("k70_core_rgb", "Lightings"));
        auto peer = std::make_shared<GuiDpiPeer>();
        peer->product = open_icue::K70_CORE_PRODUCT_ID;
        const open_icue::HidEndpoint endpoint{
            "fixture-keyboard", {}, "fixture-serial", open_icue::K70_CORE_PRODUCT_ID, {}, false};
        struct InputQueue {
            QMutex mutex;
            QList<QByteArray> reports;
        };
        auto queue = std::make_shared<InputQueue>();
        auto input = std::make_shared<open_icue::KeyboardLightingInput>([queue] {
            QMutexLocker lock(&queue->mutex);
            auto reports = queue->reports;
            queue->reports.clear();
            return reports;
        });
        auto backend = std::make_shared<open_icue::HardwareBackend>(
            state_->path(), [endpoint] { return QList{endpoint}; },
            [peer](const auto& device) {
                return std::make_unique<open_icue::HidSession>(
                    device, [peer](const QByteArray& request) { return peer->exchange(request); });
            },
            input);
        model_->enableHardware(backend);
        const auto typeId = [this](const QString& prefix) {
            for (const auto& value : model_->lightingTypes()) {
                const auto id = value.toMap().value("id").toString();
                if (id.startsWith(prefix + "LightingTypeData"))
                    return id;
            }
            return QString();
        };
        const auto staticType = typeId("StaticCustom"), triggerType = typeId(effect);
        QVERIFY(!staticType.isEmpty() && !triggerType.isEmpty());
        const QVariantList layers{QVariantMap{{"id", "base"},
                                              {"name", "Background"},
                                              {"type", staticType},
                                              {"zones", QStringList{"A", "B"}},
                                              {"colors", QVariantList{"#0000ff"}}},
                                  QVariantMap{{"id", "trigger"},
                                              {"name", "Key reaction"},
                                              {"type", triggerType},
                                              {"zones", QStringList{"A", "B"}},
                                              {"colors", QVariantList{"#ff0000"}},
                                              {"duration", 5}}};
        model_->setDocumentValue("lightingLayers", layers);
        QTRY_VERIFY2_WITH_TIMEOUT(model_->isHardwareLightingActive(), qPrintable(model_->notice()),
                                  GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(selection()->previewColor("A"), QColor("#0000ff"),
                                  GUI_TIMEOUT_MS);
        QVERIFY(!peer->hasRedFrame(4, 230));
        QByteArray down(15, 0);
        down[0] = 8;
        down[2] = 4;
        {
            QMutexLocker lock(&queue->mutex);
            queue->reports.append(down);
        }
        QTRY_COMPARE_WITH_TIMEOUT(model_->lightingState().value("reactiveEventCount").toULongLong(),
                                  quint64(1), GUI_TIMEOUT_MS);
        QTRY_VERIFY_WITH_TIMEOUT(selection()->previewColor("A").red() > 100, GUI_TIMEOUT_MS);
        QCOMPARE(selection()->previewColor("B"), QColor("#0000ff"));
        QTRY_VERIFY_WITH_TIMEOUT(peer->hasRedFrame(4, 230), GUI_TIMEOUT_MS);
        QVERIFY(!model_->lightingState().contains("previewColors"));
        QVERIFY(capture("keyboard-triggered-frame-" + effect));
        QVERIFY(click("navigation-Assignments"));
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareLightingActive(), GUI_TIMEOUT_MS);
        QVERIFY(model_->hardwarePreviewColors().isEmpty());
        QCOMPARE(peer->mode, quint32(1));
    }
    void surroundIsOneClickWithoutAudioPopup() {
        QVERIFY(open("virtuoso_black", "Equalizer"));
        QVERIFY(!item("audio-target"));
        QVERIFY(!model_->document().value("isSurroundEnabled").toBool());
        QVERIFY(click("audio-options"));
        QVERIFY(model_->document().value("isSurroundEnabled").toBool());
        QCOMPARE(item("audio-options")->property("text").toString(),
                 QString::fromUtf8("♬  7.1 Sound"));
        QVERIFY(capture("surround-direct-toggle"));
        QVERIFY(click("audio-options"));
        QVERIFY(!model_->document().value("isSurroundEnabled").toBool());
        QVERIFY(!item("audio-target"));
    }
    void virtuosoReceiverDoesNotShowDarkstarPairing() {
        QVERIFY(open("virtuoso_black_dongle", "Connections"));
        QVERIFY(item("selected-connection-status")->isVisible());
        QVERIFY(!item("pairing-start")->isVisible());
        for (const auto& usb : model_->usbInventory())
            QCOMPARE(usb.toMap().value("modelId").toString(), QString("virtuoso_black_dongle"));
        QVERIFY(capture("virtuoso-receiver-scoped"));
    }
    void profileActivationSendsEachConnectedPeripheralItsOwnLayers() {
        QVERIFY(open("darkstar_wireless", "Lightings"));
        const QStringList ids{"darkstar_wireless", "k70_core_rgb", "virtuoso_black"};
        const QStringList colors{"#ff0000", "#00ff00", "#0000ff"};
        model_->createProfile("Whole profile");
        for (int index = 0; index < ids.size(); ++index) {
            model_->selectDevice(ids[index]);
            model_->setDocumentValue(
                "lightingLayers",
                QVariantList{
                    QVariantMap{{"name", "Distinct device color"},
                                {"type", "StaticCustomLightingTypeData.lightings.cue.corsair.com"},
                                {"zones", QStringList{"all"}},
                                {"colors", QVariantList{colors[index]}}}});
            QVERIFY(model_->lastOperation().value("isOk").toBool());
        }
        model_->selectDevice(ids.first());
        auto mouse = std::make_shared<GuiDpiPeer>();
        auto keyboard = std::make_shared<GuiDpiPeer>();
        keyboard->product = open_icue::K70_CORE_PRODUCT_ID;
        auto headset = std::make_shared<GuiDpiPeer>();
        headset->product = open_icue::VIRTUOSO_RECEIVER_PRODUCT_ID;
        const QList<open_icue::HidEndpoint> endpoints{
            {"profile-mouse", {}, "mouse", open_icue::DARKSTAR_PRODUCT_ID, {}, false},
            {"profile-keyboard", {}, "keyboard", open_icue::K70_CORE_PRODUCT_ID, {}, false},
            {"profile-headset", {}, "headset", open_icue::VIRTUOSO_RECEIVER_PRODUCT_ID, {}, true}};
        auto backend = std::make_shared<open_icue::HardwareBackend>(
            state_->path(), [endpoints] { return endpoints; },
            [mouse, keyboard, headset](const open_icue::HidEndpoint& endpoint) {
                auto peer = endpoint.product == open_icue::DARKSTAR_PRODUCT_ID   ? mouse
                            : endpoint.product == open_icue::K70_CORE_PRODUCT_ID ? keyboard
                                                                                 : headset;
                return std::make_unique<open_icue::HidSession>(
                    endpoint,
                    [peer](const QByteArray& request) { return peer->exchange(request); });
            });
        model_->enableHardware(backend);
        QTRY_COMPARE_WITH_TIMEOUT(
            model_->lightingState().value("profileDeviceResults").toList().size(), 3,
            GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(mouse->lightingPayload().size(), 39, GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(keyboard->lightingPayload().size(), 369, GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(headset->lightingPayload().size(), 9, GUI_TIMEOUT_MS);
        QCOMPARE(quint8(mouse->lightingPayload()[9]), quint8(255));
        QCOMPARE(quint8(keyboard->lightingPayload()[open_icue::k70LedIndex("A") + 123]),
                 quint8(255));
        QCOMPARE(quint8(headset->lightingPayload()[6]), quint8(255));
        QVERIFY(capture("profile-three-device-replay"));
        QVERIFY(click("navigation-Dpi"));
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareLightingActive() && !model_->isHardwareBusy(),
                                 GUI_TIMEOUT_MS);
        QCOMPARE(mouse->mode, 1U);
        QCOMPARE(keyboard->mode, 1U);
    }
    void profileSidebarAddsBelowListAndOffersRightClickActions() {
        QVERIFY(open("k70_core_rgb", "Lightings"));
        const auto originalId = model_->activeProfileId();
        QVERIFY(click("profiles-toggle"));
        QTRY_VERIFY_WITH_TIMEOUT(item("profile-add") && item("profile-add")->isVisible(),
                                 GUI_TIMEOUT_MS);
        const auto listBottom =
            item("profile-list")->mapToScene(QPointF(0, item("profile-list")->height())).y();
        QVERIFY(item("profile-add")->mapToScene(QPointF()).y() >= listBottom);
        QVERIFY(click("profile-add"));
        auto* createDialog =
            engine_->rootObjects().first()->findChild<QObject*>("profileCreateDialog");
        QVERIFY(createDialog);
        QVERIFY(QMetaObject::invokeMethod(createDialog, "accept"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->profiles().size(), 2, GUI_TIMEOUT_MS);
        const auto createdId = model_->activeProfileId();
        QVERIFY(createdId != originalId);
        const auto openContextMenu = [this](const QString& id) {
            window_->grabWindow();
            auto* row = item("profile-row-" + id);
            if (!row)
                return false;
            QTest::mouseClick(window_, Qt::RightButton, Qt::NoModifier,
                              row->mapToScene(QPointF(24, row->height() / 2)).toPoint());
            return QTest::qWaitFor(
                [this] {
                    return item("profile-duplicate") && item("profile-duplicate")->isVisible();
                },
                GUI_TIMEOUT_MS);
        };
        QVERIFY(openContextMenu(createdId));
        QVERIFY(capture("profile-right-click-menu"));
        QVERIFY(click("profile-duplicate"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->profiles().size(), 3, GUI_TIMEOUT_MS);
        const auto duplicatedId = model_->activeProfileId();
        QVERIFY(duplicatedId != createdId);
        QVERIFY(openContextMenu(duplicatedId));
        QVERIFY(click("profile-rename"));
        QTRY_VERIFY_WITH_TIMEOUT(item("profile-rename-name") &&
                                     item("profile-rename-name")->isVisible(),
                                 GUI_TIMEOUT_MS);
        item("profile-rename-name")->setProperty("text", "Renamed from menu");
        auto* renameDialog =
            engine_->rootObjects().first()->findChild<QObject*>("profileRenameDialog");
        QVERIFY(renameDialog);
        QVERIFY(QMetaObject::invokeMethod(renameDialog, "accept"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->profiles().last().toMap().value("name").toString(),
                                  QString("Renamed from menu"), GUI_TIMEOUT_MS);
        QVERIFY(openContextMenu(duplicatedId));
        QVERIFY(click("profile-delete"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->profiles().size(), 2, GUI_TIMEOUT_MS);
        QVERIFY(capture("profiles-after-actions"));
    }
    void assignmentKeyboard() {
        QVERIFY(open("darkstar_wireless", "Assignments"));
        QVERIFY(capture("assignments-before-input"));
        QVERIFY(click("assignment-input-Mouse3"));
        QCOMPARE(window_->property("selectedKey").toString(), QString("Mouse3"));
        QCOMPARE(item("assignmentPanel")->property("selectedKey").toString(), QString("Mouse3"));
        QVERIFY(click("device-view-1"));
        QCOMPARE(window_->property("viewIndex").toInt(), 1);
        QVERIFY(item("deviceImage")->property("source").toUrl().toString().endsWith("/1"));
        QVERIFY(click("assignment-type-Keyboard"));
        QVERIFY(click("assignment-keyboard"));
        QTRY_VERIFY_WITH_TIMEOUT(item("virtual-key-F24") && item("virtual-key-F24")->isVisible(),
                                 GUI_TIMEOUT_MS);
        QVERIFY(click("virtual-key-F24"));
        QVERIFY(capture("keyboard-F24"));
        QVERIFY(click("virtual-key-confirm"));
        QTRY_COMPARE_WITH_TIMEOUT(model_->document().value("assignments").toList().size(), 1,
                                  GUI_TIMEOUT_MS);
        QTRY_COMPARE_WITH_TIMEOUT(model_->document()
                                      .value("assignments")
                                      .toList()
                                      .first()
                                      .toMap()
                                      .value("params")
                                      .toMap()
                                      .value("key")
                                      .toString(),
                                  QString("F24"), GUI_TIMEOUT_MS);
        QCOMPARE(item("assignment-keyboard")->property("text").toString(), QString("F24"));
        QCOMPARE(model_->document()
                     .value("assignments")
                     .toList()
                     .first()
                     .toMap()
                     .value("input")
                     .toString(),
                 QString("Mouse3"));
        QVERIFY(capture("assignments-after-input"));
    }
    void keyboardClickShiftAndRectangle() {
        QVERIFY(open("k70_core_rgb", "Lightings"));
        QVERIFY(selection());
        QVERIFY(capture("keyboard-selection-before-input"));
        QVERIFY(click("deviceSelection", selection()->keyCenter("A")));
        QCOMPARE(zones(), QStringList{"A"});
        QVERIFY(click("lighting-add"));
        QCOMPARE(model_->document()
                     .value("lightingLayers")
                     .toList()
                     .first()
                     .toMap()
                     .value("zones")
                     .toStringList(),
                 QStringList{"A"});
        QVERIFY(click("deviceSelection", selection()->keyCenter("B"), Qt::ShiftModifier));
        const auto combined = zones();
        QCOMPARE(QSet<QString>(combined.begin(), combined.end()), (QSet<QString>{"A", "B"}));
        QVERIFY(click("deviceSelection", selection()->keyCenter("A"), Qt::ShiftModifier));
        QCOMPARE(zones(), QStringList{"B"});
        const auto first = selection()->keyRect("F1"), third = selection()->keyRect("F3");
        QVERIFY(!first.isEmpty() && !third.isEmpty());
        QVERIFY(
            dragSelection(first.topLeft() - QPointF(1, 1), third.bottomRight() + QPointF(1, 1)));
        const auto range = zones();
        QCOMPARE(QSet<QString>(range.begin(), range.end()), (QSet<QString>{"F1", "F2", "F3"}));
        QCOMPARE(selection()->selectedIds(), range);
        QVERIFY(capture("keyboard-selection-after-input"));
        window_->resize(1280, 900);
        QTRY_COMPARE_WITH_TIMEOUT(window_->width(), 1280, GUI_TIMEOUT_MS);
        QVERIFY(click("deviceSelection", selection()->keyCenter("A")));
        QCOMPARE(zones(), QStringList{"A"});
        open_icue::ApplicationModel restored(state_->path());
        restored.selectDevice("k70_core_rgb");
        QCOMPARE(restored.document().value("lightingLayers"),
                 model_->document().value("lightingLayers"));
    }
    void separateKeyColorsAndLayerParameters() {
        QVERIFY(open("k70_core_rgb", "Lightings"));
        QVERIFY(click("deviceSelection", selection()->keyCenter("A")));
        QVERIFY(click("lighting-add"));
        QVERIFY(chooseLightingType("StaticCustom"));
        QVERIFY(chooseStaticColor("ff0000"));
        QVERIFY(click("lighting-duplicate"));
        QVERIFY(click("deviceSelection", selection()->keyCenter("B")));
        QVERIFY(chooseStaticColor("0000ff"));
        const auto layers = model_->document().value("lightingLayers").toList();
        QCOMPARE(layers.size(), 2);
        QCOMPARE(layers.first().toMap().value("zones").toStringList(), QStringList{"A"});
        QCOMPARE(layers.last().toMap().value("zones").toStringList(), QStringList{"B"});
        QCOMPARE(selection()->previewColor("A"), QColor("#ff0000"));
        QCOMPARE(selection()->previewColor("B"), QColor("#0000ff"));
        QVERIFY(capture("keyboard-independent-colors"));
        QVERIFY(chooseLightingType("RainbowWave"));
        auto* speed = item("lighting-speed");
        QVERIFY(speed && speed->isVisible());
        QVERIFY(click("lighting-speed", QPointF(speed->width() - 6, speed->height() / 2)));
        QTRY_COMPARE_WITH_TIMEOUT(
            item("lightingPanel")->property("selectedLayer").toMap().value("speed").toInt(), 2,
            GUI_TIMEOUT_MS);
        QVERIFY(click("lighting-direction"));
        QTest::keyClick(window_, Qt::Key_End);
        QTest::keyClick(window_, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(
            item("lightingPanel")->property("selectedLayer").toMap().value("direction").toInt(), 3,
            GUI_TIMEOUT_MS);
        QVERIFY(capture("keyboard-effect-parameters"));
        QVERIFY(chooseLightingType("Gradient"));
        QVERIFY(click("lighting-duration"));
        QTest::keyClick(window_, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(window_, Qt::Key_2);
        QTest::keyClick(window_, Qt::Key_Period);
        QTest::keyClick(window_, Qt::Key_5);
        QTest::keyClick(window_, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(
            item("lightingPanel")->property("selectedLayer").toMap().value("duration").toDouble(),
            2.5, GUI_TIMEOUT_MS);
        QVERIFY(capture("keyboard-gradient-duration"));
    }
    void importedGradientTimelineRemainsEditableThroughVisibleControls() {
        const QVariantList points{QVariantMap{{"time", 0.0}, {"color", "#80ff0000"}},
                                  QVariantMap{{"time", 0.25}, {"color", "#80ff0000"}},
                                  QVariantMap{{"time", 1.0}, {"color", "#80ff0000"}}};
        const QVariantMap layer{{"name", "Imported timeline"},
                                {"type", "GradientLightingTypeData.lightings.cue.corsair.com"},
                                {"zones", QStringList{"A"}},
                                {"colors", QVariantList{"#80ff0000", "#80ff0000", "#80ff0000"}},
                                {"transitions", points},
                                {"isLooping", true},
                                {"duration", 2.0},
                                {"opacity", 100}};
        model_->selectDevice("k70_core_rgb");
        model_->setDocumentValue("lightingLayers", QVariantList{layer});
        QVERIFY(model_->lastOperation().value("isOk").toBool());
        QVERIFY(open("k70_core_rgb", "Lightings"));
        const auto preview = selection()->previewColor("A");
        QCOMPARE(preview.red(), 255);
        QCOMPARE(preview.green(), 0);
        QVERIFY(qAbs(preview.alpha() - 128) <= 1);
        QVERIFY(!item("lighting-transition-time-0")->isEnabled());
        QVERIFY(click("lighting-transition-time-1"));
        QTest::keyClick(window_, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(window_, Qt::Key_5);
        QTest::keyClick(window_, Qt::Key_0);
        QTest::keyClick(window_, Qt::Key_Return);
        auto timeline = [&] {
            return model_->document()
                .value("lightingLayers")
                .toList()
                .first()
                .toMap()
                .value("transitions")
                .toList();
        };
        QTRY_COMPARE_WITH_TIMEOUT(timeline().at(1).toMap().value("time").toDouble(), 0.5,
                                  GUI_TIMEOUT_MS);
        QVERIFY(chooseStaticColor("0000ff"));
        QTRY_COMPARE_WITH_TIMEOUT(timeline().first().toMap().value("color").toString(),
                                  QString("#0000ff"), GUI_TIMEOUT_MS);
        QCOMPARE(model_->document()
                     .value("lightingLayers")
                     .toList()
                     .first()
                     .toMap()
                     .value("colors")
                     .toList()
                     .first()
                     .toString(),
                 QString("#0000ff"));
        QVERIFY(click("lighting-color-add"));
        QTRY_COMPARE_WITH_TIMEOUT(timeline().size(), 4, GUI_TIMEOUT_MS);
        QCOMPARE(timeline().at(2).toMap().value("time").toDouble(), 0.75);
        QCOMPARE(timeline().last().toMap().value("time").toDouble(), 1.0);
        QVERIFY(capture("imported-gradient-edited"));
        QVERIFY(chooseLightingType("StaticCustom"));
        QVERIFY(!model_->document()
                     .value("lightingLayers")
                     .toList()
                     .first()
                     .toMap()
                     .contains("transitions"));
    }
    void allFifteenMouseButtonsHaveIndependentAssignments() {
        QVERIFY(open("darkstar_wireless", "Assignments"));
        QVariantList physical;
        for (const auto& value : model_->keys())
            if (!value.toMap().value("id").toString().startsWith("Gesture_"))
                physical.append(value);
        QCOMPARE(physical.size(), 15);
        for (int index = 0; index < physical.size(); ++index) {
            const auto key = physical.at(index).toMap();
            if (item("assignmentPanel")->property("isSide").toBool() !=
                key.value("isSide").toBool())
                QVERIFY(click("assignment-toggle-side"));
            QVERIFY(click("assignment-input-" + key.value("id").toString()));
            QCOMPARE(window_->property("selectedKey").toString(), key.value("id").toString());
            QVERIFY(click("assignment-keyboard"));
            QVERIFY(click("virtual-key-F" + QString::number(index + 1)));
            QVERIFY(click("virtual-key-confirm"));
        }
        const auto assignments = model_->document().value("assignments").toList();
        QCOMPARE(assignments.size(), 15);
        for (int index = 0; index < assignments.size(); ++index) {
            QCOMPARE(assignments.at(index).toMap().value("input"),
                     physical.at(index).toMap().value("id"));
            QCOMPARE(assignments.at(index).toMap().value("params").toMap().value("key").toString(),
                     "F" + QString::number(index + 1));
        }
        QVERIFY(click("deviceSelection", selection()->keyCenter("MouseSideFront1")));
        QCOMPARE(window_->property("selectedKey").toString(), QString("MouseSideFront1"));
        QVERIFY(click("assignment-keyboard"));
        for (const auto& group : {"extendedRows", "mainRows", "navigationRows", "numpadRows"})
            for (const auto& row : model_->assignmentKeyboard().value(group).toList())
                for (const auto& value : row.toList()) {
                    auto* button = item("virtual-key-" + value.toMap().value("id").toString());
                    QVERIFY(button && button->isVisible() && button->isEnabled());
                    QVERIFY(QRectF(QPointF(), window_->size())
                                .contains(button->mapToScene(
                                    QPointF(button->width() / 2, button->height() / 2))));
                }
        QVERIFY(click("virtual-key-KeypadEnter"));
        QVERIFY(capture("keyboard-full-layout"));
        QVERIFY(click("virtual-key-confirm"));
        QCOMPARE(item("assignment-keyboard")->property("text").toString(), QString("Num Enter"));
        QVERIFY(capture("mouse-fifteen-assignments"));
        open_icue::ApplicationModel restored(state_->path());
        QCOMPARE(restored.document().value("assignments"), model_->document().value("assignments"));
    }
    void mouseLightingUsesSelectableLedContours() {
        QVERIFY(open("darkstar_wireless", "Lightings"));
        QVERIFY(selection());
        QVERIFY(capture("mouse-lighting-before-input"));
        const auto rect = selection()->keyRect("MouseLogoLed");
        QVERIFY(!rect.isEmpty());
        QVERIFY(rect.width() < selection()->width() / 2);
        QVERIFY(click("deviceSelection", selection()->keyCenter("MouseLogoLed")));
        QCOMPARE(zones(), QStringList{"MouseLogoLed"});
        QVERIFY(click("lighting-add"));
        QCOMPARE(model_->document()
                     .value("lightingLayers")
                     .toList()
                     .first()
                     .toMap()
                     .value("zones")
                     .toStringList(),
                 QStringList{"MouseLogoLed"});
        QVERIFY(click("device-view-1"));
        QVERIFY(selection()->keyCenter("MouseDpiIndicator1").x() >= 0);
        QVERIFY(click("deviceSelection", selection()->keyCenter("MouseDpiIndicator1"),
                      Qt::ShiftModifier));
        const auto selected = zones();
        QCOMPARE(QSet<QString>(selected.begin(), selected.end()),
                 (QSet<QString>{"MouseLogoLed", "MouseDpiIndicator1"}));
        QVERIFY(capture("mouse-lighting-after-input"));
    }
    void svgSemanticPaletteChangesRenderedPixels() {
        open_icue::IconImages provider;
        QSize size;
        const auto original = provider.requestImage("controls/palette-test", &size, QSize(64, 64));
        QVERIFY(!original.isNull());
        QCOMPARE(size, QSize(64, 64));
        const auto recolored = provider.requestImage(
            "controls/palette-test?base=%23ff0000&accent=%2300ff00&secondary=%230000ff", &size,
            QSize(128, 128));
        QCOMPARE(recolored.size(), QSize(128, 128));
        int red = 0, green = 0, blue = 0;
        for (int y = 0; y < recolored.height(); ++y)
            for (int x = 0; x < recolored.width(); ++x) {
                const auto color = recolored.pixelColor(x, y);
                if (color.alpha() < 200)
                    continue;
                if (color.red() > 240 && color.green() < 10 && color.blue() < 10)
                    ++red;
                if (color.green() > 240 && color.red() < 10 && color.blue() < 10)
                    ++green;
                if (color.blue() > 240 && color.red() < 10 && color.green() < 10)
                    ++blue;
            }
        QVERIFY(red > 0 && green > 0 && blue > 0);
        QVERIFY(provider.requestImage("../vendor/escape", &size, QSize(64, 64)).isNull());
        QVERIFY(original.save(artifacts_ + "/svg-default-palette.png"));
        QVERIFY(recolored.save(artifacts_ + "/svg-custom-palette.png"));
        events_.append(QJsonObject{{"action", "SVG semantic role rendering"},
                                   {"dispatch", "native IconImages provider"},
                                   {"proof", "renderer infrastructure; not a separate GUI gesture"},
                                   {"redPixels", red},
                                   {"greenPixels", green},
                                   {"bluePixels", blue}});
    }
    void lightingTypeControlsFollowMetadata() {
        QVERIFY(open("k70_core_rgb", "Lightings"));
        QVERIFY(click("lighting-add"));
        QCOMPARE(model_->lightingTypes().size(), 14);
        for (const auto& value : model_->lightingTypes()) {
            const auto effect = value.toMap();
            QVERIFY2(chooseLightingType(effect.value("key").toString()),
                     qPrintable(effect.value("key").toString()));
            QCOMPARE(item("lightingPanel")->property("selectedLayer").toMap().value("type"),
                     effect.value("id"));
            const auto controls = effect.value("controls").toStringList();
            for (const auto& name : {"speed", "direction", "duration", "opacity"}) {
                auto* control = item("lighting-" + QString(name));
                QVERIFY(control);
                QCOMPARE(control->isVisible(), controls.contains(name));
            }
        }
        QVERIFY(capture("keyboard-lighting-types"));
    }
    void languageSelectorUpdatesUiAndPreservesProfiles() {
        model_->createProfile("My unchanged profile");
        model_->setDocumentValue(
            "assignments", QVariantList{QVariantMap{{"input", "Mouse2"},
                                                    {"type", "Keyboard"},
                                                    {"name", "My unchanged assignment"},
                                                    {"params", QVariantMap{{"key", "F24"}}}}});
        QVERIFY(model_->lastOperation().value("isOk").toBool());
        const auto document = model_->document();
        const auto profiles = model_->profiles();
        QVERIFY(open("darkstar_wireless", "Assignments"));
        const auto deviceName = model_->device().value("name");
        QVERIFY(click("application-settings"));
        QVERIFY(QTest::qWaitFor(
            [this] { return item("language-selector") && item("language-selector")->isVisible(); },
            GUI_TIMEOUT_MS));
        const QList<QPair<QString, int>> choices{{"en", 1}, {"de", 0}, {"ru", 2}};
        for (const auto& [code, index] : choices) {
            QVERIFY(click("language-selector"));
            const auto timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            QTest::keyClick(window_, Qt::Key_Home);
            for (int step = 0; step < index; ++step)
                QTest::keyClick(window_, Qt::Key_Down);
            QTest::keyClick(window_, Qt::Key_Return);
            QTRY_COMPARE_WITH_TIMEOUT(translations_->languageCode(), code, GUI_TIMEOUT_MS);
            const auto expected = code == "ru"   ? QString::fromUtf8("Язык")
                                  : code == "en" ? QString("Language")
                                                 : QString("Sprache");
            QTRY_COMPARE_WITH_TIMEOUT(item("language-label")->property("text").toString(), expected,
                                      GUI_TIMEOUT_MS);
            QCOMPARE(item("app-brand-label")->property("text").toString(), QString("OpenQUI"));
            QCOMPARE(model_->device().value("name"), deviceName);
            QCOMPARE(model_->document(), document);
            QCOMPARE(model_->profiles(), profiles);
            events_.append(QJsonObject{{"action", "language selector keyboard choice"},
                                       {"dispatch", "QTest keys in opened ComboBox"},
                                       {"timestampUtc", timestamp},
                                       {"language", code},
                                       {"visibleLabel", expected},
                                       {"isProfileUnchanged", true},
                                       {"isDeviceNameUnchanged", true}});
            QVERIFY(capture("settings-" + code));
        }
        QVERIFY(click("settings-close"));
        engine_.reset();
        window_ = nullptr;
        translations_.reset();
        translations_ = std::make_unique<open_icue::TranslationManager>(state_->path());
        connect(translations_.get(), &open_icue::TranslationManager::languageChanged, model_.get(),
                &open_icue::ApplicationModel::retranslate);
        QCOMPARE(translations_->languageCode(), QString("ru"));
        QVERIFY(open("darkstar_wireless", "Assignments"));
        QCOMPARE(model_->document(), document);
        QVERIFY(click("assignment-keyboard"));
        QVERIFY(capture("keyboard-language-ru"));
        QVERIFY(item("virtual-key-F24") && item("virtual-key-F24")->isVisible());
    }
    void translatedNavigationKeepsStablePanelAndKeyIds_data() {
        QTest::addColumn<QString>("language");
        QTest::newRow("de") << QString("de");
        QTest::newRow("en") << QString("en");
        QTest::newRow("ru") << QString("ru");
    }
    void translatedNavigationKeepsStablePanelAndKeyIds() {
        QFETCH(QString, language);
        QVERIFY(translations_->setLanguage(language));
        QVERIFY(open("darkstar_wireless", "Dpi"));
        const QList<QStringList> panels{
            {"virtuoso_black", "Equalizer", "eq-band-0"},
            {"darkstar_wireless", "Dpi", "dpi-select-0"},
            {"k70_core_rgb", "Lightings", "lighting-add"},
            {"slipstream_wireless_dongle_ss", "Connections", "pairing-status"}};
        for (const auto& panel : panels) {
            QVERIFY(click("device-" + panel[0]));
            QCOMPARE(window_->property("panelId").toString(), panel[1]);
            QVERIFY2(QTest::qWaitFor(
                         [this, &panel] { return item(panel[2]) && item(panel[2])->isVisible(); },
                         GUI_TIMEOUT_MS),
                     qPrintable(panel[1]));
        }
        QVERIFY(click("device-darkstar_wireless"));
        QVERIFY(click("navigation-Assignments"));
        QCOMPARE(window_->property("panelId").toString(), QString("Assignments"));
        QVERIFY(click("assignment-keyboard"));
        QVERIFY(QTest::qWaitFor(
            [this] { return item("virtual-key-F24") && item("virtual-key-F24")->isVisible(); },
            GUI_TIMEOUT_MS));
        QVERIFY(click("virtual-key-F24"));
        QVERIFY(click("virtual-key-confirm"));
        QCOMPARE(model_->document().value("assignments").toList().size(), 1);
        const auto assignment = model_->document().value("assignments").toList().first().toMap();
        QCOMPARE(assignment.value("input").toString(), QString("Mouse2"));
        QCOMPARE(assignment.value("type").toString(), QString("Keyboard"));
        QCOMPARE(assignment.value("params").toMap().value("key").toString(), QString("F24"));
        QVERIFY(capture("assignments-" + language));
    }
    void surroundTogglePersistsThroughDirectButtonInput() {
        QVERIFY(open("virtuoso_black", "Equalizer"));
        QVERIFY(click("audio-options"));
        QVERIFY(model_->document().value("isSurroundEnabled").toBool());
        QVERIFY(!item("surround-toggle"));
        QVERIFY(capture("surround-direct-persisted"));
        open_icue::ApplicationModel restarted(state_->path());
        restarted.selectDevice("virtuoso_black");
        QVERIFY(restarted.document().value("isSurroundEnabled").toBool());
    }
    void connectionDiagnosticsDoNotChangeProfilesOrPretendToPair() {
        QVERIFY(open("slipstream_wireless_dongle_ss", "Connections"));
        const auto original = model_->document();
        QVERIFY(!model_->isPairingImplemented());
        QVERIFY(item("pairing-start") && !item("pairing-start")->isEnabled());
        QCOMPARE(item("pairing-status")->property("text").toString(),
                 QString("Funkstatus nicht ausgelesen"));
        QVERIFY(capture("connections-de"));
        QVERIFY(click("connections-refresh"));
        QCOMPARE(model_->document(), original);
        QVERIFY(model_->lastOperation().value("isOk").toBool());
        const auto state = QJsonDocument::fromJson(model_->GetState().toUtf8()).object();
        QCOMPARE(state.value("isPairingImplemented").toBool(), false);
        QCOMPARE(state.value("scopedUsbInventory").toArray().size(), model_->usbInventory().size());
        for (const auto& usb : model_->usbInventory())
            QCOMPARE(usb.toMap().value("modelId").toString(),
                     QString("slipstream_wireless_dongle_ss"));
        QVERIFY(capture("connections-after-refresh"));
    }
    /** Opt-in acceptance through the same QML controls against attached physical HID devices. */
    void connectedDevicesAcceptLiveRgbEdits() {
        if (qEnvironmentVariable("OPENQUI_LIVE_TEST") != "1")
            QSKIP("Physical RGB writes require OPENQUI_LIVE_TEST=1");
        QVERIFY(open("darkstar_wireless", "Lightings"));
        model_->enableHardware();
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareBusy(), LIVE_TIMEOUT_MS);
        const QStringList models{"darkstar_wireless", "k70_core_rgb", "virtuoso_black"};
        for (const auto& modelId : models) {
            QVERIFY(click("device-" + modelId));
            QTRY_COMPARE_WITH_TIMEOUT(model_->selectedModelId(), modelId, GUI_TIMEOUT_MS);
            QVERIFY(click("navigation-Lightings"));
            QVERIFY2(model_->device().value("isConnected").toBool(), qPrintable(modelId));
            QVERIFY(click("lighting-select-all"));
            QVERIFY(click("lighting-add"));
            QVERIFY(chooseLightingType("StaticCustom"));
            QVERIFY(chooseStaticColor("ff0000"));
            QTRY_VERIFY_WITH_TIMEOUT(hasLatestLightingAck(modelId), LIVE_TIMEOUT_MS);
            QVERIFY(saveState(modelId + "-red-ack"));
            QVERIFY(click("lighting-color-0"));
            QVERIFY(click("color-preset-00ff00"));
            QTRY_VERIFY_WITH_TIMEOUT(hasLatestLightingAck(modelId), LIVE_TIMEOUT_MS);
            QCOMPARE(model_->document()
                         .value("lightingLayers")
                         .toList()
                         .first()
                         .toMap()
                         .value("colors")
                         .toList()
                         .first()
                         .toString(),
                     QString("#00ff00"));
            QVERIFY(item("color-confirm")->isVisible());
            QVERIFY(saveState(modelId + "-green-before-ok-ack"));
            QVERIFY(capture(modelId + "-green-before-ok"));
            QVERIFY(click("color-cancel"));
            QTRY_VERIFY_WITH_TIMEOUT(hasLatestLightingAck(modelId), LIVE_TIMEOUT_MS);
            QCOMPARE(model_->document()
                         .value("lightingLayers")
                         .toList()
                         .first()
                         .toMap()
                         .value("colors")
                         .toList()
                         .first()
                         .toString(),
                     QString("#ff0000"));
            QVERIFY(saveState(modelId + "-cancel-red-ack"));
            QVERIFY(click("lighting-clear-selection"));
            QTRY_VERIFY_WITH_TIMEOUT(hasLatestLightingAck(modelId), LIVE_TIMEOUT_MS);
            QVERIFY(saveState(modelId + "-clear-selection-ack"));
            QVERIFY(click("lighting-select-all"));
            QString rainbowType;
            for (const auto& effect : model_->lightingTypes())
                if (effect.toMap().value("key").toString().startsWith("Rainbow")) {
                    rainbowType = effect.toMap().value("key").toString();
                    break;
                }
            QVERIFY(!rainbowType.isEmpty());
            QVERIFY(chooseLightingType(rainbowType));
            QTRY_VERIFY_WITH_TIMEOUT(hasLatestLightingAck(modelId), LIVE_TIMEOUT_MS);
            QVERIFY(saveState(modelId + "-rainbow-ack"));
            model_->setDocumentValue("lightingLayers", QVariantList{});
            QTRY_VERIFY_WITH_TIMEOUT(hasLatestLightingAck(modelId), LIVE_TIMEOUT_MS);
            QVERIFY(saveState(modelId + "-empty-stack-ack"));
            QString otherPanel;
            for (const auto& panel : model_->navigation())
                if (panel.toMap().value("id").toString() != "Lightings") {
                    otherPanel = panel.toMap().value("id").toString();
                    break;
                }
            QVERIFY(!otherPanel.isEmpty());
            QVERIFY(click("navigation-" + otherPanel));
            QTRY_VERIFY_WITH_TIMEOUT(
                !model_->isHardwareBusy() && !model_->isHardwareLightingActive(), LIVE_TIMEOUT_MS);
            QVERIFY2(model_->lastOperation().value("isOk").toBool(), qPrintable(model_->notice()));
            QVERIFY(saveState(modelId + "-released"));
        }
    }
    /** Opt-in acceptance of automatic Virtuoso routing, channel layout and original voice playback.
     */
    void connectedVirtuosoUsesExclusiveAudioRoute() {
        if (qEnvironmentVariable("OPENQUI_LIVE_TEST") != "1")
            QSKIP("Physical audio routing requires OPENQUI_LIVE_TEST=1");
        const auto previousSink = configuredAudioSink();
        QVERIFY(open("virtuoso_black", "Equalizer"));
        model_->enableHardware();
        QTRY_VERIFY_WITH_TIMEOUT(!model_->isHardwareBusy(), LIVE_TIMEOUT_MS);
        QVERIFY(!model_->audioTargets().isEmpty());
        const QStringList supportedProducts{"0a41", "0a42", "0a4a"};
        for (const auto& entry : model_->audioTargets()) {
            const auto target = entry.toMap();
            QCOMPARE(target.value("vendor").toUInt(), uint(open_icue::CORSAIR_VENDOR_ID));
            QVERIFY(supportedProducts.contains(
                QString::number(target.value("product").toUInt(), 16).rightJustified(4, '0')));
        }
        for (const bool isSurround : {true, false}) {
            QVERIFY(click("audio-options"));
            QVERIFY2(model_->lastOperation().value("isOk").toBool(), qPrintable(model_->notice()));
            QCOMPARE(model_->document().value("isSurroundEnabled").toBool(), isSurround);
            QTRY_COMPARE_WITH_TIMEOUT(configuredAudioSink(), QString("open-icue.input"),
                                      LIVE_TIMEOUT_MS);
            QVERIFY(!item("audio-target"));
            QTRY_VERIFY_WITH_TIMEOUT(QJsonDocument::fromJson(model_->GetState().toUtf8())
                                         .object()
                                         .value("voicePrompt")
                                         .toObject()
                                         .value("isFinished")
                                         .toBool(),
                                     LIVE_TIMEOUT_MS);
            const auto state = QJsonDocument::fromJson(model_->GetState().toUtf8()).object();
            QVERIFY(state.value("isAudioRouteActive").toBool());
            QVERIFY(state.value("voicePrompt").toObject().value("isOk").toBool());
            QCOMPARE(state.value("voicePrompt").toObject().value("target").toString(),
                     state.value("audioTarget").toString());
            const auto graph = pipeWireGraph();
            quint32 outputId = 0, physicalId = 0;
            bool hasInput = false;
            for (const auto& entry : graph) {
                const auto object = entry.toObject();
                const auto props = object.value("info").toObject().value("props").toObject();
                if (props.value("node.name") == "open-icue.input") {
                    QCOMPARE(props.value("audio.channels").toVariant().toInt(), isSurround ? 8 : 2);
                    hasInput = true;
                }
                if (props.value("node.name") == "open-icue.output")
                    outputId = object.value("id").toInt();
                if (props.value("node.name") == state.value("audioTarget"))
                    physicalId = object.value("id").toInt();
            }
            QVERIFY(hasInput && outputId && physicalId);
            int links = 0;
            for (const auto& entry : graph) {
                const auto object = entry.toObject();
                const auto info = object.value("info").toObject();
                if (object.value("type") == "PipeWire:Interface:Link" &&
                    info.value("output-node-id").toInt() == int(outputId)) {
                    QCOMPARE(info.value("input-node-id").toInt(), int(physicalId));
                    ++links;
                }
            }
            QCOMPARE(links, 2);
            QVERIFY(saveState(isSurround ? "physical-surround-on" : "physical-surround-off"));
            QVERIFY(capture(isSurround ? "physical-surround-on" : "physical-surround-off"));
            QSaveFile snapshot(artifacts_ +
                               (isSurround ? "/surround-graph.json" : "/stereo-graph.json"));
            QVERIFY(snapshot.open(QIODevice::WriteOnly));
            QVERIFY(snapshot.write(QJsonDocument(graph).toJson()) > 0);
            QVERIFY(snapshot.commit());
        }
        model_->stopAudio();
        QTRY_COMPARE_WITH_TIMEOUT(configuredAudioSink(), previousSink, LIVE_TIMEOUT_MS);
        QVERIFY(saveState("physical-audio-restored"));
    }
    void cleanupTestCase() {
        QSaveFile file(artifacts_ + "/gui-events.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QJsonObject result{{"dispatch", "native Qt window mouse and keyboard events"},
                                 {"platform", QGuiApplication::platformName()},
                                 {"events", events_}};
        QVERIFY(file.write(QJsonDocument(result).toJson()) > 0);
        QVERIFY(file.commit());
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    qmlRegisterType<open_icue::DeviceSelection>("OpenICUE", 1, 0, "DeviceSelection");
    app.setApplicationName("open-icue-gui-tests");
    NativeGuiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_gui.moc"
