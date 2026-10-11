/** Verify the native service over a private D-Bus session, including variant values. */
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDomDocument>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace {
constexpr int SERVICE_TIMEOUT_MS = 3000;
constexpr auto SERVICE = "org.kde.cyberdeck.icue";
constexpr auto OBJECT_PATH = "/org/kde/cyberdeck/icue";
QMap<QString, QString> wireContract(const QByteArray& xml) {
    QDomDocument document;
    if (!document.setContent(xml))
        return {};
    QMap<QString, QString> result;
    for (auto interface = document.documentElement().firstChildElement("interface");
         !interface.isNull(); interface = interface.nextSiblingElement("interface")) {
        if (interface.attribute("name") != SERVICE)
            continue;
        for (auto member = interface.firstChildElement(); !member.isNull();
             member = member.nextSiblingElement()) {
            if (member.tagName() != "method" && member.tagName() != "signal")
                continue;
            QString input, output;
            for (auto arg = member.firstChildElement("arg"); !arg.isNull();
                 arg = arg.nextSiblingElement("arg")) {
                if (arg.attribute("direction") == "out")
                    output += arg.attribute("type");
                else
                    input += arg.attribute("type");
            }
            result.insert(member.tagName() + ":" + member.attribute("name"), input + ">" + output);
        }
    }
    return result;
}
} // namespace
class NativeDbusTest : public QObject {
    Q_OBJECT
    QTemporaryDir state_;
    QProcess service_;
    std::unique_ptr<QDBusInterface> proxy_;
    QJsonObject state() {
        const QDBusReply<QString> reply = proxy_->call("GetState");
        if (!reply.isValid()) {
            QTest::qFail(qPrintable(reply.error().message()), __FILE__, __LINE__);
            return {};
        }
        return QJsonDocument::fromJson(reply.value().toUtf8()).object();
    }
    QJsonObject invoke(const QString& method, const QVariantList& arguments = {}) {
        const QDBusReply<QString> reply =
            proxy_->callWithArgumentList(QDBus::Block, method, arguments);
        if (!reply.isValid()) {
            QTest::qFail(qPrintable(reply.error().message()), __FILE__, __LINE__);
            return {};
        }
        return QJsonDocument::fromJson(reply.value().toUtf8()).object();
    }
  private slots:
    void initTestCase() {
        QVERIFY(state_.isValid());
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        service_.start(QCoreApplication::applicationDirPath() + "/open-cue-gui",
                       {"--service", "--no-hardware", "--state-directory", state_.path()});
        QVERIFY(service_.waitForStarted(SERVICE_TIMEOUT_MS));
        QTRY_VERIFY_WITH_TIMEOUT(
            QDBusConnection::sessionBus().interface()->isServiceRegistered(SERVICE).value(),
            SERVICE_TIMEOUT_MS);
        proxy_ = std::make_unique<QDBusInterface>(SERVICE, OBJECT_PATH, SERVICE);
        QVERIFY(proxy_->isValid());
    }
    void variantFieldsAndRejections() {
        QVERIFY(invoke("selectDevice", {"darkstar_wireless"}).value("isOk").toBool());
        QVERIFY(invoke("setDpiStage", {0, "dpiX", QVariant::fromValue(QDBusVariant(1750))})
                    .value("isOk")
                    .toBool());
        auto document = state().value("document").toObject();
        QCOMPARE(document.value("dpiStages").toArray().first().toObject().value("dpiX").toInt(),
                 1750);
        const auto rejection =
            invoke("setDpiStage", {0, "dpiX", QVariant::fromValue(QDBusVariant(999999))});
        QVERIFY(!rejection.value("isOk").toBool());
        QVERIFY(!rejection.value("error").toString().isEmpty());
        QCOMPARE(state().value("document").toObject(), document);
        QVERIFY(
            invoke("setDocumentValue", {"isMultipleTilt", QVariant::fromValue(QDBusVariant(true))})
                .value("isOk")
                .toBool());
        QVERIFY(state().value("document").toObject().value("isMultipleTilt").toBool());
        const QVariantList colors{"#010203", "#112233", "#aabbcc"};
        const auto result = invoke("setDocumentValue",
                                   {"indicatorColors", QVariant::fromValue(QDBusVariant(colors))});
        QVERIFY2(result.value("isOk").toBool(), qPrintable(result.value("error").toString()));
        QCOMPARE(state()
                     .value("document")
                     .toObject()
                     .value("indicatorColors")
                     .toArray()
                     .first()
                     .toString(),
                 QString("#010203"));
        const auto before = state().value("document");
        QVERIFY(!invoke("applyHardware").value("isOk").toBool());
        QVERIFY(!invoke("applyDpiSettings").value("isOk").toBool());
        QCOMPARE(state().value("document"), before);
    }
    void introspectionMatchesDeclaredContract() {
        QFile declared(QFINDTESTDATA("../../packaging/dbus/org.kde.cyberdeck.icue.xml"));
        QVERIFY(declared.open(QIODevice::ReadOnly));
        const auto expected = wireContract(declared.readAll());
        QCOMPARE(expected.value("method:applyDpiSettings"), QString(">s"));
        QDBusInterface introspection(SERVICE, OBJECT_PATH, "org.freedesktop.DBus.Introspectable");
        const QDBusReply<QString> reply = introspection.call("Introspect");
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        QCOMPARE(wireContract(reply.value().toUtf8()), expected);
    }
    void profileCreationAndRestart() {
        QVERIFY(invoke("createProfile", {"D-Bus test"}).value("isOk").toBool());
        const auto before = state().value("workspace").toObject();
        QCOMPARE(before.value("profiles").toArray().size(), 2);
        service_.terminate();
        QVERIFY(service_.waitForFinished(SERVICE_TIMEOUT_MS));
        service_.start(QCoreApplication::applicationDirPath() + "/open-cue-gui",
                       {"--service", "--no-hardware", "--state-directory", state_.path()});
        QVERIFY(service_.waitForStarted(SERVICE_TIMEOUT_MS));
        QTRY_VERIFY_WITH_TIMEOUT(
            QDBusConnection::sessionBus().interface()->isServiceRegistered(SERVICE).value(),
            SERVICE_TIMEOUT_MS);
        QCOMPARE(state().value("workspace").toObject(), before);
    }
    void duplicateServiceIsRejected() {
        QTemporaryDir directory;
        QProcess second;
        second.start(QCoreApplication::applicationDirPath() + "/open-cue-gui",
                     {"--service", "--no-hardware", "--state-directory", directory.path()});
        QVERIFY(second.waitForFinished(SERVICE_TIMEOUT_MS));
        QVERIFY(second.exitCode() != 0);
        QVERIFY(second.readAllStandardError().contains("already running"));
        QCOMPARE(QDBusConnection::sessionBus().interface()->servicePid(SERVICE).value(),
                 static_cast<uint>(service_.processId()));
    }
    void filterDiesWithOwner() {
        const auto target = qEnvironmentVariable("OPENQUI_TEST_AUDIO_TARGET");
        if (target.isEmpty())
            QSKIP("Run the private PipeWire test for filter crash recovery");
        QVERIFY(invoke("selectDevice", {"virtuoso_black"}).value("isOk").toBool());
        const auto started = invoke("startAudio", {target});
        QVERIFY2(started.value("isOk").toBool(), qPrintable(started.value("error").toString()));
        QTRY_VERIFY_WITH_TIMEOUT(state().value("isAudioClientRunning").toBool(),
                                 SERVICE_TIMEOUT_MS);
        const auto childPid = state().value("audioClientPid").toInteger();
        QVERIFY(childPid > 0);
        QFile children(QString("/proc/%1/task/%1/children").arg(service_.processId()));
        QVERIFY(children.open(QIODevice::ReadOnly));
        QVERIFY(children.readAll().contains(QByteArray::number(childPid)));
        QVERIFY(invoke("setDocumentValue",
                       {"isSurroundEnabled", QVariant::fromValue(QDBusVariant(true))})
                    .value("isOk")
                    .toBool());
        QVERIFY(invoke("selectDevice", {"darkstar_wireless"}).value("isOk").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(state().value("audioClientPid").toInteger() > 0 &&
                                     state().value("audioClientPid").toInteger() != childPid,
                                 SERVICE_TIMEOUT_MS);
        QCOMPARE(state().value("audioModelId").toString(), QString("virtuoso_black"));
        const auto restartedChildPid = state().value("audioClientPid").toInteger();
        QVERIFY(restartedChildPid > 0);
        QFile config(state_.path() + "/equalizer.conf");
        QVERIFY(config.open(QIODevice::ReadOnly));
        QVERIFY(config.readAll().contains("\"audio.channels\" = 8"));
        service_.kill();
        QVERIFY(service_.waitForFinished(SERVICE_TIMEOUT_MS));
        QVERIFY(QTest::qWaitFor(
            [restartedChildPid] {
                QFile status(QString("/proc/%1/stat").arg(restartedChildPid));
                if (!status.open(QIODevice::ReadOnly))
                    return true;
                const auto bytes = status.readAll();
                const auto closingName = bytes.lastIndexOf(')');
                return closingName >= 0 && bytes.mid(closingName + 2, 1) == "Z";
            },
            SERVICE_TIMEOUT_MS));
    }
    void cleanupTestCase() {
        if (service_.state() != QProcess::NotRunning) {
            service_.terminate();
            if (!service_.waitForFinished(SERVICE_TIMEOUT_MS)) {
                service_.kill();
                service_.waitForFinished(SERVICE_TIMEOUT_MS);
            }
        }
        const auto errors = service_.readAllStandardError();
        QVERIFY2(errors.isEmpty(), errors.constData());
    }
};
QTEST_GUILESS_MAIN(NativeDbusTest)
#include "test_dbus.moc"
