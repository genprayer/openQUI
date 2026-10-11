/** Exact headset identity and scoped, reversible PipeWire metadata changes. */
#include "audio/routing.h"
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QJsonObject node(int id, const QString& name, const QString& media, int card = -1, int client = 0,
                 qint64 serial = 0) {
    return {{"id", id},
            {"type", "PipeWire:Interface:Node"},
            {"info", QJsonObject{{"props", QJsonObject{{"node.name", name},
                                                       {"media.class", media},
                                                       {"api.alsa.pcm.card", card},
                                                       {"client.id", client},
                                                       {"object.serial", serial}}}}}};
}
class GraphBackend final : public open_icue::AudioRoutingBackend {
  public:
    QJsonArray value;
    int writes = 0;
    QJsonArray graph() override {
        return value;
    }
    void setMetadata(quint32 subject, const QString& key, const QString& content,
                     const QString& type) override {
        for (int i = 0; i < value.size(); ++i) {
            auto object = value[i].toObject();
            if (object.value("type") != "PipeWire:Interface:Metadata")
                continue;
            auto rows = object.value("metadata").toArray();
            for (int j = rows.size() - 1; j >= 0; --j)
                if (rows[j].toObject().value("subject").toInteger() == subject &&
                    rows[j].toObject().value("key") == key)
                    rows.removeAt(j);
            if (!type.isEmpty())
                rows.append(QJsonObject{
                    {"subject", int(subject)}, {"key", key}, {"type", type}, {"value", content}});
            object["metadata"] = rows;
            value[i] = object;
            ++writes;
        }
    }
    void deleteMetadata(quint32 subject, const QString& key) override {
        setMetadata(subject, key, {}, {});
    }
    QString read(quint32 subject, const QString& key) const {
        for (const auto& item : value)
            for (const auto& row : item.toObject().value("metadata").toArray())
                if (row.toObject().value("subject").toInteger() == subject &&
                    row.toObject().value("key") == key)
                    return row.toObject().value("value").toString();
        return {};
    }
};
} // namespace
class AudioRoutingTests : public QObject {
    Q_OBJECT
  private slots:
    void exactUsbAncestryAndOwnedMetadataRestoration() {
        QTemporaryDir directory;
        const auto root = directory.path();
        const auto usb = root + "/usb/1-1";
        QVERIFY(QDir().mkpath(usb + "/1-1:1.0"));
        QVERIFY(QDir().mkpath(root + "/sound/card0"));
        QFile vendor(usb + "/idVendor"), product(usb + "/idProduct");
        QVERIFY(vendor.open(QIODevice::WriteOnly));
        QCOMPARE(vendor.write("1b1c\n"), qint64(5));
        vendor.close();
        QVERIFY(product.open(QIODevice::WriteOnly));
        QCOMPARE(product.write("0a42\n"), qint64(5));
        product.close();
        QVERIFY(QFile::link(usb + "/1-1:1.0", root + "/sound/card0/device"));
        auto backend = std::make_shared<GraphBackend>();
        backend->value = QJsonArray{
            node(10, "headset-output", "Audio/Sink", 0),
            node(11, "Corsair-named-but-unrelated", "Audio/Sink", 1),
            node(20, "open-icue.input", "Audio/Sink", -1, 21),
            QJsonObject{
                {"id", 21},
                {"type", "PipeWire:Interface:Client"},
                {"info", QJsonObject{{"props", QJsonObject{{"application.process.id", "123"}}}}}},
            node(30, "game", "Stream/Output/Audio", -1, 22, 300),
            node(31, "unrelated-game", "Stream/Output/Audio", -1, 22, 301),
            QJsonObject{{"type", "PipeWire:Interface:Link"},
                        {"info", QJsonObject{{"input-node-id", 10}, {"output-node-id", 30}}}},
            QJsonObject{{"type", "PipeWire:Interface:Link"},
                        {"info", QJsonObject{{"input-node-id", 11}, {"output-node-id", 31}}}},
            QJsonObject{{"type", "PipeWire:Interface:Metadata"},
                        {"props", QJsonObject{{"metadata.name", "default"}}},
                        {"metadata", QJsonArray{}}}};
        const auto targets = open_icue::audioTargetsFromGraph(backend->value, root + "/sound");
        QCOMPARE(targets.size(), 2);
        const auto scoped = open_icue::headsetAudioTargetsForUsb(targets, usb);
        QCOMPARE(scoped.size(), 1);
        QCOMPARE(scoped.first().toMap().value("id").toString(), QString("headset-output"));
        QVERIFY(open_icue::headsetAudioTargetsForUsb(targets, root + "/unrelated").isEmpty());
        backend->setMetadata(0, "default.configured.audio.sink", "{\"name\":\"headset-output\"}",
                             "Spa:String:JSON");
        open_icue::HeadsetAudioRoute route(backend, root + "/sound");
        route.activate(usb, "headset-output", 123);
        QVERIFY(route.isActive());
        QCOMPARE(backend->read(30, "target.object"), QString("open-icue.input"));
        QVERIFY(backend->read(31, "target.object").isEmpty());
        route.deactivate();
        QCOMPARE(backend->read(0, "default.configured.audio.sink"),
                 QString("{\"name\":\"headset-output\"}"));
        QVERIFY(backend->read(30, "target.object").isEmpty());
        route.activate(usb, "headset-output", 123);
        backend->setMetadata(0, "default.configured.audio.sink", "{\"name\":\"user-choice\"}",
                             "Spa:String:JSON");
        backend->setMetadata(30, "target.object", "user-choice", "Spa:String");
        route.deactivate();
        QCOMPARE(backend->read(0, "default.configured.audio.sink"),
                 QString("{\"name\":\"user-choice\"}"));
        QCOMPARE(backend->read(30, "target.object"), QString("user-choice"));
        QVERIFY(!route.isActive());
    }
};
QTEST_GUILESS_MAIN(AudioRoutingTests)
#include "test_audio_routing.moc"
