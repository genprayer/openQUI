#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <functional>

namespace open_icue {

/** Atomic, process-locked profile documents; no physical device state is inferred. */
class Workspace {
  public:
    explicit Workspace(QString directory);
    [[nodiscard]] QVariantMap snapshot() const;
    [[nodiscard]] QVariantMap device(const QString& modelId, const QVariantMap& defaults) const;
    void updateDevice(const QString& modelId, const QString& key, const QVariant& value,
                      const QVariantMap& defaults);
    void updateDeviceFields(const QString& modelId, const QVariantMap& fields,
                            const QVariantMap& defaults);
    void createProfile(const QString& name);
    void selectProfile(const QString& id);
    void renameProfile(const QString& id, const QString& name);
    /** Copy every device document into an independent profile, preserving import provenance. */
    void duplicateProfile(const QString& id, const QString& name);
    void removeProfile(const QString& id);
    void importProfile(const QString& path);
    [[nodiscard]] QString directory() const {
        return directory_;
    }

  private:
    [[nodiscard]] QJsonObject load() const;
    void change(const std::function<void(QJsonObject&)>& operation);
    QString directory_;
};

} // namespace open_icue
