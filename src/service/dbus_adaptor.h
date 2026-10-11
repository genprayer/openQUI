#pragma once
#include "gui/application_model.h"
#include <QDBusAbstractAdaptor>
#include <QDBusVariant>

namespace open_icue {
/** Session D-Bus wire adapter; all commands execute on the model's Qt thread. */
class DbusAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.cyberdeck.icue")
  public:
    explicit DbusAdaptor(ApplicationModel* model);
  public slots:
    QString GetState() const;
    QString ListDevices() const;
    QString selectDevice(const QString& id);
    QString selectProfile(const QString& id);
    QString createProfile(const QString& name);
    QString renameProfile(const QString& id, const QString& name);
    QString removeProfile(const QString& id);
    QString importProfile(const QString& path);
    QString setDocumentValue(const QString& key, const QDBusVariant& value);
    QString setEqGain(int index, double gain);
    QString selectEqPreset(int index);
    QString addEqPreset(const QString& name);
    QString removeEqPreset(int index);
    QString resetEqPreset();
    QString addDpiPreset(const QString& name);
    QString selectDpiPreset(int index);
    QString removeDpiPreset(int index);
    QString resetDpiPreset();
    QString selectDpiStage(int index);
    QString setDpiStage(int index, const QString& field, const QDBusVariant& value);
    QString setTiltAngle(int index, int angle);
    QString setMixerControl(const QString& control, int percent, bool isMuted);
    QString startAudio(const QString& target);
    QString stopAudio();
    QString applyHardware();
    QString setLightingEditorActive(bool isActive);
    QString toggleSurround();
    QString applyDpiSettings();
    QString pairDarkstar();
    QString probeHardware();
    QString stopHardwareLighting();
    QString refresh();
    bool exportEqualizer(const QString& path, const QString& target);
  signals:
    void stateChanged();
    void noticeChanged();

  private:
    QString invoke(const std::function<void()>& operation);
    ApplicationModel* model_;
};
} // namespace open_icue
