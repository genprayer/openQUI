#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVariantList>
#include <optional>

namespace open_icue {

/** Device metadata and original application schematics, with optional local RCC input. */
class Catalog {
  public:
    Catalog();
    /** True only when the build explicitly embeds locally supplied Corsair resources. */
    [[nodiscard]] bool isUsingLocalCorsairResources() const;
    [[nodiscard]] QVariantList models() const;
    [[nodiscard]] QJsonObject definition(const QString& id) const;
    [[nodiscard]] std::optional<QString> modelForUsb(quint16 vendor, quint16 product) const;
    [[nodiscard]] QVariantList navigation(const QString& id) const;
    [[nodiscard]] QVariantList keys(const QString& id) const;
    [[nodiscard]] QVariantList lightingKeys(const QString& id) const;
    /** Complete assignment keyboard layout; identifiers are independent of labels. */
    [[nodiscard]] QVariantMap assignmentKeyboard() const {
        return keyboardLayout_;
    }
    [[nodiscard]] QVariantList lightingTypes(const QString& id) const;
    [[nodiscard]] QVariantList factoryEqPresets() const;
    [[nodiscard]] QVariantMap initialDocument(const QString& id) const;

  private:
    [[nodiscard]] QVariantList layoutItems(const QString& id, const QString& type) const;
    QMap<QString, QJsonObject> definitions_;
    QStringList order_;
    QVariantMap keyboardLayout_;
    QVariantList defaultEqPresets_;
};

/** Keep registered RCC memory alive until process exit. */
void registerOriginalResources();

} // namespace open_icue
