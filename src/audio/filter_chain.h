#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

namespace open_icue {
inline constexpr int EQ_BAND_COUNT = 10;
inline constexpr int EQ_FREQUENCIES[EQ_BAND_COUNT] = {32,   64,   125,  250,  500,
                                                      1000, 2000, 4000, 8000, 16000};
/** Validate UI gains before persisting or building a DSP graph. */
void validateEq(const QList<double>& gains);
/** Native PipeWire filter-chain client configuration with an explicit physical target. */
[[nodiscard]] QByteArray equalizerConfig(const QList<double>& gains, const QString& target,
                                         const QString& hrirPath = {});
/** Export the attributed, embedded KEMAR HRIR bank atomically for PipeWire's convolver. */
QString ensureKemarHrir(const QString& directory);
} // namespace open_icue
