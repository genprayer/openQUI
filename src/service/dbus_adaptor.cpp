#include "dbus_adaptor.h"
#include <QDBusArgument>
#include <QJsonDocument>
#include <QJsonObject>
#include <stdexcept>

namespace open_icue {
namespace {
constexpr int MAX_VARIANT_DEPTH = 32;
QVariant unpack(const QVariant& input, int depth = 0) {
    if (depth >= MAX_VARIANT_DEPTH)
        throw std::invalid_argument("D-Bus value nesting exceeds limit");
    QVariant value = input;
    if (value.metaType().id() == qMetaTypeId<QDBusVariant>())
        return unpack(value.value<QDBusVariant>().variant(), depth + 1);
    if (value.metaType().id() == qMetaTypeId<QDBusArgument>()) {
        const auto argument = value.value<QDBusArgument>();
        if (argument.currentSignature() == "av")
            value = QVariant::fromValue(qdbus_cast<QVariantList>(argument));
        else if (argument.currentSignature() == "a{sv}")
            value = QVariant::fromValue(qdbus_cast<QVariantMap>(argument));
        else
            throw std::invalid_argument("Unsupported D-Bus container; use av or a{sv}");
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        auto list = value.toList();
        for (auto& item : list)
            item = unpack(item, depth + 1);
        return list;
    }
    if (value.metaType().id() == QMetaType::QVariantMap) {
        auto map = value.toMap();
        for (auto& item : map)
            item = unpack(item, depth + 1);
        return map;
    }
    return value;
}
} // namespace
DbusAdaptor::DbusAdaptor(ApplicationModel* model) : QDBusAbstractAdaptor(model), model_(model) {
    setAutoRelaySignals(true);
}
QString DbusAdaptor::GetState() const {
    return model_->GetState();
}
QString DbusAdaptor::ListDevices() const {
    return model_->ListDevices();
}
QString DbusAdaptor::invoke(const std::function<void()>& operation) {
    try {
        operation();
        const auto result = QJsonObject::fromVariantMap(model_->lastOperation());
        return QString::fromUtf8(
            QJsonDocument(
                QJsonObject{
                    {"schemaVersion", 1},
                    {"isOk", result.value("isOk")},
                    {"isPending", result.value("isPending").toBool()},
                    {"error", result.value("error")},
                    {"state", QJsonDocument::fromJson(model_->GetState().toUtf8()).object()}})
                .toJson(QJsonDocument::Compact));
    } catch (const std::exception& error) {
        return QString::fromUtf8(
            QJsonDocument(QJsonObject{{"schemaVersion", 1},
                                      {"isOk", false},
                                      {"error", QString::fromUtf8(error.what())}})
                .toJson(QJsonDocument::Compact));
    }
}
QString DbusAdaptor::selectDevice(const QString& id) {
    return invoke([&] { model_->selectDevice(id); });
}
QString DbusAdaptor::selectProfile(const QString& id) {
    return invoke([&] { model_->selectProfile(id); });
}
QString DbusAdaptor::createProfile(const QString& name) {
    return invoke([&] { model_->createProfile(name); });
}
QString DbusAdaptor::renameProfile(const QString& id, const QString& name) {
    return invoke([&] { model_->renameProfile(id, name); });
}
QString DbusAdaptor::removeProfile(const QString& id) {
    return invoke([&] { model_->removeProfile(id); });
}
QString DbusAdaptor::importProfile(const QString& path) {
    return invoke([&] { model_->importProfile(path); });
}
QString DbusAdaptor::setDocumentValue(const QString& key, const QDBusVariant& value) {
    return invoke([&] { model_->setDocumentValue(key, unpack(value.variant())); });
}
QString DbusAdaptor::setEqGain(int index, double gain) {
    return invoke([&] { model_->setEqGain(index, gain); });
}
QString DbusAdaptor::selectEqPreset(int index) {
    return invoke([&] { model_->selectEqPreset(index); });
}
QString DbusAdaptor::addEqPreset(const QString& name) {
    return invoke([&] { model_->addEqPreset(name); });
}
QString DbusAdaptor::removeEqPreset(int index) {
    return invoke([&] { model_->removeEqPreset(index); });
}
QString DbusAdaptor::resetEqPreset() {
    return invoke([&] { model_->resetEqPreset(); });
}
QString DbusAdaptor::addDpiPreset(const QString& name) {
    return invoke([&] { model_->addDpiPreset(name); });
}
QString DbusAdaptor::selectDpiPreset(int index) {
    return invoke([&] { model_->selectDpiPreset(index); });
}
QString DbusAdaptor::removeDpiPreset(int index) {
    return invoke([&] { model_->removeDpiPreset(index); });
}
QString DbusAdaptor::resetDpiPreset() {
    return invoke([&] { model_->resetDpiPreset(); });
}
QString DbusAdaptor::selectDpiStage(int index) {
    return invoke([&] { model_->selectDpiStage(index); });
}
QString DbusAdaptor::setDpiStage(int index, const QString& field, const QDBusVariant& value) {
    return invoke([&] { model_->setDpiStage(index, field, unpack(value.variant())); });
}
QString DbusAdaptor::setTiltAngle(int index, int angle) {
    return invoke([&] { model_->setTiltAngle(index, angle); });
}
QString DbusAdaptor::setMixerControl(const QString& control, int percent, bool isMuted) {
    return invoke([&] { model_->setMixerControl(control, percent, isMuted); });
}
QString DbusAdaptor::startAudio(const QString& target) {
    return invoke([&] { model_->startAudio(target); });
}
QString DbusAdaptor::stopAudio() {
    return invoke([&] {
        model_->stopAudio();
        model_->refresh();
    });
}
QString DbusAdaptor::applyHardware() {
    return invoke([&] { model_->applyHardware(); });
}
QString DbusAdaptor::setLightingEditorActive(bool isActive) {
    return invoke([&] { model_->setLightingEditorActive(isActive); });
}
QString DbusAdaptor::toggleSurround() {
    return invoke([&] { model_->toggleSurround(); });
}
QString DbusAdaptor::applyDpiSettings() {
    return invoke([&] { model_->applyDpiSettings(); });
}
QString DbusAdaptor::pairDarkstar() {
    return invoke([&] { model_->pairDarkstar(); });
}
QString DbusAdaptor::probeHardware() {
    return invoke([&] { model_->probeHardware(); });
}
QString DbusAdaptor::stopHardwareLighting() {
    return invoke([&] { model_->stopHardwareLighting(); });
}
QString DbusAdaptor::refresh() {
    return invoke([&] { model_->refresh(); });
}
bool DbusAdaptor::exportEqualizer(const QString& path, const QString& target) {
    return model_->exportEqualizer(path, target);
}
} // namespace open_icue
