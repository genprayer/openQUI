#pragma once

#include <QJsonArray>
#include <QString>
#include <QVariantList>
#include <memory>

namespace open_icue {
/** Resolve physical PipeWire sinks through their ALSA card's USB sysfs ancestry. */
[[nodiscard]] QVariantList
audioTargetsFromGraph(const QJsonArray& graph,
                      const QString& soundClassRoot = QStringLiteral("/sys/class/sound"));
/** Read the current PipeWire graph; the caller decides which selected device is eligible. */
[[nodiscard]] QVariantList queryAudioTargets();
/** Restrict discovered sinks to an exact supported Virtuoso USB device or its receiver. */
[[nodiscard]] QVariantList headsetAudioTargetsForUsb(const QVariantList& targets,
                                                     const QString& usbSysPath);

/** Injectable PipeWire metadata operations, used by the scoped route and its tests. */
class AudioRoutingBackend {
  public:
    virtual ~AudioRoutingBackend() = default;
    [[nodiscard]] virtual QJsonArray graph() = 0;
    virtual void setMetadata(quint32 subject, const QString& key, const QString& value,
                             const QString& type) = 0;
    virtual void deleteMetadata(quint32 subject, const QString& key) = 0;
};

/** Routes the selected headset through our DSP and restores only changes we still own. */
class HeadsetAudioRoute {
  public:
    explicit HeadsetAudioRoute(std::shared_ptr<AudioRoutingBackend> backend = {},
                               QString soundClassRoot = QStringLiteral("/sys/class/sound"));
    ~HeadsetAudioRoute();
    HeadsetAudioRoute(const HeadsetAudioRoute&) = delete;
    HeadsetAudioRoute& operator=(const HeadsetAudioRoute&) = delete;

    /** Call after starting the filter; filterPid binds the virtual sink to that process. */
    void activate(const QString& usbSysPath, const QString& physicalTarget, qint64 filterPid);
    /** Call before terminating or replacing the filter process. */
    void deactivate();
    [[nodiscard]] bool isActive() const;

  private:
    struct State;
    std::unique_ptr<State> state_;
    std::shared_ptr<AudioRoutingBackend> backend_;
    QString soundClassRoot_;
};
} // namespace open_icue
