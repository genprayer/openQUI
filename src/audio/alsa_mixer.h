#pragma once
#include <QString>
#include <QVariantMap>

namespace open_icue {
/** ALSA controls bound to the selected USB device's sysfs ancestry. */
[[nodiscard]] QVariantMap readHeadsetMixer(const QString& usbSysPath);
[[nodiscard]] QVariantMap setHeadsetControl(const QString& usbSysPath, const QString& control,
                                            int percent, bool isMuted);
} // namespace open_icue
