#include "spatial_effects.h"
#include "hardware_backend.h"

#include <QLineF>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace open_icue {
namespace {
constexpr double MAX_PARAMETER = 10000;
constexpr double CLOCK_PERIOD = 4096;
constexpr double DEFAULT_RIPPLE_SPEED = 4;
constexpr double DEFAULT_RIPPLE_TAIL = 1.5;
constexpr double RAIN_TAIL = 1.25;
constexpr double MAX_GEOMETRY_COORDINATE = 1000000;
constexpr double MIN_GEOMETRY_PITCH = 0.01;

double checkedNumber(const QVariantMap& layer, const QString& field, double fallback, double lower,
                     double upper) {
    bool isNumber = false;
    const double value = layer.value(field, fallback).toDouble(&isNumber);
    if (!isNumber || !std::isfinite(value) || value < lower || value > upper)
        throw std::invalid_argument("Invalid spatial lighting parameter");
    return value;
}

quint32 stableHash(const QByteArray& bytes) {
    quint32 value = 2166136261U;
    for (const auto byte : bytes) {
        value ^= quint8(byte);
        value *= 16777619U;
    }
    return value;
}

QColor effectColor(const QVariantMap& layer, quint32 event) {
    const auto colors = layer.value("colors").toList();
    if (layer.value("isRandom").toBool())
        return QColor::fromHsvF((event % 360) / 360.0, 1, 1);
    return QColor(colors.isEmpty() ? "#ffffff" : colors[event % colors.size()].toString());
}

QColor withOpacity(QColor color, double opacity) {
    color.setAlphaF(std::clamp(color.alphaF() * opacity, 0.0, 1.0));
    return color;
}

QColor composite(const QColor& previous, const QColor& next) {
    const double alpha = next.alphaF() + previous.alphaF() * (1 - next.alphaF());
    if (alpha == 0)
        return QColor(Qt::transparent);
    const double first = previous.alphaF() * (1 - next.alphaF()) / alpha;
    const double last = next.alphaF() / alpha;
    return QColor::fromRgbF(previous.redF() * first + next.redF() * last,
                            previous.greenF() * first + next.greenF() * last,
                            previous.blueF() * first + next.blueF() * last, alpha);
}
} // namespace

void validateSpatialEffect(const QVariantMap& layer) {
    const auto type = layer.value("type").toString().section("LightingTypeData", 0, 0);
    if (layer.contains("isKeyTriggered") &&
        (layer.value("isKeyTriggered").metaType().id() != QMetaType::Bool ||
         (layer.value("isKeyTriggered").toBool() &&
          !QStringList{"Type", "Ripple", "Solid"}.contains(type))))
        throw std::invalid_argument("Invalid spatial lighting parameter");
    if (type == "Solid" && layer.value("isKeyTriggered").toBool() && !layer.contains("transitions"))
        throw std::invalid_argument("Invalid continuous lighting timeline");
    if (type != "Rain" && type != "Ripple")
        return;
    (void)checkedNumber(layer, "speed", 1, 0, 2);
    (void)checkedNumber(layer, "duration", 1, 0.1, 60);
    (void)checkedNumber(layer, "opacity", 100, 0, 100);
    if (layer.contains("isRandom") && layer.value("isRandom").metaType().id() != QMetaType::Bool)
        throw std::invalid_argument("Random lighting mode requires a boolean");
    if (type == "Rain") {
        const double direction = checkedNumber(layer, "direction", 2, 0, 3);
        if (std::floor(direction) != direction)
            throw std::invalid_argument("Invalid spatial lighting parameter");
    } else {
        if (layer.contains("rippleVelocity"))
            (void)checkedNumber(layer, "rippleVelocity", 1, 0.1, MAX_PARAMETER);
        if (layer.contains("rippleTailLength"))
            (void)checkedNumber(layer, "rippleTailLength", 1, 0.1, MAX_PARAMETER);
        if (layer.contains("isKeyTriggered") &&
            layer.value("isKeyTriggered").metaType().id() != QMetaType::Bool)
            throw std::invalid_argument("Invalid spatial lighting parameter");
    }
    for (const auto& value : layer.value("colors").toList())
        if (!QColor(value.toString()).isValid())
            throw std::invalid_argument("Invalid lighting color");
}

SpatialEffects::SpatialEffects(const QVariantList& zones) {
    QList<double> widths;
    for (qsizetype index = 0; index < zones.size(); ++index) {
        const auto zone = zones[index].toMap();
        const auto shapes = zone.value("geometry").toList();
        const auto shape = shapes.isEmpty() ? QVariantMap{} : shapes.first().toMap();
        double x = shape.value("x", double(index)).toDouble();
        double y = shape.value("y", 0).toDouble();
        const double width = shape.value("width", 0).toDouble();
        const double height = shape.value("height", 0).toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
            !std::isfinite(height) || std::abs(x) > MAX_GEOMETRY_COORDINATE ||
            std::abs(y) > MAX_GEOMETRY_COORDINATE || width < 0 || height < 0 ||
            width > MAX_GEOMETRY_COORDINATE || height > MAX_GEOMETRY_COORDINATE)
            throw std::invalid_argument("Invalid lighting geometry");
        if (width >= MIN_GEOMETRY_PITCH)
            widths.append(width);
        const QPointF point(x + width / 2, y + height / 2);
        positions_.append(point);
        ids_.append(zone.value("id").toString());
        const int slot = k70LedIndex(ids_.last());
        if (slot >= 0)
            keyPositions_.insert(slot, index);
        if (index == 0)
            minimum_ = maximum_ = point;
        minimum_.setX(std::min(minimum_.x(), point.x()));
        minimum_.setY(std::min(minimum_.y(), point.y()));
        maximum_.setX(std::max(maximum_.x(), point.x()));
        maximum_.setY(std::max(maximum_.y(), point.y()));
    }
    if (!widths.isEmpty()) {
        std::sort(widths.begin(), widths.end());
        pitch_ = widths[widths.size() / 2];
    }
}

QColor SpatialEffects::rain(const QVariantMap& layer, qsizetype zoneIndex, double seconds) const {
    const int direction = layer.value("direction", 2).toInt();
    const bool isHorizontal = direction < 2;
    const auto position = positions_.at(zoneIndex) - minimum_;
    const auto size = maximum_ - minimum_;
    double along = (isHorizontal ? position.x() : position.y()) / pitch_;
    const double across = (isHorizontal ? position.y() : position.x()) / pitch_;
    const double extent = std::max(1.0, (isHorizontal ? size.x() : size.y()) / pitch_);
    if (direction == 1 || direction == 3)
        along = extent - along;
    const QByteArray identity =
        layer.value("sourceId", layer.value("name", "Rain")).toString().toUtf8();
    const auto lane = QByteArray::number(std::floor(across));
    const quint32 seed = stableHash(identity + ':' + lane);
    const double period = extent + RAIN_TAIL * 2;
    const double speed = (0.25 + layer.value("speed", 1).toDouble() * 0.5) * 4 /
                         layer.value("duration", 1).toDouble();
    const double clock =
        std::fmod(seconds, CLOCK_PERIOD) * speed + (seed % 10000) / 10000.0 * period;
    const double distance = std::fmod(clock, period) - along;
    if (distance < 0 || distance > RAIN_TAIL)
        return QColor(Qt::transparent);
    const auto cycle = quint32(std::fmod(std::floor(clock / period), CLOCK_PERIOD));
    const double opacity = 1 - distance / RAIN_TAIL;
    return withOpacity(
        effectColor(layer, stableHash(lane + ':' + QByteArray::number(cycle)) ^ seed), opacity);
}

QColor SpatialEffects::ripple(const QVariantMap& layer, qsizetype zoneIndex, double seconds,
                              const QMap<int, double>* presses,
                              const QList<LightingKeyPress>* history) const {
    const double duration = layer.value("duration", 1).toDouble();
    const double velocity =
        layer.value("rippleVelocity", DEFAULT_RIPPLE_SPEED * pitch_).toDouble() *
        (0.25 + layer.value("speed", 1.5).toDouble() * 0.5);
    const double tail = layer.value("rippleTailLength", DEFAULT_RIPPLE_TAIL * pitch_).toDouble();
    const bool isTriggered = layer.value("isKeyTriggered", true).toBool();
    const auto sample = [&](QPointF center, double started, quint32 seed) {
        const double age = seconds - started;
        if (age < 0 || age >= duration)
            return QColor(Qt::transparent);
        const double distance = QLineF(center, positions_.at(zoneIndex)).length();
        const double localTime = age - distance / velocity;
        if (localTime < 0 || localTime * velocity >= tail)
            return QColor(Qt::transparent);
        if (layer.contains("transitions"))
            return *lightingTimelineColor(layer,
                                          std::min(localTime * velocity / tail, 1.0) * duration);
        return withOpacity(effectColor(layer, seed), 1 - localTime * velocity / tail);
    };
    if (!isTriggered) {
        const double age = std::fmod(seconds, duration);
        return sample((minimum_ + maximum_) / 2, seconds - age, 0);
    }
    QColor result(Qt::transparent);
    if (!presses && !history)
        return result;
    const auto selected = layer.value("zones").toStringList();
    const auto accumulate = [&](int slot, double started) {
        if (seconds < started || seconds - started >= duration)
            return;
        const auto origin = keyPositions_.constFind(slot);
        if (origin == keyPositions_.cend() ||
            (!selected.contains("all") && !selected.contains(ids_.at(*origin))))
            return;
        const auto seed = stableHash(ids_.at(*origin).toUtf8() + ':' + QByteArray::number(started));
        result = composite(result, sample(positions_.at(*origin), started, seed));
    };
    if (history)
        for (const auto& event : *history)
            accumulate(event.slot, event.seconds);
    else
        for (auto press = presses->cbegin(); press != presses->cend(); ++press)
            accumulate(press.key(), press.value());
    return result;
}

} // namespace open_icue
