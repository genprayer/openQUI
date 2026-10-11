#include "device_selection.h"
#include "devices/hardware_backend.h"

#include <QGuiApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleHints>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace open_icue {
namespace {
constexpr int PREVIEW_INTERVAL_MS = 50;
constexpr int MASK_SELECTION_ALPHA = 24;
constexpr double GLOW_ALPHA = 0.40;

QRegion maskRegion(const QImage& image) {
    QRegion result;
    for (int y = 0; y < image.height(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        int start = -1;
        for (int x = 0; x <= image.width(); ++x) {
            const bool hasAlpha = x < image.width() && qAlpha(pixels[x]) > MASK_SELECTION_ALPHA;
            if (hasAlpha && start < 0)
                start = x;
            if (!hasAlpha && start >= 0) {
                result += QRect(start, y, x - start, 1);
                start = -1;
            }
        }
    }
    return result;
}
} // namespace

DeviceSelection::DeviceSelection(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    previewClock_.start();
    previewTimer_.setInterval(PREVIEW_INTERVAL_MS);
    connect(&previewTimer_, &QTimer::timeout, this, [this] {
        if (isVisible())
            update();
    });
}
void DeviceSelection::setEntries(const QVariantList& value) {
    if (entries_ == value)
        return;
    entries_ = value;
    rebuildContours();
    emit entriesChanged();
}
void DeviceSelection::setCanvasSize(QSizeF size) {
    if (size.width() <= 0 || size.height() <= 0 || canvasSize_ == size)
        return;
    canvasSize_ = size;
    update();
    emit geometryChanged();
}
void DeviceSelection::setViewIndex(int index) {
    if (viewIndex_ == index)
        return;
    viewIndex_ = index;
    rebuildContours();
    emit geometryChanged();
}
void DeviceSelection::setSelectedIds(const QStringList& value) {
    if (selectedIds_ == value)
        return;
    selectedIds_ = value;
    update();
    emit selectionChanged();
}
void DeviceSelection::setIsMultiSelection(bool value) {
    if (isMultiSelection_ == value)
        return;
    isMultiSelection_ = value;
    emit modeChanged();
}
void DeviceSelection::setPreviewLayers(const QVariantList& value) {
    if (previewLayers_ == value)
        return;
    previewLayers_ = value;
    if (value.isEmpty())
        previewTimer_.stop();
    else
        previewTimer_.start();
    update();
    emit previewChanged();
}
void DeviceSelection::setFrameColors(const QVariantMap& colors) {
    if (frameColors_ == colors)
        return;
    frameColors_ = colors;
    update();
    emit previewChanged();
}
void DeviceSelection::rebuildContours() {
    contours_.clear();
    for (const auto& entry : entries_) {
        const auto item = entry.toMap();
        const auto geometry = item.value("geometry").toList();
        // A missing view is intentionally absent; never reuse a different view's contour.
        if (viewIndex_ < 0 || viewIndex_ >= geometry.size())
            continue;
        const auto data = geometry.at(viewIndex_).toMap();
        Contour contour;
        contour.id = item.value("id").toString();
        contour.label = item.value("label").toString();
        const auto mask = data.value("mask").toString();
        if (!mask.isEmpty()) {
            const auto full = QImage(mask).convertToFormat(QImage::Format_ARGB32);
            if (full.isNull())
                continue;
            contour.alphaRegion = maskRegion(full);
            contour.maskBounds = contour.alphaRegion.boundingRect();
            contour.mask = full.copy(contour.maskBounds);
            if (contour.maskBounds.isEmpty())
                continue;
        } else {
            const auto shape = data.value("shape").toMap();
            if (shape.value("type") == "rectangle") {
                contour.path.addRoundedRect(
                    QRectF(shape.value("x").toDouble(), shape.value("y").toDouble(),
                           shape.value("width").toDouble(), shape.value("height").toDouble()),
                    shape.value("rx").toDouble(), shape.value("ry").toDouble());
            } else {
                QPolygonF polygon;
                for (const auto& point : data.value("points").toList()) {
                    const auto coordinates = point.toMap();
                    polygon.append(QPointF(coordinates.value("x").toDouble(),
                                           coordinates.value("y").toDouble()));
                }
                if (polygon.size() < 3)
                    continue;
                contour.path.addPolygon(polygon);
                contour.path.closeSubpath();
            }
            if (contour.path.boundingRect().isEmpty())
                continue;
        }
        contours_.append(std::move(contour));
    }
    update();
}
QPointF DeviceSelection::toCanvas(QPointF point) const {
    return QPointF(point.x() * canvasSize_.width() / std::max(width(), 1.0),
                   point.y() * canvasSize_.height() / std::max(height(), 1.0));
}
QRectF DeviceSelection::keyRect(const QString& id) const {
    for (const auto& contour : contours_) {
        if (contour.id != id)
            continue;
        const QRectF bounds =
            contour.mask.isNull() ? contour.path.boundingRect() : QRectF(contour.maskBounds);
        return QRectF(bounds.x() * width() / canvasSize_.width(),
                      bounds.y() * height() / canvasSize_.height(),
                      bounds.width() * width() / canvasSize_.width(),
                      bounds.height() * height() / canvasSize_.height());
    }
    return {};
}
QPointF DeviceSelection::keyCenter(const QString& id) const {
    for (const auto& contour : contours_) {
        if (contour.id != id)
            continue;
        QPointF point = contour.mask.isNull() ? contour.path.boundingRect().center()
                                              : contour.maskBounds.center();
        if (!contour.mask.isNull() && !contour.alphaRegion.contains(point.toPoint())) {
            double closest = std::numeric_limits<double>::max();
            const auto center = point;
            for (const auto& rect : contour.alphaRegion) {
                const auto candidate = QPointF(rect.center());
                const auto offset = candidate - center;
                const double distance = offset.x() * offset.x() + offset.y() * offset.y();
                if (distance < closest) {
                    closest = distance;
                    point = candidate;
                }
            }
        }
        return QPointF(point.x() * width() / canvasSize_.width(),
                       point.y() * height() / canvasSize_.height());
    }
    return QPointF(-1, -1);
}
QString DeviceSelection::hit(QPointF point) const {
    for (const auto& contour : contours_) {
        if (contour.mask.isNull() ? contour.path.contains(point)
                                  : contour.alphaRegion.contains(point.toPoint()))
            return contour.id;
    }
    return {};
}
QStringList DeviceSelection::rectangleIds() const {
    QStringList ids = isAdditive_ ? pressSelection_ : QStringList{};
    for (const auto& contour : contours_) {
        const bool isInside = contour.mask.isNull()
                                  ? contour.path.intersects(selectionRect_)
                                  : contour.alphaRegion.intersects(selectionRect_.toAlignedRect());
        if (isInside && !ids.contains(contour.id))
            ids.append(contour.id);
    }
    return ids;
}
void DeviceSelection::mousePressEvent(QMouseEvent* event) {
    isPressed_ = true;
    isDragging_ = false;
    pressPoint_ = event->position();
    pressSelection_ = selectedIds_;
    isAdditive_ = event->modifiers().testFlag(Qt::ShiftModifier) ||
                  event->modifiers().testFlag(Qt::ControlModifier);
    selectionRect_ = {};
    event->accept();
    emit gestureChanged();
}
void DeviceSelection::mouseMoveEvent(QMouseEvent* event) {
    if (!isPressed_ || !isMultiSelection_)
        return;
    if ((event->position() - pressPoint_).manhattanLength() >=
        QGuiApplication::styleHints()->startDragDistance())
        isDragging_ = true;
    if (!isDragging_)
        return;
    selectionRect_ = QRectF(toCanvas(pressPoint_), toCanvas(event->position())).normalized();
    emit selectionRequested(rectangleIds());
    update();
    event->accept();
    emit gestureChanged();
}
void DeviceSelection::mouseReleaseEvent(QMouseEvent* event) {
    if (!isPressed_)
        return;
    QStringList ids;
    if (isDragging_)
        ids = rectangleIds();
    else {
        const auto id = hit(toCanvas(event->position()));
        if (isMultiSelection_ && isAdditive_) {
            ids = pressSelection_;
            if (ids.contains(id))
                ids.removeAll(id);
            else if (!id.isEmpty())
                ids.append(id);
        } else if (!id.isEmpty())
            ids.append(id);
    }
    isPressed_ = false;
    isDragging_ = false;
    selectionRect_ = {};
    previewClock_.restart();
    emit selectionRequested(ids);
    emit gestureChanged();
    update();
    event->accept();
}
void DeviceSelection::mouseUngrabEvent() {
    isPressed_ = false;
    isDragging_ = false;
    selectionRect_ = {};
    emit gestureChanged();
    update();
}
void DeviceSelection::hoverMoveEvent(QHoverEvent* event) {
    const auto id = hit(toCanvas(event->position()));
    QString label;
    for (const auto& contour : contours_)
        if (contour.id == id)
            label = contour.label;
    if (label != hoveredLabel_) {
        hoveredLabel_ = label;
        emit hoverChanged();
    }
}
void DeviceSelection::hoverLeaveEvent(QHoverEvent*) {
    hoveredLabel_.clear();
    emit hoverChanged();
}
QColor DeviceSelection::colorFor(const Contour& contour) const {
    if (frameColors_.contains(contour.id))
        return QColor(frameColors_.value(contour.id).toString());
    QColor result(Qt::transparent);
    const double seconds = previewClock_.elapsed() / 1000.0;
    for (const auto& value : previewLayers_) {
        const auto layer = value.toMap();
        const auto zones = layer.value("zones").toStringList();
        if (!zones.contains("all") && !zones.contains(contour.id))
            continue;
        const auto colors = layer.value("colors").toList();
        if (colors.isEmpty())
            continue;
        const auto type = layer.value("type").toString().section("LightingTypeData", 0, 0);
        QColor color(colors.first().toString());
        const double duration = std::max(layer.value("duration", 1).toDouble(), 0.1);
        const double phase = seconds * (0.25 + layer.value("speed", 1).toDouble() * 0.5) / duration;
        const auto bounds =
            contour.mask.isNull() ? contour.path.boundingRect() : QRectF(contour.maskBounds);
        const int direction = layer.value("direction", 0).toInt();
        double position = direction >= 2 ? bounds.center().y() / canvasSize_.height()
                                         : bounds.center().x() / canvasSize_.width();
        if (direction % 2)
            position = 1 - position;
        const QStringList randomTypes{"ColorPulse", "ColorShift", "ColorWave", "Visor",
                                      "Rain",       "Type",       "Ripple"};
        const bool isRandom = layer.value("isRandom").toBool() && randomTypes.contains(type);
        if (isRandom)
            color = QColor::fromHsvF(std::fmod(phase * 0.15 + position, 1.0), 0.9, 1);
        std::optional<QColor> timeline;
        try {
            timeline = lightingTimelineColor(layer, seconds);
        } catch (const std::exception& error) {
            qWarning("Lighting preview: %s", error.what());
            continue;
        }
        if (timeline)
            color = *timeline;
        else if (type.startsWith("Rainbow") || type == "SpiralRainbow")
            color = QColor::fromHsvF(std::fmod(phase * 0.25 + position, 1.0), 0.9, 1);
        else if (!isRandom && (type == "Gradient" || type == "ColorShift" || type == "ColorWave")) {
            const double blend =
                (std::sin((phase + (type == "ColorShift" ? 0 : position)) * 2 * std::numbers::pi) +
                 1) /
                2;
            const double segment = blend * std::max(0, int(colors.size()) - 1);
            const int lower = std::min(int(segment), std::max(0, int(colors.size()) - 2));
            color = QColor(colors.at(lower).toString());
            const QColor next(colors.at(std::min(lower + 1, int(colors.size()) - 1)).toString());
            const double weight = segment - lower;
            color = QColor::fromRgbF(color.redF() * (1 - weight) + next.redF() * weight,
                                     color.greenF() * (1 - weight) + next.greenF() * weight,
                                     color.blueF() * (1 - weight) + next.blueF() * weight);
        }
        double alpha = color.alphaF() * layer.value("opacity", 100).toDouble() / 100.0;
        if (type == "ColorPulse" || type == "Wave" || type == "Visor" || type == "Rain")
            alpha *= 0.15 + 0.85 * (std::sin((phase + position) * 2 * std::numbers::pi) + 1) / 2;
        if (type == "Type" || type == "Ripple")
            continue;
        if (type == "Solid")
            alpha *= std::max(0.0, 1 - seconds / duration);
        // Local visualization only; no claim of matching the vendor's animation or DSP.
        const double previousAlpha = result.alphaF();
        const double combinedAlpha = alpha + previousAlpha * (1 - alpha);
        if (combinedAlpha > 0)
            result = QColor::fromRgbF(
                (color.redF() * alpha + result.redF() * previousAlpha * (1 - alpha)) /
                    combinedAlpha,
                (color.greenF() * alpha + result.greenF() * previousAlpha * (1 - alpha)) /
                    combinedAlpha,
                (color.blueF() * alpha + result.blueF() * previousAlpha * (1 - alpha)) /
                    combinedAlpha,
                combinedAlpha);
    }
    return result;
}
QColor DeviceSelection::previewColor(const QString& id) const {
    for (const auto& contour : contours_)
        if (contour.id == id)
            return colorFor(contour);
    return Qt::transparent;
}
void DeviceSelection::paint(QPainter* painter) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->scale(width() / canvasSize_.width(), height() / canvasSize_.height());
    const double pixelWidth = canvasSize_.width() / std::max(width(), 1.0);
    const auto selection = isDragging_ ? rectangleIds() : selectedIds_;
    for (const auto& contour : contours_) {
        const auto color = colorFor(contour);
        const bool isSelected = selection.contains(contour.id);
        if (!contour.mask.isNull()) {
            QImage tinted = contour.mask;
            QPainter tint(&tinted);
            tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tint.fillRect(tinted.rect(), color.alpha() ? color
                                         : isSelected  ? QColor("#e8e71c")
                                                       : QColor(Qt::transparent));
            tint.end();
            painter->setOpacity(isSelected ? 0.9 : 0.75);
            painter->drawImage(contour.maskBounds.topLeft(), tinted);
            painter->setOpacity(1);
            if (isSelected) {
                painter->setBrush(Qt::NoBrush);
                painter->setPen(QPen(QColor("#e8e71c"), pixelWidth));
                painter->drawRect(contour.maskBounds);
            }
            continue;
        }
        if (color.alpha()) {
            QColor glow = color;
            glow.setAlphaF(color.alphaF() * GLOW_ALPHA);
            painter->setPen(QPen(glow, 5 * pixelWidth));
            painter->setBrush(glow);
            painter->drawPath(contour.path);
        }
        if (isSelected) {
            painter->setPen(QPen(QColor("#fff465"), 1.6 * pixelWidth));
            painter->setBrush(QColor("#44e8e71c"));
            painter->drawPath(contour.path);
        }
    }
    if (isDragging_) {
        painter->setPen(QPen(QColor("#eeeeee"), pixelWidth, Qt::DashLine));
        painter->setBrush(QColor("#2267c5ff"));
        painter->drawRect(selectionRect_);
    }
}

} // namespace open_icue
