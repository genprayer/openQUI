#pragma once
#include <QQuickImageProvider>

namespace open_icue {
/** Render app-owned SVG icons after replacing explicit semantic palette roles. */
class IconImages final : public QQuickImageProvider {
  public:
    IconImages() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
} // namespace open_icue
