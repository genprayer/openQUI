#pragma once
#include <QByteArray>
#include <QString>
#include <QVariantMap>

namespace open_icue {
struct CerealProfile {
    QByteArray bytes;
    QString name;
    QVariantMap metadata;
    QVariantMap devices;
};
/** Convert supported Cereal lighting while preserving source bytes and unsupported blocks. */
[[nodiscard]] CerealProfile readCerealProfile(const QString& path);
} // namespace open_icue
