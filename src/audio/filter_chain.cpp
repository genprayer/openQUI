#include "filter_chain.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
#include <stdexcept>

namespace open_icue {
namespace {
QByteArray spa(const QJsonValue& value) {
    if (value.isObject()) {
        QByteArray bytes = "{\n";
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            bytes += spa(it.key()) + " = " + spa(it.value()) + "\n";
        return bytes + "}";
    }
    if (value.isArray()) {
        QByteArray bytes = "[ ";
        for (const auto& item : value.toArray())
            bytes += spa(item) + " ";
        return bytes + "]";
    }
    const auto encoded = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return encoded.mid(1, encoded.size() - 2);
}
} // namespace

void validateEq(const QList<double>& gains) {
    if (gains.size() != EQ_BAND_COUNT)
        throw std::invalid_argument("Equalizer requires ten gains");
    for (const auto value : gains)
        if (!std::isfinite(value) || value < -12 || value > 12 ||
            std::abs(std::round(value * 2) - value * 2) > 1e-8)
            throw std::invalid_argument(
                "EQ gains require finite 0.5 dB steps between -12 and +12 dB");
}

QString ensureKemarHrir(const QString& directory) {
    QFile source(":/app/audio/kemar-7.1.wav");
    if (!source.open(QIODevice::ReadOnly))
        throw std::runtime_error("Embedded KEMAR HRIR is unavailable");
    const auto bytes = source.readAll();
    if (QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() !=
        "0e53a0d54cf59ebba03ff2137b884791866cd5d9a036966171d7ec1f64937979")
        throw std::runtime_error("KEMAR HRIR integrity check failed");
    if (!QDir().mkpath(directory))
        throw std::runtime_error("Cannot create audio state directory");
    const auto path = QDir(directory).absoluteFilePath("kemar-7.1.wav");
    if (QFileInfo(path).isSymLink())
        throw std::runtime_error("HRIR state file must not be a symbolic link");
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == bytes)
        return path;
    current.close();
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() ||
        !output.commit())
        throw std::runtime_error("Cannot export the KEMAR HRIR bank");
    return path;
}

QByteArray equalizerConfig(const QList<double>& gains, const QString& target,
                           const QString& hrirPath) {
    validateEq(gains);
    if (target.isEmpty() || target.contains(QChar(0)) || target.startsWith("open-icue."))
        throw std::invalid_argument("A physical PipeWire target is required");
    QJsonArray nodes, links;
    const bool isSurround = !hrirPath.isEmpty();
    if (isSurround && (!QFileInfo(hrirPath).isAbsolute() || !QFileInfo(hrirPath).isReadable()))
        throw std::invalid_argument("An accessible absolute KEMAR HRIR path is required");
    double positiveGain = 0;
    for (const auto gain : gains)
        positiveGain += std::max(0.0, gain);
    for (const auto& side : {QString("L"), QString("R")}) {
        QString previous = "pre" + side;
        nodes.append(QJsonObject{{"type", "builtin"},
                                 {"name", previous},
                                 {"label", "linear"},
                                 {"control", QJsonObject{{"Mult", std::pow(10, -positiveGain / 20) *
                                                                      (isSurround ? 0.125 : 1.0)},
                                                         {"Add", 0}}}});
        for (int index = 0; index < EQ_BAND_COUNT; ++index) {
            const auto name = "eq" + side + QString::number(index);
            nodes.append(QJsonObject{{"type", "builtin"},
                                     {"name", name},
                                     {"label", "bq_peaking"},
                                     {"control", QJsonObject{{"Freq", EQ_FREQUENCIES[index]},
                                                             {"Q", 1.4},
                                                             {"Gain", gains[index]}}}});
            links.append(QJsonObject{{"output", previous + ":Out"}, {"input", name + ":In"}});
            previous = name;
        }
    }
    const QJsonArray positions{"FL", "FR"};
    QJsonArray inputs{"preL:In", "preR:In"};
    const QJsonArray surroundPositions{"FL", "FR", "FC", "LFE", "RL", "RR", "SL", "SR"};
    if (isSurround) {
        inputs = {};
        for (const auto& side : {QString("L"), QString("R")})
            nodes.append(
                QJsonObject{{"type", "builtin"}, {"name", "mix" + side}, {"label", "mixer"}});
        nodes.append(
            QJsonObject{{"type", "builtin"},
                        {"name", "lfeLowPass"},
                        {"label", "bq_lowpass"},
                        {"control", QJsonObject{{"Freq", 120}, {"Q", 0.707}, {"Gain", 0}}}});
        for (int index = 0; index < surroundPositions.size(); ++index) {
            const auto channel = surroundPositions[index].toString();
            nodes.append(QJsonObject{{"type", "builtin"}, {"name", channel}, {"label", "copy"}});
            inputs.append(channel + ":In");
            const auto source = channel == "LFE" ? "lfeLowPass:Out" : channel + ":Out";
            if (channel == "LFE")
                links.append(QJsonObject{{"output", channel + ":Out"}, {"input", "lfeLowPass:In"}});
            for (int ear = 0; ear < 2; ++ear) {
                const QString side = ear ? "R" : "L";
                const auto name = "hrir" + channel + side;
                nodes.append(QJsonObject{
                    {"type", "builtin"},
                    {"name", name},
                    {"label", "convolver"},
                    {"config", QJsonObject{{"filename", hrirPath}, {"channel", index * 2 + ear}}}});
                links.append(QJsonObject{{"output", source}, {"input", name + ":In"}});
                links.append(
                    QJsonObject{{"output", name + ":Out"},
                                {"input", "mix" + side + ":In " + QString::number(index + 1)}});
            }
        }
        links.append(QJsonObject{{"output", "mixL:Out"}, {"input", "preL:In"}});
        links.append(QJsonObject{{"output", "mixR:Out"}, {"input", "preR:In"}});
    }
    const QJsonObject args{
        {"node.description", isSurround ? "OpenQUI 7.1 Surround" : "OpenQUI Equalizer"},
        {"media.name", "OpenQUI DSP"},
        {"filter.graph", QJsonObject{{"nodes", nodes},
                                     {"links", links},
                                     {"inputs", inputs},
                                     {"outputs", QJsonArray{"eqL9:Out", "eqR9:Out"}}}},
        {"capture.props",
         QJsonObject{{"node.name", "open-icue.input"},
                     {"media.class", "Audio/Sink"},
                     {"audio.channels", isSurround ? 8 : 2},
                     {"audio.position", isSurround ? surroundPositions : positions},
                     {"priority.session", 0}}},
        {"playback.props", QJsonObject{{"node.name", "open-icue.output"},
                                       {"node.passive", true},
                                       {"target.object", target},
                                       {"node.dont-fallback", true},
                                       {"audio.channels", 2},
                                       {"audio.position", positions}}}};
    const QJsonObject config{
        {"context.properties",
         QJsonObject{{"core.daemon", false}, {"core.name", "open-icue-filter"}}},
        {"context.spa-libs", QJsonObject{{"audio.convert.*", "audioconvert/libspa-audioconvert"}}},
        {"context.modules",
         QJsonArray{QJsonObject{{"name", "libpipewire-module-protocol-native"}},
                    QJsonObject{{"name", "libpipewire-module-client-node"}},
                    QJsonObject{{"name", "libpipewire-module-adapter"}},
                    QJsonObject{{"name", "libpipewire-module-filter-chain"}, {"args", args}}}}};
    QByteArray bytes;
    for (auto it = config.begin(); it != config.end(); ++it)
        bytes += spa(it.key()) + " = " + spa(it.value()) + "\n";
    return bytes;
}
} // namespace open_icue
