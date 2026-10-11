#pragma once
#include "devices/catalog.h"
#include <QCache>
#include <QQuickImageProvider>

namespace open_icue {
/** Composite the original JPEG bodies with their original alpha hull masks. */
class DeviceImages final : public QQuickImageProvider {
  public:
    DeviceImages();
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

  private:
    Catalog catalog_;
    QCache<QString, QImage> cache_{32768};
};
} // namespace open_icue
