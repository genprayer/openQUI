#include "devices/hardware_backend.h"
#include "gui/application_model.h"
#include "gui/device_images.h"
#include "gui/device_selection.h"
#include "gui/icon_images.h"
#include "gui/system_tray.h"
#include "gui/translation_manager.h"
#include "service/dbus_adaptor.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <csignal>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace {
volatile std::sig_atomic_t SHOULD_QUIT = 0;
constexpr int SHUTDOWN_POLL_INTERVAL_MS = 50;
constexpr auto LEGACY_STATE_DIRECTORY = "OpenICUE/open-icue/native";
void requestShutdown(int) {
    SHOULD_QUIT = 1;
}
} // namespace

int main(int argc, char* argv[]) {
    bool isHeadless = false;
    for (int index = 1; index < argc; ++index)
        if (QByteArray(argv[index]) == "--service" || QByteArray(argv[index]) == "--dump-state" ||
            QByteArray(argv[index]) == "--export-eq" ||
            QByteArray(argv[index]) == "--probe-hardware" ||
            QByteArray(argv[index]) == "--pair-darkstar" ||
            QByteArray(argv[index]) == "--test-lighting")
            isHeadless = true;
    std::unique_ptr<QCoreApplication> application;
    if (isHeadless)
        application = std::make_unique<QCoreApplication>(argc, argv);
    else
        application = std::make_unique<QApplication>(argc, argv);
    auto& app = *application;
    app.setApplicationName("open-cue-gui");
    app.setOrganizationName("OpenQUI");
    app.setApplicationVersion(OPENQUI_VERSION);
    if (auto* gui = qobject_cast<QGuiApplication*>(&app))
        gui->setApplicationDisplayName("OpenQUI");
    QCommandLineParser parser;
    parser.setApplicationDescription("Native C++/Qt 6 OpenQUI");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions(
        {{{"s", "state-directory"}, "Workspace directory", "path"},
         {"service", "Run native D-Bus service"},
         {"device", "Select original device model", "id", "darkstar_wireless"},
         {"panel", "Open device panel", "id", "Dpi"},
         {"capture", "Save rendered native window and exit", "path"},
         {"dump-state", "Print native state and exit"},
         {"import-profile", "Import an original Cereal profile into the selected workspace",
          "path"},
         {"probe-hardware", "Read verified HID identities, battery and wireless links"},
         {"pair-darkstar", "Pair wired Darkstar with the separate 1bdc receiver"},
         {"test-lighting", "Send a one-second static RGB test, then restore firmware control",
          "color"},
         {"no-hardware", "Disable real HID access for isolated UI and D-Bus tests"},
         {"export-eq", "Export the selected profile's native EQ configuration", "path"},
         {"surround", "Use measured KEMAR 7.1 binaural rendering with --export-eq"},
         {"target", "Explicit PipeWire output node name", "name"},
         {"language", "Interface language: de, en or ru (temporary override)", "code"}});
    parser.process(app);
    // Signal handlers only set a flag; cleanup runs on the Qt thread, including HID RAII.
    std::signal(SIGINT, requestShutdown);
    std::signal(SIGTERM, requestShutdown);
    QTimer shutdownTimer;
    QObject::connect(&shutdownTimer, &QTimer::timeout, &app, [&app] {
        if (SHOULD_QUIT)
            app.quit();
    });
    shutdownTimer.start(SHUTDOWN_POLL_INTERVAL_MS);
    try {
        const auto state =
            parser.isSet("state-directory")
                ? parser.value("state-directory")
                : QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                      .filePath(LEGACY_STATE_DIRECTORY);
        if (parser.isSet("probe-hardware") || parser.isSet("pair-darkstar") ||
            parser.isSet("test-lighting")) {
            open_icue::HardwareBackend hardware(state);
            QVariantMap result;
            if (parser.isSet("test-lighting")) {
                const QColor color(parser.value("test-lighting"));
                if (!color.isValid())
                    throw std::invalid_argument("Invalid test color");
                open_icue::Catalog catalog;
                const QVariantList layers{QVariantMap{{"type", "StaticCustomLightingTypeData"},
                                                      {"zones", QStringList{"all"}},
                                                      {"colors", QVariantList{color.name()}},
                                                      {"opacity", 100}}};
                result = hardware.applyLighting(parser.value("device"), layers,
                                                catalog.lightingKeys(parser.value("device")));
                QThread::msleep(1000);
                hardware.stopLighting();
                result["afterRestore"] = hardware.probe();
            } else
                result = parser.isSet("pair-darkstar") ? hardware.pairDarkstar() : hardware.probe();
            std::puts(
                QJsonDocument::fromVariant(result).toJson(QJsonDocument::Indented).constData());
            return 0;
        }
        open_icue::ApplicationModel model(state);
        open_icue::TranslationManager translations(state, parser.value("language"));
        QObject::connect(&translations, &open_icue::TranslationManager::languageChanged, &model,
                         &open_icue::ApplicationModel::retranslate);
        model.selectDevice(parser.value("device"));
        if (!model.lastOperation().value("isOk").toBool())
            throw std::invalid_argument(model.notice().toStdString());
        if (parser.isSet("import-profile")) {
            model.importProfile(parser.value("import-profile"));
            if (!model.lastOperation().value("isOk").toBool())
                throw std::invalid_argument(model.notice().toStdString());
        }
        if (parser.isSet("dump-state")) {
            std::puts(model.GetState().toUtf8().constData());
            return 0;
        }
        if (parser.isSet("export-eq")) {
            if (parser.isSet("surround"))
                model.setDocumentValue("isSurroundEnabled", true);
            if (model.exportEqualizer(parser.value("export-eq"), parser.value("target")))
                return 0;
            std::fprintf(stderr, "%s\n", model.notice().toUtf8().constData());
            return 1;
        }
        if (!parser.isSet("no-hardware"))
            model.enableHardware();
        auto bus = QDBusConnection::sessionBus();
        if (bus.isConnected()) {
            if (bus.registerService("org.kde.cyberdeck.icue")) {
                new open_icue::DbusAdaptor(&model);
                if (!bus.registerObject("/org/kde/cyberdeck/icue", &model,
                                        QDBusConnection::ExportAdaptors))
                    throw std::runtime_error("Cannot register the native D-Bus object");
            } else
                throw std::runtime_error(
                    "OpenQUI is already running in this session; D-Bus service name is owned");
        } else if (parser.isSet("service"))
            throw std::runtime_error("Session D-Bus is unavailable");
        if (parser.isSet("service"))
            return app.exec();
        if (model.isUsingLocalCorsairResources()) {
            for (const auto& name : {"Regular", "SemiBold", "Bold"})
                QFontDatabase::addApplicationFont(
                    QString(":/vendor/fonts/OpenSans-%1.ttf").arg(name));
            QGuiApplication::setFont(QFont("Open Sans", 10));
        } else
            QGuiApplication::setFont(QFont("sans-serif", 10));
        QGuiApplication::setDesktopFileName("open-cue-gui");
        QGuiApplication::setWindowIcon(QIcon(":/app/branding/open-cue-gui.svg"));
        QQmlApplicationEngine engine;
        qmlRegisterType<open_icue::DeviceSelection>("OpenICUE", 1, 0, "DeviceSelection");
        engine.addImageProvider("devices", new open_icue::DeviceImages);
        engine.addImageProvider("icons", new open_icue::IconImages);
        engine.rootContext()->setContextProperty("icue", &model);
        engine.rootContext()->setContextProperty("i18n", &translations);
        QObject::connect(&translations, &open_icue::TranslationManager::languageChanged, &engine,
                         &QQmlApplicationEngine::retranslate);
        engine.rootContext()->setContextProperty("initialPanel", parser.value("panel"));
        engine.load(QUrl("qrc:/qml/Main.qml"));
        if (engine.rootObjects().isEmpty())
            return 1;
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        open_icue::SystemTray tray(&model, window);
        if (tray.isVisible())
            static_cast<QApplication&>(app).setQuitOnLastWindowClosed(false);
        if (parser.isSet("capture")) {
            const auto path = parser.value("capture");
            QTimer::singleShot(800, &app, [&app, &engine, path] {
                auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                const bool isSaved = window && window->grabWindow().save(path);
                app.exit(isSaved ? 0 : 2);
            });
        }
        return app.exec();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "OpenQUI: %s\n", error.what());
        return 1;
    }
}
