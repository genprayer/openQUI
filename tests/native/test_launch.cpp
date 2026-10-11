/** Verify the documented launcher owns a normal window in the active KDE session. */
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {
constexpr int LAUNCH_TIMEOUT_MS = 5000;
constexpr auto PROBE_INTERFACE = "org.kde.cyberdeck.icue.LaunchProbe";
} // namespace
class NativeLaunchTest : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.cyberdeck.icue.LaunchProbe")
    QTemporaryDir temporary_;
    QProcess application_;
    QString probeService_, pluginName_, artifacts_;
    QJsonArray windows_;
  public slots:
    Q_SCRIPTABLE void Record(const QString& json) {
        windows_ = QJsonDocument::fromJson(json.toUtf8()).array();
    }
  private slots:
    void initTestCase() {
        QVERIFY(temporary_.isValid());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        probeService_ =
            QString(PROBE_INTERFACE) + ".p" + QString::number(QCoreApplication::applicationPid());
        QVERIFY(bus.registerService(probeService_));
        QVERIFY(bus.registerObject("/probe", this, QDBusConnection::ExportScriptableSlots));
        artifacts_ = qEnvironmentVariable("OPENQUI_LAUNCH_ARTIFACTS", temporary_.path());
        QVERIFY(QDir().mkpath(artifacts_));
    }
    void documentedLauncherOwnsKdeWindow() {
        const auto launcher = QFINDTESTDATA("../../scripts/run.sh");
        QVERIFY(!launcher.isEmpty());
        const QStringList arguments{"--state-directory", temporary_.path() + "/state",
                                    "--device",          "virtuoso_black",
                                    "--panel",           "Equalizer"};
        application_.start(launcher, arguments);
        QVERIFY(application_.waitForStarted(LAUNCH_TIMEOUT_MS));
        const auto pid = application_.processId();
        auto bus = QDBusConnection::sessionBus();
        QTRY_VERIFY_WITH_TIMEOUT(
            bus.interface()->isServiceRegistered("org.kde.cyberdeck.icue").value(),
            LAUNCH_TIMEOUT_MS);
        QCOMPARE(bus.interface()->servicePid("org.kde.cyberdeck.icue").value(),
                 static_cast<uint>(pid));
        const auto binary = QFileInfo(qEnvironmentVariable("OPENQUI_BINARY",
                                                           QCoreApplication::applicationDirPath() +
                                                               "/open-cue-gui"))
                                .canonicalFilePath();
        QCOMPARE(QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget(), binary);
        QDBusInterface scripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting");
        QVERIFY(scripting.isValid());
        // KWin's installed windowList() API is used only to observe this process.
        const auto scriptPath = temporary_.path() + "/observe-window.js";
        QFile script(scriptPath);
        QVERIFY(script.open(QIODevice::WriteOnly));
        const auto source =
            QString(
                "function publishWindow() {\n"
                "const rows = [];\n"
                "for (const w of workspace.windowList()) {\n"
                " if (w.pid !== %1) continue;\n"
                " rows.push({pid:w.pid,caption:w.caption,id:w.internalId.toString(),"
                "isNormalWindow:w.normalWindow,isManaged:w.managed,isDeleted:w.deleted,"
                "isMinimized:w.minimized,isActive:w.active,resourceClass:w.resourceClass,"
                "geometry:{x:w.frameGeometry.x,y:w.frameGeometry.y,width:w.frameGeometry.width,"
                "height:w.frameGeometry.height},"
                "desktopIds:w.desktops.map(d=>d.id),currentDesktop:workspace.currentDesktop.id});\n"
                "}\n"
                "callDBus('%2','/probe','%3','Record',JSON.stringify(rows));\n"
                "}\n"
                "workspace.windowAdded.connect(publishWindow);\n"
                "publishWindow();\n")
                .arg(pid)
                .arg(probeService_, PROBE_INTERFACE)
                .toUtf8();
        QCOMPARE(script.write(source), source.size());
        script.close();
        pluginName_ = "open-icue-launch-probe-" + QString::number(pid);
        const QDBusReply<int> loaded = scripting.call("loadScript", scriptPath, pluginName_);
        QVERIFY2(loaded.isValid(), qPrintable(loaded.error().message()));
        QVERIFY(loaded.value() >= 0);
        QDBusInterface observer("org.kde.KWin", QString("/Scripting/Script%1").arg(loaded.value()),
                                "org.kde.kwin.Script");
        QVERIFY(observer.isValid());
        const auto run = observer.call("run");
        QVERIFY2(run.type() != QDBusMessage::ErrorMessage, qPrintable(run.errorMessage()));
        QTRY_VERIFY_WITH_TIMEOUT(!windows_.isEmpty(), LAUNCH_TIMEOUT_MS);
        const auto window = windows_.first().toObject();
        QCOMPARE(window.value("pid").toInteger(), pid);
        QCOMPARE(window.value("caption").toString(), QString("OpenQUI"));
        QVERIFY(window.value("isNormalWindow").toBool());
        QVERIFY(window.value("isManaged").toBool());
        QVERIFY(!window.value("isDeleted").toBool());
        QVERIFY(!window.value("isMinimized").toBool());
        QVERIFY(window.value("geometry").toObject().value("width").toDouble() >= 1280);
        QVERIFY(window.value("geometry").toObject().value("height").toDouble() >= 840);
        const QDBusReply<QString> state =
            QDBusInterface("org.kde.cyberdeck.icue", "/org/kde/cyberdeck/icue",
                           "org.kde.cyberdeck.icue")
                .call("GetState");
        QVERIFY(state.isValid());
        QCOMPARE(QJsonDocument::fromJson(state.value().toUtf8())
                     .object()
                     .value("selectedModelId")
                     .toString(),
                 QString("virtuoso_black"));
        QFile executable(binary);
        QVERIFY(executable.open(QIODevice::ReadOnly));
        const auto sha =
            QCryptographicHash::hash(executable.readAll(), QCryptographicHash::Sha256).toHex();
        QSaveFile report(artifacts_ + "/desktop-launch.json");
        QVERIFY(report.open(QIODevice::WriteOnly));
        const QJsonObject result{
            {"launcher", launcher},
            {"arguments", QJsonArray::fromStringList(arguments)},
            {"binary", binary},
            {"sha256", QString::fromLatin1(sha)},
            {"pid", pid},
            {"windows", windows_},
            {"state", QJsonDocument::fromJson(state.value().toUtf8()).object()}};
        QVERIFY(report.write(QJsonDocument(result).toJson()) > 0);
        QVERIFY(report.commit());
    }
    void cleanupTestCase() {
        if (!pluginName_.isEmpty()) {
            const QDBusReply<bool> removed =
                QDBusInterface("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting")
                    .call("unloadScript", pluginName_);
            QVERIFY(removed.isValid() && removed.value());
        }
        application_.terminate();
        if (!application_.waitForFinished(LAUNCH_TIMEOUT_MS)) {
            application_.kill();
            application_.waitForFinished(LAUNCH_TIMEOUT_MS);
        }
        QFile log(artifacts_ + "/desktop-launch.log");
        QVERIFY(log.open(QIODevice::WriteOnly));
        log.write(application_.readAllStandardOutput());
        log.write(application_.readAllStandardError());
        QDBusConnection::sessionBus().unregisterObject("/probe");
        QDBusConnection::sessionBus().unregisterService(probeService_);
    }
};
QTEST_GUILESS_MAIN(NativeLaunchTest)
#include "test_launch.moc"
