#pragma once
#include "audio/routing.h"
#include "core/workspace.h"
#include "devices/catalog.h"
#include "devices/hardware_backend.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace open_icue {
/** Native Qt controller for recovered device metadata, profile editing, and Linux audio. */
class ApplicationModel : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.cyberdeck.icue")
    Q_PROPERTY(QVariantList devices READ devices NOTIFY stateChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY stateChanged)
    Q_PROPERTY(QVariantList navigation READ navigation NOTIFY stateChanged)
    Q_PROPERTY(QVariantList keys READ keys NOTIFY stateChanged)
    Q_PROPERTY(QVariantList lightingKeys READ lightingKeys NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap assignmentKeyboard READ assignmentKeyboard CONSTANT)
    Q_PROPERTY(QVariantList lightingTypes READ lightingTypes NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap device READ device NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap document READ document NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap mixer READ mixer NOTIFY stateChanged)
    Q_PROPERTY(QVariantList audioTargets READ audioTargets NOTIFY stateChanged)
    Q_PROPERTY(QString audioTarget READ audioTarget NOTIFY stateChanged)
    Q_PROPERTY(QVariantList usbInventory READ usbInventory NOTIFY stateChanged)
    Q_PROPERTY(bool isPairingImplemented READ isPairingImplemented NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap hardwareState READ hardwareState NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap lightingState READ lightingState NOTIFY lightingStateChanged)
    Q_PROPERTY(
        QVariantMap hardwarePreviewColors READ hardwarePreviewColors NOTIFY lightingStateChanged)
    Q_PROPERTY(bool isHardwareBusy READ isHardwareBusy NOTIFY stateChanged)
    Q_PROPERTY(bool isHardwareLightingActive READ isHardwareLightingActive NOTIFY stateChanged)
    Q_PROPERTY(bool canApplyDpi READ canApplyDpi NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap dpiState READ dpiState NOTIFY stateChanged)
    Q_PROPERTY(QString selectedModelId READ selectedModelId NOTIFY stateChanged)
    Q_PROPERTY(QString activeProfileId READ activeProfileId NOTIFY stateChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(bool isAudioClientRunning READ isAudioClientRunning NOTIFY stateChanged)
  public:
    explicit ApplicationModel(const QString& stateDirectory, QObject* parent = nullptr);
    /** Stop only this controller's filter process during normal shutdown. */
    ~ApplicationModel() override;
    [[nodiscard]] QVariantList devices() const;
    [[nodiscard]] QVariantList profiles() const;
    [[nodiscard]] QVariantList navigation() const;
    [[nodiscard]] QVariantList keys() const;
    [[nodiscard]] QVariantList lightingKeys() const;
    [[nodiscard]] QVariantMap assignmentKeyboard() const {
        return catalog_.assignmentKeyboard();
    }
    [[nodiscard]] QVariantList lightingTypes() const;
    [[nodiscard]] QVariantMap device() const;
    /** Physical output of this application's current filter client. */
    [[nodiscard]] QString audioTarget() const {
        return audioTarget_;
    }
    [[nodiscard]] QVariantMap document() const;
    [[nodiscard]] QVariantMap mixer() const {
        return mixer_;
    }
    [[nodiscard]] QVariantList audioTargets() const;
    /** USB interfaces belonging to the selected device and its supported receiver. */
    [[nodiscard]] QVariantList usbInventory() const;
    [[nodiscard]] bool isPairingImplemented() const {
        return hardware_ &&
               (selectedModelId_ == "darkstar_wireless" ||
                selectedModelId_ == "slipstream_wireless_dongle_ss") &&
               hardwareState_.value("canPairDarkstar").toBool();
    }
    [[nodiscard]] QVariantMap hardwareState() const {
        return hardwareState_;
    }
    [[nodiscard]] QVariantMap lightingState() const {
        return lightingState_;
    }
    /** Rendered colors of the acknowledged outgoing frame, not physical LED readback. */
    [[nodiscard]] QVariantMap hardwarePreviewColors() const {
        return hardwarePreviewColors_;
    }
    [[nodiscard]] bool isHardwareBusy() const {
        return isHardwarePending_;
    }
    [[nodiscard]] bool isHardwareLightingActive() const {
        return !lightingModel_.isEmpty();
    }
    /** Opt in to real HID access at the application entrypoint, never in model-only tests. */
    void enableHardware(std::shared_ptr<HardwareBackend> injectedBackend = {});
    /** Active software DPI requires one verified wired Darkstar command interface. */
    [[nodiscard]] bool canApplyDpi() const {
        return hardware_ && selectedModelId_ == "darkstar_wireless" &&
               hardwareState_.value("canApplyDpi").toBool();
    }
    /** Last acknowledged DPI transaction, separately from edits to the local preset. */
    [[nodiscard]] QVariantMap dpiState() const {
        return dpiState_;
    }
    /** Identify the selected asset mode without implying hardware support. */
    [[nodiscard]] bool isUsingLocalCorsairResources() const {
        return catalog_.isUsingLocalCorsairResources();
    }
    [[nodiscard]] QString selectedModelId() const {
        return selectedModelId_;
    }
    [[nodiscard]] QString activeProfileId() const;
    [[nodiscard]] QString notice() const;
    /** Refresh translated presentation fields without changing IDs, profiles or hardware. */
    void retranslate();
    /** Process liveness only; does not prove routing or audible hardware output. */
    [[nodiscard]] bool isAudioClientRunning() const {
        return filter_.state() == QProcess::Running;
    }
    [[nodiscard]] QVariantMap lastOperation() const {
        return lastOperation_;
    }
    /** Prefer a compiled app-owned SVG; keep the original source as fallback. */
    Q_INVOKABLE QString iconSource(const QString& category, const QString& id,
                                   const QString& fallback) const;
  public slots:
    void selectDevice(const QString& id);
    void selectProfile(const QString& id);
    void createProfile(const QString& name);
    void renameProfile(const QString& id, const QString& name);
    void removeProfile(const QString& id);
    void importProfile(const QString& path);
    void setDocumentValue(const QString& key, const QVariant& value);
    void setEqGain(int index, double gain);
    void selectEqPreset(int index);
    void addEqPreset(const QString& name);
    void removeEqPreset(int index);
    void resetEqPreset();
    void addDpiPreset(const QString& name);
    void selectDpiPreset(int index);
    void removeDpiPreset(int index);
    void resetDpiPreset();
    void selectDpiStage(int index);
    void setDpiStage(int index, const QString& field, const QVariant& value);
    void setTiltAngle(int index, int angle);
    void setMixerControl(const QString& control, int percent, bool isMuted);
    void startAudio(const QString& target);
    /** Toggle Stereo/7.1 immediately on the selected Virtuoso output. */
    void toggleSurround();
    void stopAudio();
    void applyHardware();
    /** Live RGB edits are sent only while the selected lighting editor is active. */
    void setLightingEditorActive(bool isActive);
    /** Apply the selected preset's active value; firmware stage storage is separate. */
    void applyDpiSettings();
    void probeHardware();
    void pairDarkstar();
    void stopHardwareLighting();
    void refresh();
    QString GetState() const;
    QString ListDevices() const;
    bool exportEqualizer(const QString& path, const QString& target);
  signals:
    void stateChanged();
    void noticeChanged();
    void lightingStateChanged();

  private:
    void perform(const std::function<void()>& operation, const QString& success = {});
    void setNotice(const QString& text, const QStringList& arguments = {});
    void persist(const QString& key, const QVariant& value);
    void persistFields(const QVariantMap& fields);
    void validateLightingLayers(const QVariantList& layers) const;
    void validateAssignments(const QVariantList& assignments) const;
    void scheduleHardware(const QString& action, const std::function<QVariantMap()>& operation);
    void requestLiveLighting();
    void synchronizeLighting();
    [[nodiscard]] QString selectedUsbPath() const;
    [[nodiscard]] QList<double> currentGains(const QString& modelId = {}) const;
    void startAudioForDevice(const QString& target, const QString& modelId);
    void playSurroundPrompt(bool isEnabled);
    [[nodiscard]] QString headsetAudioTarget() const;
    Catalog catalog_;
    Workspace workspace_;
    QString selectedModelId_ = "darkstar_wireless";
    QVariantList usbDevices_, audioTargets_;
    QVariantMap mixer_, lastOperation_{{"isOk", true}, {"error", QString()}};
    QString notice_, audioTarget_, audioModelId_;
    QStringList noticeArguments_;
    QProcess filter_;
    QProcess voicePrompt_;
    HeadsetAudioRoute audioRoute_;
    QTimer scanTimer_, audioUpdateTimer_;
    std::shared_ptr<HardwareBackend> hardware_;
    QFutureWatcher<QVariantMap> hardwareFuture_;
    QTimer hardwareTimer_, lightingTimer_;
    QElapsedTimer lightingClock_;
    QVariantMap hardwareState_;
    QVariantMap lightingState_;
    QVariantMap hardwarePreviewColors_;
    QVariantMap voicePromptState_;
    QVariantMap dpiState_;
    QString hardwareAction_, lightingModel_;
    QString desiredLightingModel_, submittedLightingModel_;
    QVariantList appliedLayers_, appliedZones_;
    bool isHardwarePending_ = false;
    bool isLightingEditorActive_ = false, hasLightingUpdate_ = false;
    bool shouldReleaseControl_ = false;
    quint64 lightingRevision_ = 0, submittedLightingRevision_ = 0;
};
} // namespace open_icue
