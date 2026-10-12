#include "workspace.h"
#include "profiles/cereal.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>
#include <stdexcept>

namespace open_icue {
namespace {
constexpr qint64 MAX_WORKSPACE_BYTES = 16 * 1024 * 1024;
constexpr int LOCK_TIMEOUT_MS = 2000;
constexpr int MAX_PROFILES = 256;
QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
QString checkedName(QString name) {
    name = name.trimmed();
    if (name.isEmpty() || name.size() > 128)
        throw std::invalid_argument("Profile name must contain 1–128 characters");
    return name;
}
void saveBytes(const QString& path, const QByteArray& data) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        throw std::runtime_error("Atomic file write failed");
}
int profileIndex(const QJsonArray& profiles, const QString& id) {
    for (qsizetype i = 0; i < profiles.size(); ++i)
        if (profiles.at(i).toObject().value("id").toString() == id)
            return static_cast<int>(i);
    throw std::invalid_argument("Profile does not exist");
}
} // namespace

Workspace::Workspace(QString directory) : directory_(std::move(directory)) {
    if (directory_.isEmpty() || !QDir().mkpath(directory_))
        throw std::runtime_error("Cannot create workspace directory");
    change([](QJsonObject&) {});
}

QJsonObject Workspace::load() const {
    QFile file(QDir(directory_).filePath("workspace.json"));
    if (!file.exists()) {
        const auto id = newId();
        return {{"version", 1},
                {"activeProfileId", id},
                {"profiles", QJsonArray{QJsonObject{
                                 {"id", id}, {"name", "Standard"}, {"devices", QJsonObject{}}}}}};
    }
    if (file.size() > MAX_WORKSPACE_BYTES || !file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot read workspace");
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(file.readAll(), &error);
    auto result = document.object();
    if (error.error != QJsonParseError::NoError || result.value("version").toInt() != 1 ||
        !result.value("profiles").isArray() || result.value("profiles").toArray().isEmpty())
        throw std::runtime_error("Workspace is invalid; preserved without overwriting");
    const auto profiles = result.value("profiles").toArray();
    profileIndex(profiles, result.value("activeProfileId").toString());
    return result;
}

void Workspace::change(const std::function<void(QJsonObject&)>& operation) {
    QLockFile lock(QDir(directory_).filePath("workspace.lock"));
    if (!lock.tryLock(LOCK_TIMEOUT_MS))
        throw std::runtime_error("Workspace is locked by another process");
    auto state = load();
    operation(state);
    const auto bytes = QJsonDocument(state).toJson();
    if (bytes.size() > MAX_WORKSPACE_BYTES)
        throw std::runtime_error("Workspace exceeds size limit");
    saveBytes(QDir(directory_).filePath("workspace.json"), bytes);
}

QVariantMap Workspace::snapshot() const {
    return load().toVariantMap();
}

QVariantMap Workspace::device(const QString& modelId, const QVariantMap& defaults) const {
    const auto state = load();
    const auto profiles = state.value("profiles").toArray();
    const auto profile =
        profiles.at(profileIndex(profiles, state.value("activeProfileId").toString())).toObject();
    const auto saved = profile.value("devices").toObject().value(modelId).toObject().toVariantMap();
    auto result = defaults;
    for (auto it = saved.cbegin(); it != saved.cend(); ++it)
        result[it.key()] = it.value();
    return result;
}

void Workspace::updateDevice(const QString& modelId, const QString& key, const QVariant& value,
                             const QVariantMap& defaults) {
    updateDeviceFields(modelId, {{key, value}}, defaults);
}

void Workspace::updateDeviceFields(const QString& modelId, const QVariantMap& fields,
                                   const QVariantMap& defaults) {
    change([&](QJsonObject& state) {
        auto profiles = state.value("profiles").toArray();
        const int index = profileIndex(profiles, state.value("activeProfileId").toString());
        auto profile = profiles.at(index).toObject();
        auto devices = profile.value("devices").toObject();
        auto data = devices.contains(modelId) ? devices.value(modelId).toObject()
                                              : QJsonObject::fromVariantMap(defaults);
        for (auto it = fields.cbegin(); it != fields.cend(); ++it)
            data[it.key()] = QJsonValue::fromVariant(it.value());
        devices[modelId] = data;
        profile["devices"] = devices;
        profiles[index] = profile;
        state["profiles"] = profiles;
    });
}

void Workspace::createProfile(const QString& name) {
    const auto validated = checkedName(name);
    change([&](QJsonObject& state) {
        auto profiles = state.value("profiles").toArray();
        if (profiles.size() >= MAX_PROFILES)
            throw std::runtime_error("Profile limit reached");
        const auto id = newId();
        profiles.append(QJsonObject{{"id", id}, {"name", validated}, {"devices", QJsonObject{}}});
        state["profiles"] = profiles;
        state["activeProfileId"] = id;
    });
}

void Workspace::selectProfile(const QString& id) {
    change([&](QJsonObject& state) {
        profileIndex(state.value("profiles").toArray(), id);
        state["activeProfileId"] = id;
    });
}

void Workspace::renameProfile(const QString& id, const QString& name) {
    const auto validated = checkedName(name);
    change([&](QJsonObject& state) {
        auto profiles = state.value("profiles").toArray();
        const int index = profileIndex(profiles, id);
        auto profile = profiles.at(index).toObject();
        profile["name"] = validated;
        profiles[index] = profile;
        state["profiles"] = profiles;
    });
}

void Workspace::duplicateProfile(const QString& id, const QString& name) {
    const auto validated = checkedName(name);
    change([&](QJsonObject& state) {
        auto profiles = state.value("profiles").toArray();
        if (profiles.size() >= MAX_PROFILES)
            throw std::runtime_error("Profile limit reached");
        auto copy = profiles.at(profileIndex(profiles, id)).toObject();
        const auto copyId = newId();
        copy["id"] = copyId;
        copy["name"] = validated;
        // Import identity belongs to the original; importing again must not select this copy.
        if (copy.contains("sourceSha256")) {
            copy["originSourceSha256"] = copy.take("sourceSha256");
        }
        profiles.append(copy);
        state["profiles"] = profiles;
        state["activeProfileId"] = copyId;
    });
}

void Workspace::removeProfile(const QString& id) {
    change([&](QJsonObject& state) {
        auto profiles = state.value("profiles").toArray();
        if (profiles.size() <= 1)
            throw std::invalid_argument("The last profile cannot be removed");
        profiles.removeAt(profileIndex(profiles, id));
        if (state.value("activeProfileId").toString() == id)
            state["activeProfileId"] = profiles.first().toObject().value("id");
        state["profiles"] = profiles;
    });
}

void Workspace::importProfile(const QString& path) {
    const auto parsed = readCerealProfile(path);
    const auto hash = QString::fromLatin1(
        QCryptographicHash::hash(parsed.bytes, QCryptographicHash::Sha256).toHex());
    const auto imports = QDir(directory_).filePath("imports");
    if (!QDir().mkpath(imports))
        throw std::runtime_error("Cannot create import directory");
    saveBytes(QDir(imports).filePath(hash + ".cueprofile"), parsed.bytes);
    change([&](QJsonObject& state) {
        auto profiles = state.value("profiles").toArray();
        for (qsizetype index = 0; index < profiles.size(); ++index) {
            auto profile = profiles.at(index).toObject();
            if (profile.value("sourceSha256").toString() == hash) {
                const auto previousMetadata = profile.value("sourceMetadata").toObject();
                const auto previousConversion = previousMetadata.value("conversion").toObject();
                const auto currentConversion = parsed.metadata.value("conversion").toMap();
                // Keep earlier conversions and their local edits. A newer converter imports
                // into a separate document; subsequent imports select that version.
                if (!previousConversion.isEmpty() && previousConversion.value("version").toInt() !=
                                                         currentConversion.value("version").toInt())
                    continue;
                // Upgrade earlier byte-only imports without replacing locally edited devices.
                if (profile.value("devices").toObject().isEmpty() &&
                    !profile.value("sourceMetadata").toObject().contains("conversion")) {
                    profile["devices"] = QJsonObject::fromVariantMap(parsed.devices);
                    profile["sourceMetadata"] = QJsonObject::fromVariantMap(parsed.metadata);
                    profiles[index] = profile;
                    state["profiles"] = profiles;
                }
                state["activeProfileId"] = profile.value("id");
                return;
            }
        }
        if (profiles.size() >= MAX_PROFILES)
            throw std::runtime_error("Profile limit reached");
        const auto id = newId();
        profiles.append(
            QJsonObject{{"id", id},
                        {"name", parsed.name},
                        {"sourceSha256", hash},
                        {"sourceMetadata", QJsonObject::fromVariantMap(parsed.metadata)},
                        {"devices", QJsonObject::fromVariantMap(parsed.devices)}});
        state["profiles"] = profiles;
        state["activeProfileId"] = id;
    });
}

} // namespace open_icue
