#include "device_images.h"
#include <QImage>
#include <QUrl>
#include <algorithm>

namespace open_icue {
DeviceImages::DeviceImages() : QQuickImageProvider(QQuickImageProvider::Image) {}
QImage DeviceImages::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    QImage result;
    if (const auto* cached = cache_.object(id))
        result = *cached;
    else {
        const auto modelId = id.section('/', 0, 0);
        bool isValid = false;
        const int view = id.section('/', 1, 1).toInt(&isValid);
        if (!isValid || view < 0)
            return {};
        for (const auto& entry : catalog_.models()) {
            const auto model = entry.toMap();
            if (model.value("id").toString() != modelId)
                continue;
            const auto bodies = model.value("bodies").toStringList();
            const auto masks = model.value("hullMasks").toList();
            if (view >= bodies.size())
                return {};
            const auto url = QUrl(bodies.at(view));
            result.load(":" + url.path());
            result = result.convertToFormat(QImage::Format_ARGB32);
            if (view < masks.size()) {
                const QImage mask(masks.at(view).toString());
                if (mask.isNull() || mask.size() != result.size())
                    return {};
                const auto rgba = mask.convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < result.height(); ++y) {
                    auto* pixels = reinterpret_cast<QRgb*>(result.scanLine(y));
                    const auto* alpha = reinterpret_cast<const QRgb*>(rgba.constScanLine(y));
                    for (int x = 0; x < result.width(); ++x)
                        pixels[x] = qRgba(qRed(pixels[x]), qGreen(pixels[x]), qBlue(pixels[x]),
                                          qAlpha(alpha[x]));
                }
                if (id.section('/', 2, 2) == "thumbnail") {
                    const auto clip = catalog_.definition(modelId)
                                          .value("ImagesClipRects")
                                          .toObject()
                                          .value(masks.at(view).toString())
                                          .toObject();
                    const QRect rectangle(clip.value("x").toInt(), clip.value("y").toInt(),
                                          clip.value("width").toInt(),
                                          clip.value("height").toInt());
                    const auto bounded = rectangle.intersected(result.rect());
                    if (!bounded.isEmpty())
                        result = result.copy(bounded);
                }
            }
            break;
        }
        if (!result.isNull())
            cache_.insert(id, new QImage(result), static_cast<int>(result.sizeInBytes() / 1024));
    }
    if (size)
        *size = result.size();
    return requestedSize.isValid()
               ? result.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
               : result;
}
} // namespace open_icue
