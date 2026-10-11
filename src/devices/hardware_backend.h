#pragma once
#include "hid_transport.h"
#include "keyboard_input.h"
#include <QColor>
#include <QVariantList>
#include <map>
#include <memory>
#include <optional>

namespace open_icue {
/** Native, serialized hardware operations. Keep all calls on the controller's worker. */
class HardwareBackend {
  public:
    using EndpointDiscovery = std::function<QList<HidEndpoint>()>;
    using SessionFactory = std::function<std::unique_ptr<HidSession>(const HidEndpoint&)>;
    explicit HardwareBackend(QString stateDirectory,
                             EndpointDiscovery discovery = discoverCommandEndpoints,
                             SessionFactory sessions = {},
                             std::shared_ptr<KeyboardLightingInput> keyboardInput = {});
    ~HardwareBackend();
    /** Read identity, battery and radio slots without changing firmware control mode. */
    QVariantMap probe();
    /** Restore only modes recorded by an interrupted pairing, checking serials and identity. */
    QString restoreInterruptedPairing();
    /** Pair only a wired Darkstar with the separately selected 1bdc receiver. */
    QVariantMap pairDarkstar();
    /** Send a lighting frame; unsupported zone maps fail before taking control. */
    QVariantMap applyLighting(const QString& model, const QVariantList& layers,
                              const QVariantList& zones, double seconds = 0);
    /** Apply and read back Darkstar's active software DPI; on-board stages are untouched. */
    QVariantMap applyDpi(const QString& model, quint32 x, quint32 y);
    /** Release this application's shared software control for lighting and active DPI. */
    void stopLighting();
    /** Restore owned Bragi modes with explicit error and readback reporting. */
    [[nodiscard]] QVariantMap releaseControl();

  private:
    HidSession& lightingSession(const HidEndpoint& endpoint, quint8 commandEndpoint = 8);
    QString stateDirectory_;
    EndpointDiscovery discovery_;
    std::shared_ptr<KeyboardLightingInput> keyboardInput_;
    SessionFactory sessions_;
    QString recoveryError_;
    std::map<QString, std::unique_ptr<HidSession>> lightingSessions_;
};
/** K70's Bragi LED slots: letters/digits follow USB usages; modifiers have dedicated slots. */
int k70LedIndex(const QString& id);
/** Render continuous layers and real per-key triggers into physical LED colors. */
QList<QRgb> lightingFrame(const QVariantList& layers, const QVariantList& zones, double seconds,
                          const QMap<int, double>* presses = nullptr);
/** Validate and evaluate an imported looping RGBA timeline; absent timelines return nullopt. */
[[nodiscard]] std::optional<QColor> lightingTimelineColor(const QVariantMap& layer, double seconds);
} // namespace open_icue
