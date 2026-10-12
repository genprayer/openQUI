#pragma once
#include "keyboard_input.h"

#include <QColor>
#include <QMap>
#include <QPointF>
#include <QVariantList>

namespace open_icue {

/** Shared device geometry for independently implemented rain and ripple replay. */
class SpatialEffects {
  public:
    explicit SpatialEffects(const QVariantList& zones);
    [[nodiscard]] QColor rain(const QVariantMap& layer, qsizetype zoneIndex, double seconds) const;
    /** Play a ring from each recorded key, or a looping ring from the device center. */
    [[nodiscard]] QColor ripple(const QVariantMap& layer, qsizetype zoneIndex, double seconds,
                                const QMap<int, double>* presses,
                                const QList<LightingKeyPress>* history = nullptr) const;

  private:
    QList<QPointF> positions_;
    QStringList ids_;
    QMap<int, qsizetype> keyPositions_;
    QPointF minimum_, maximum_;
    double pitch_ = 1;
};

/** Validate spatial effect parameters before opening any HID or input endpoint. */
void validateSpatialEffect(const QVariantMap& layer);

} // namespace open_icue
