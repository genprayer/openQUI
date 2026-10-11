#include "routing.h"
#include "devices/hid_transport.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <stdexcept>

namespace open_icue {
namespace {
constexpr int PROCESS_TIMEOUT_MS = 1500;
constexpr int ROUTE_READY_TIMEOUT_MS = 2000;
constexpr int ROUTE_POLL_INTERVAL_MS = 25;
constexpr auto FILTER_INPUT_NAME = "open-icue.input";
constexpr auto DEFAULT_SINK_KEY = "default.configured.audio.sink";

QByteArray commandOutput(const QString& program, const QStringList& arguments = {}) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(PROCESS_TIMEOUT_MS)) {
        process.kill();
        process.waitForFinished(PROCESS_TIMEOUT_MS);
        throw std::runtime_error("PipeWire command timed out");
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        throw std::runtime_error("PipeWire command failed");
    return process.readAllStandardOutput();
}
QJsonObject properties(const QJsonValue& entry) {
    return entry.toObject().value("info").toObject().value("props").toObject();
}
QString jsonText(const QJsonValue& value) {
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    const auto text = value.toVariant().toString();
    const auto parsed = QJsonDocument::fromJson(text.toUtf8());
    return parsed.isObject() ? QString::fromUtf8(parsed.toJson(QJsonDocument::Compact)) : text;
}
QJsonObject metadata(const QJsonArray& graph, quint32 subject, const QString& key) {
    for (const auto& entry : graph) {
        const auto object = entry.toObject();
        if (object.value("type") != "PipeWire:Interface:Metadata" ||
            object.value("props").toObject().value("metadata.name") != "default")
            continue;
        for (const auto& value : object.value("metadata").toArray()) {
            const auto item = value.toObject();
            if (item.value("subject").toInteger() == subject && item.value("key") == key)
                return item;
        }
    }
    return {};
}
quint16 usbAttribute(const QString& path, const QString& name) {
    QFile file(path + '/' + name);
    return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed().toUShort(nullptr, 16) : 0;
}
QString usbForCard(const QString& soundRoot, const QString& card) {
    if (!QRegularExpression("^[0-9]+$").match(card).hasMatch())
        return {};
    QString path = QFileInfo(soundRoot + "/card" + card + "/device").canonicalFilePath();
    while (!path.isEmpty() && path != "/") {
        if (QFile::exists(path + "/idVendor") && QFile::exists(path + "/idProduct"))
            return path;
        path = QFileInfo(path).dir().absolutePath();
    }
    return {};
}
class PipeWireRoutingBackend final : public AudioRoutingBackend {
  public:
    QJsonArray graph() override {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(commandOutput("pw-dump"), &error);
        if (error.error != QJsonParseError::NoError || !document.isArray())
            throw std::runtime_error("Invalid PipeWire graph");
        return document.array();
    }
    void setMetadata(quint32 subject, const QString& key, const QString& value,
                     const QString& type) override {
        (void)commandOutput("pw-metadata",
                            {"-n", "default", QString::number(subject), key, value, type});
    }
    void deleteMetadata(quint32 subject, const QString& key) override {
        (void)commandOutput("pw-metadata", {"-n", "default", QString::number(subject), key});
    }
};
} // namespace

QVariantList audioTargetsFromGraph(const QJsonArray& graph, const QString& soundClassRoot) {
    const auto defaultName =
        metadata(graph, 0, "default.audio.sink").value("value").toObject().value("name").toString();
    QVariantList result;
    for (const auto& entry : graph) {
        const auto props = properties(entry);
        const auto name = props.value("node.name").toString();
        if (props.value("media.class") != "Audio/Sink" || name.isEmpty() ||
            name.startsWith("open-icue."))
            continue;
        const auto usb =
            usbForCard(soundClassRoot, props.value("api.alsa.pcm.card").toVariant().toString());
        result.append(QVariantMap{{"id", name},
                                  {"name", props.value("node.description").toString(name)},
                                  {"nodeId", entry.toObject().value("id").toVariant()},
                                  {"serial", props.value("object.serial").toVariant()},
                                  {"usbSysPath", usb},
                                  {"vendor", usbAttribute(usb, "idVendor")},
                                  {"product", usbAttribute(usb, "idProduct")},
                                  {"isDefault", name == defaultName}});
    }
    return result;
}
QVariantList queryAudioTargets() {
    const auto runtime =
        qEnvironmentVariable("PIPEWIRE_RUNTIME_DIR", qEnvironmentVariable("XDG_RUNTIME_DIR"));
    const auto remote = qEnvironmentVariable("PIPEWIRE_REMOTE", "pipewire-0");
    const auto socket = remote.startsWith('/') ? remote : runtime + '/' + remote;
    if (runtime.isEmpty() || !QFile::exists(socket))
        return {};
    try {
        PipeWireRoutingBackend backend;
        return audioTargetsFromGraph(backend.graph());
    } catch (const std::exception& error) {
        qWarning("Audio discovery: %s", error.what());
        return {};
    }
}
QVariantList headsetAudioTargetsForUsb(const QVariantList& targets, const QString& usbSysPath) {
    const auto selected = QFileInfo(usbSysPath).canonicalFilePath();
    if (selected.isEmpty())
        return {};
    QVariantList result;
    for (const auto& entry : targets) {
        const auto item = entry.toMap();
        if (item.value("usbSysPath").toString() != selected ||
            item.value("vendor").toUInt() != CORSAIR_VENDOR_ID ||
            !QList<quint32>{0x0a41, VIRTUOSO_RECEIVER_PRODUCT_ID, 0x0a4a}.contains(
                item.value("product").toUInt()))
            continue;
        result.append(item);
    }
    return result;
}
struct HeadsetAudioRoute::State {
    struct Change {
        quint32 subject;
        QString key, value;
        QJsonObject previous;
        qint64 nodeSerial;
    };
    QList<Change> changes;
};
HeadsetAudioRoute::HeadsetAudioRoute(std::shared_ptr<AudioRoutingBackend> backend,
                                     QString soundClassRoot)
    : state_(std::make_unique<State>()),
      backend_(backend ? std::move(backend) : std::make_shared<PipeWireRoutingBackend>()),
      soundClassRoot_(std::move(soundClassRoot)) {}
HeadsetAudioRoute::~HeadsetAudioRoute() {
    try {
        deactivate();
    } catch (const std::exception& error) {
        qWarning("Audio route restoration: %s", error.what());
    }
}
bool HeadsetAudioRoute::isActive() const {
    return !state_->changes.isEmpty();
}
void HeadsetAudioRoute::activate(const QString& usbSysPath, const QString& physicalTarget,
                                 qint64 filterPid) {
    deactivate();
    QJsonArray graph;
    quint32 inputId = 0, physicalId = 0;
    QElapsedTimer timer;
    timer.start();
    do {
        graph = backend_->graph();
        const auto targets =
            headsetAudioTargetsForUsb(audioTargetsFromGraph(graph, soundClassRoot_), usbSysPath);
        for (const auto& target : targets)
            if (target.toMap().value("id").toString() == physicalTarget)
                physicalId = target.toMap().value("nodeId").toUInt();
        QSet<quint32> clients;
        for (const auto& entry : graph)
            if (entry.toObject().value("type") == "PipeWire:Interface:Client" &&
                properties(entry).value("application.process.id").toVariant().toLongLong() ==
                    filterPid)
                clients.insert(entry.toObject().value("id").toInt());
        for (const auto& entry : graph)
            if (properties(entry).value("node.name") == FILTER_INPUT_NAME &&
                clients.contains(properties(entry).value("client.id").toInt()))
                inputId = entry.toObject().value("id").toInt();
        if (inputId && physicalId)
            break;
        QThread::msleep(ROUTE_POLL_INTERVAL_MS);
    } while (timer.elapsed() < ROUTE_READY_TIMEOUT_MS);
    if (!physicalId || !inputId)
        throw std::runtime_error("Selected headset audio route is unavailable");

    auto change = [&](quint32 subject, const QString& key, const QString& value,
                      const QString& type, qint64 serial = 0) {
        const auto previous = metadata(graph, subject, key);
        backend_->setMetadata(subject, key, value, type);
        state_->changes.append({subject, key, value, previous, serial});
    };
    try {
        change(0, DEFAULT_SINK_KEY,
               QString::fromUtf8(QJsonDocument(QJsonObject{{"name", FILTER_INPUT_NAME}})
                                     .toJson(QJsonDocument::Compact)),
               "Spa:String:JSON");
        QSet<quint32> streams;
        for (const auto& entry : graph) {
            const auto info = entry.toObject().value("info").toObject();
            if (entry.toObject().value("type") == "PipeWire:Interface:Link" &&
                info.value("input-node-id").toInteger() == physicalId)
                streams.insert(info.value("output-node-id").toInt());
        }
        for (const auto& entry : graph) {
            const auto props = properties(entry);
            const auto id = quint32(entry.toObject().value("id").toInt());
            if (!streams.contains(id) || props.value("media.class") != "Stream/Output/Audio" ||
                props.value("node.dont-move").toVariant().toBool() ||
                props.value("node.name").toString().startsWith("open-icue."))
                continue;
            change(id, "target.object", FILTER_INPUT_NAME, "Spa:String",
                   props.value("object.serial").toInteger());
        }
    } catch (...) {
        deactivate();
        throw;
    }
}
void HeadsetAudioRoute::deactivate() {
    if (state_->changes.isEmpty())
        return;
    const auto graph = backend_->graph();
    QStringList failures;
    for (auto it = state_->changes.crbegin(); it != state_->changes.crend(); ++it) {
        const auto current = metadata(graph, it->subject, it->key);
        if (jsonText(current.value("value")) != it->value)
            continue;
        if (it->subject) {
            bool isSameStream = false;
            for (const auto& entry : graph)
                if (entry.toObject().value("id").toInteger() == it->subject &&
                    properties(entry).value("object.serial").toInteger() == it->nodeSerial)
                    isSameStream = true;
            if (!isSameStream)
                continue;
        }
        try {
            if (it->previous.isEmpty())
                backend_->deleteMetadata(it->subject, it->key);
            else
                backend_->setMetadata(it->subject, it->key, jsonText(it->previous.value("value")),
                                      it->previous.value("type").toString());
        } catch (const std::exception& error) {
            failures.append(QString::fromUtf8(error.what()));
        }
    }
    if (!failures.isEmpty())
        throw std::runtime_error("Cannot restore the previous headset audio route");
    state_->changes.clear();
}
} // namespace open_icue
