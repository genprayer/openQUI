#include "cereal.h"
#include "devices/catalog.h"

#include <QColor>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace open_icue {
namespace {
constexpr qint64 MAX_PROFILE_BYTES = 32 * 1024 * 1024;
constexpr int MAX_XML_DEPTH = 256;
constexpr int MAX_XML_NODES = 500000;
constexpr int MAX_LIGHTING_COLORS = 12;
constexpr double MIN_LIGHTING_DURATION = 0.1;
constexpr double MAX_LIGHTING_DURATION = 60.0;
constexpr double SOURCE_FULL_BRIGHTNESS = 10.0;
constexpr double SOURCE_MAX_OPACITY = 255.0;
constexpr double MILLISECONDS_PER_SECOND = 1000.0;
constexpr double MAX_WAVE_PARAMETER = 10000.0;
constexpr quint32 CEREAL_ID_MASK = 0x7fffffff;

struct XmlNode {
    QString name;
    QString text;
    QList<int> children;
};

/** A bounded tree of XML values; no external references are resolved. */
struct ProfileXml {
    QList<XmlNode> nodes;
    QHash<quint32, QString> types;
    QHash<quint32, int> pointerData;

    int child(int node, const QString& name) const {
        if (node < 0)
            return -1;
        for (const int index : nodes.at(node).children)
            if (nodes.at(index).name == name)
                return index;
        return -1;
    }
    int path(int node, std::initializer_list<QString> names) const {
        for (const auto& name : names)
            node = child(node, name);
        return node;
    }
    QString value(int node, const QString& name) const {
        const int index = child(node, name);
        return index < 0 ? QString{} : nodes.at(index).text.trimmed();
    }
    int inheritedField(int node, const QString& name) const {
        while (node >= 0) {
            const int field = child(node, name);
            if (field >= 0)
                return field;
            node = child(node, "base");
        }
        return -1;
    }
    QString inheritedValue(int node, const QString& name) const {
        const int field = inheritedField(node, name);
        return field < 0 ? QString{} : nodes.at(field).text.trimmed();
    }
    QString type(int node) const {
        const auto name = value(node, "polymorphic_name");
        return name.isEmpty() ? types.value(value(node, "polymorphic_id").toUInt() & CEREAL_ID_MASK)
                              : name;
    }
    int data(int node) const {
        const int wrapper = child(node, "ptr_wrapper");
        if (wrapper < 0 || value(wrapper, "valid") == "0")
            return -1;
        const int direct = child(wrapper, "data");
        return direct >= 0 ? direct
                           : pointerData.value(value(wrapper, "id").toUInt() & CEREAL_ID_MASK, -1);
    }
};

ProfileXml parseXml(const QByteArray& bytes) {
    QXmlStreamReader reader(bytes);
    ProfileXml result;
    QList<int> stack;
    while (!reader.atEnd()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference)
            throw std::invalid_argument("DTD and entity references are not accepted in profiles");
        if (reader.isStartElement()) {
            if (stack.size() >= MAX_XML_DEPTH || result.nodes.size() >= MAX_XML_NODES)
                throw std::invalid_argument("Profile XML exceeds structural limits");
            const int index = static_cast<int>(result.nodes.size());
            result.nodes.append({reader.name().toString(), {}, {}});
            if (!stack.isEmpty())
                result.nodes[stack.last()].children.append(index);
            stack.append(index);
        } else if (reader.isCharacters() && !stack.isEmpty()) {
            if (!reader.isWhitespace())
                result.nodes[stack.last()].text.append(reader.text());
        } else if (reader.isEndElement() && !stack.isEmpty()) {
            stack.removeLast();
        }
    }
    if (reader.hasError() || result.nodes.isEmpty() || result.nodes.first().name != "cereal")
        throw std::invalid_argument("Invalid Cereal profile XML");
    for (int index = 0; index < result.nodes.size(); ++index) {
        const auto name = result.value(index, "polymorphic_name");
        if (!name.isEmpty())
            result.types.insert(result.value(index, "polymorphic_id").toUInt() & CEREAL_ID_MASK,
                                name);
        if (result.nodes.at(index).name != "ptr_wrapper")
            continue;
        const int data = result.child(index, "data");
        const auto id = result.value(index, "id").toUInt() & CEREAL_ID_MASK;
        if (id != 0 && data >= 0)
            result.pointerData.insert(id, data);
    }
    return result;
}

int profileNode(const ProfileXml& xml) {
    int profile = xml.child(0, "profile");
    for (const int child : xml.nodes.first().children) {
        const int nested = xml.child(child, "profile");
        if (nested < 0)
            continue;
        if (profile >= 0)
            throw std::invalid_argument("Multiple Cereal profiles are not accepted");
        profile = nested;
    }
    return profile;
}

std::optional<QColor> parseColor(const QString& text) {
    const QColor direct(text);
    if (direct.isValid())
        return direct;
    static const QRegularExpression normalized(
        R"(^rgba?\(\s*([0-9.eE+-]+)\s+([0-9.eE+-]+)\s+([0-9.eE+-]+)(?:\s+([0-9.eE+-]+))?\s*\)$)");
    const auto match = normalized.match(text);
    const bool hasAlpha = text.startsWith("rgba(");
    if (!match.hasMatch() || hasAlpha != !match.captured(4).isEmpty())
        return std::nullopt;
    double components[] = {0, 0, 0, 1};
    for (int index = 0; index < (hasAlpha ? 4 : 3); ++index) {
        bool isNumber = false;
        components[index] = match.captured(index + 1).toDouble(&isNumber);
        if (!isNumber || !std::isfinite(components[index]) || components[index] < 0 ||
            components[index] > 1)
            return std::nullopt;
    }
    return QColor::fromRgbF(components[0], components[1], components[2], components[3]);
}

bool isContinuousLayer(const ProfileXml& xml, int base) {
    if (xml.value(base, "enabled") != "true")
        return false;
    const int hints = xml.child(base, "executionHints");
    return xml.value(hints, "startWithProfile") == "true" &&
           xml.value(hints, "startOnKeyPress") == "false" &&
           xml.value(hints, "stopOnKeyPress") == "false" &&
           xml.value(hints, "stopOnKeyRelease") == "false" &&
           xml.value(hints, "stopAfterTimes") == "-1" &&
           xml.value(hints, "playOption") == "Standard";
}

/** Only this recorded execution mode has per-key semantics implemented by the K70 reader. */
bool isPerKeyLayer(const ProfileXml& xml, int base, const QString& model) {
    const int hints = xml.child(base, "executionHints");
    return model == "k70_core_rgb" && xml.value(base, "enabled") == "true" &&
           xml.value(hints, "startWithProfile") == "false" &&
           xml.value(hints, "startOnKeyPress") == "true" &&
           xml.value(hints, "stopOnKeyPress") == "false" &&
           xml.value(hints, "stopOnKeyRelease") == "false" &&
           xml.value(hints, "stopAfterTimes") == "1" &&
           xml.value(hints, "playOption") == "PlayOnKeyCenter";
}

/** Rank routes without applying a profile for another model or a receiver. */
int routeRank(const ProfileXml& xml, int key, const QString& category, const QString& model,
              const Catalog& catalog) {
    const auto definition = catalog.definition(model);
    if (definition.value("DeviceType").toString() != category ||
        !xml.value(key, "deviceId").isEmpty())
        return 0;
    const int sourceModel = xml.child(key, "modelId");
    bool isVendor = false, isProduct = false;
    const auto vendor = xml.value(sourceModel, "usbVid").toUInt(&isVendor);
    const auto product = xml.value(sourceModel, "usbPid").toUInt(&isProduct);
    if (!isVendor || !isProduct || vendor > 0xffff || product > 0xffff)
        return 0;
    if (vendor != 0 || product != 0) {
        const auto target =
            catalog.modelForUsb(static_cast<quint16>(vendor), static_cast<quint16>(product));
        return target && *target == model ? 3 : 0;
    }
    if (xml.value(key, "hidCaps") != "StandardRgb")
        return 0;
    // The original Virtuoso manifest declares sharedStorage = Virtuoso.
    const QMap<QString, QString> legacyGroups{{"virtuoso_black", "Virtuoso"}};
    const auto storage = definition.value("LightingStorageParameters")
                             .toObject()
                             .value("sharedStorage")
                             .toString(legacyGroups.value(model));
    const auto sourceGroup = xml.value(key, "deviceLightingType");
    if (!storage.isEmpty())
        return sourceGroup == storage ? 2 : 0;
    return (category == "Mouse" || category == "Keyboard") && sourceGroup == "Default" ? 1 : 0;
}

struct Conversion {
    int convertedLayers = 0;
    int unmappedZones = 0;
    QSet<QString> warnings;
};

std::optional<QVariantMap> convertLayer(const ProfileXml& xml, int entry, const QString& model,
                                        const Catalog& catalog, Conversion& summary) {
    const int data = xml.data(entry);
    const int base = xml.child(data, "base");
    const bool isPerKey = isPerKeyLayer(xml, base, model);
    if (xml.type(entry) != "AdvancedLightingLayer" ||
        (!isContinuousLayer(xml, base) && !isPerKey)) {
        summary.warnings.insert("unsupported-execution-or-layer");
        return std::nullopt;
    }
    const int lighting = xml.child(data, "lighting");
    const auto sourceType = xml.type(lighting);
    const bool isStatic = sourceType == "StaticLighting";
    const bool isWave = sourceType == "WaveLighting";
    const bool isPulse = sourceType == "ColorPulseLighting";
    if ((isPerKey && sourceType != "GradientLighting") ||
        (!isStatic && !isWave && !isPulse && sourceType != "GradientLighting")) {
        summary.warnings.insert("unsupported-lighting-effect");
        return std::nullopt;
    }
    const QString nativeType = QString(isPerKey   ? "Type"
                                       : isStatic ? "StaticCustom"
                                       : isWave   ? "Wave"
                                       : isPulse  ? "ColorPulse"
                                                  : "Gradient") +
                               "LightingTypeData.lightings.cue.corsair.com";
    bool isSupported = false;
    for (const auto& type : catalog.lightingTypes(model))
        isSupported |= type.toMap().value("id").toString() == nativeType;
    if (!isSupported)
        return std::nullopt;

    QSet<QString> availableZones;
    for (const auto& zone : catalog.lightingKeys(model))
        availableZones.insert(zone.toMap().value("id").toString());
    if (availableZones.isEmpty())
        return std::nullopt;
    QVariantList zones;
    const auto option = xml.value(base, "option");
    if (option == "AllLeds") {
        zones.append("all");
    } else if (option == "ConcreteLeds") {
        const int keys = xml.child(base, "keys");
        if (keys < 0)
            return std::nullopt;
        QSet<QString> seen;
        for (const int key : xml.nodes.at(keys).children) {
            const auto id = xml.nodes.at(key).text.trimmed();
            if (!availableZones.contains(id)) {
                ++summary.unmappedZones;
                summary.warnings.insert("unmapped-lighting-zones");
            } else if (!seen.contains(id)) {
                zones.append(id);
                seen.insert(id);
            }
        }
    } else {
        summary.warnings.insert("unsupported-zone-selection");
        return std::nullopt;
    }
    if (zones.isEmpty())
        return std::nullopt;

    const int effectData = xml.data(lighting);
    bool isBrightness = false, isDuration = false;
    const double brightness = xml.inheritedValue(effectData, "brightness").toDouble(&isBrightness);
    const double duration =
        xml.inheritedValue(effectData, "duration").toDouble(&isDuration) / MILLISECONDS_PER_SECOND;
    if (!isBrightness || brightness != SOURCE_FULL_BRIGHTNESS || !isDuration ||
        !std::isfinite(duration) || duration < MIN_LIGHTING_DURATION ||
        duration > MAX_LIGHTING_DURATION) {
        summary.warnings.insert("unsupported-lighting-timing-or-brightness");
        return std::nullopt;
    }
    QVariantMap parameters;
    if (isWave) {
        for (const auto& field : {QString("velocity"), QString("tailLength"), QString("angle")}) {
            bool isNumber = false;
            const double value = xml.value(effectData, field).toDouble(&isNumber);
            if (!isNumber || !std::isfinite(value) || value < 0 ||
                value > (field == "angle" ? 360.0 : MAX_WAVE_PARAMETER) ||
                (field != "angle" && value == 0)) {
                summary.warnings.insert("unsupported-wave-parameters");
                return std::nullopt;
            }
            parameters.insert("wave" + field.left(1).toUpper() + field.mid(1), value);
        }
        const auto doublesided = xml.value(effectData, "isDoublesided");
        if (doublesided != "true" && doublesided != "false")
            return std::nullopt;
        parameters.insert("isWaveDoublesided", doublesided == "true");
    }
    if (isPulse) {
        QVariantList colors;
        const int sourceColors = xml.child(effectData, "colors");
        for (const auto& field : {QString("first"), QString("second")}) {
            bool isColor = false;
            const quint32 argb = xml.value(sourceColors, field).toUInt(&isColor);
            if (!isColor)
                return std::nullopt;
            colors.append(QColor::fromRgba(argb).name(QColor::HexArgb));
        }
        const auto random = xml.value(effectData, "random");
        bool isSpeed = false;
        const double speed = xml.value(effectData, "speed").toDouble(&isSpeed);
        if ((random != "true" && random != "false") || !isSpeed || !std::isfinite(speed) ||
            speed < 0 || speed > 2)
            return std::nullopt;
        ++summary.convertedLayers;
        return QVariantMap{{"name", xml.value(base, "name")},
                           {"type", nativeType},
                           {"zones", zones},
                           {"colors", colors},
                           {"duration", duration},
                           {"speed", speed},
                           {"isRandom", random == "true"},
                           {"opacity", 100},
                           {"sourceId", xml.value(base, "id")},
                           {"sourceEffect", sourceType}};
    }
    const int transitions = xml.inheritedField(effectData, "transitions");
    if (transitions < 0 || xml.nodes.at(transitions).children.isEmpty() ||
        xml.nodes.at(transitions).children.size() > MAX_LIGHTING_COLORS)
        return std::nullopt;
    QVariantList points, colors;
    double previous = -1;
    for (const int transition : xml.nodes.at(transitions).children) {
        bool isTime = false;
        const double time = xml.value(transition, "time").toDouble(&isTime);
        const auto color = parseColor(xml.value(transition, "color"));
        if (!isTime || !std::isfinite(time) || time < 0 || time > 1 || time <= previous || !color) {
            summary.warnings.insert("invalid-lighting-transition");
            return std::nullopt;
        }
        const auto value = color->name(QColor::HexArgb);
        points.append(QVariantMap{{"time", time}, {"color", value}});
        colors.append(value);
        previous = time;
    }
    if ((isStatic &&
         (points.size() != 1 || points.first().toMap().value("time").toDouble() != 0)) ||
        (!isStatic && !isWave &&
         (points.size() < 2 || points.first().toMap().value("time").toDouble() != 0 ||
          points.last().toMap().value("time").toDouble() != 1))) {
        summary.warnings.insert("unsupported-transition-boundaries");
        return std::nullopt;
    }
    // Wave editor timelines can omit transparent endpoints; hold the nearest source color.
    if (isWave) {
        if (points.first().toMap().value("time").toDouble() > 0) {
            points.prepend(QVariantMap{{"time", 0.0}, {"color", colors.first()}});
            colors.prepend(colors.first());
        }
        if (points.last().toMap().value("time").toDouble() < 1) {
            points.append(QVariantMap{{"time", 1.0}, {"color", colors.last()}});
            colors.append(colors.last());
        }
        if (points.size() > MAX_LIGHTING_COLORS) {
            summary.warnings.insert("unsupported-wave-transition-count");
            return std::nullopt;
        }
    }
    double opacity = 100;
    if (isStatic) {
        bool isOpacity = false;
        const double raw = xml.value(effectData, "opacity").toDouble(&isOpacity);
        if (!isOpacity || !std::isfinite(raw) || raw < 0 || raw > SOURCE_MAX_OPACITY)
            return std::nullopt;
        opacity = raw * 100 / SOURCE_MAX_OPACITY;
    }
    QVariantMap result{{"name", xml.value(base, "name")},
                       {"type", nativeType},
                       {"zones", zones},
                       {"colors", colors},
                       {"opacity", opacity},
                       {"sourceId", xml.value(base, "id")},
                       {"sourceEffect", sourceType}};
    result.insert(parameters);
    if (!isStatic) {
        result.insert("duration", duration);
        result.insert("transitions", points);
        result.insert("isLooping", !isPerKey);
    }
    if (isWave)
        result.insert("speed", 1.5);
    ++summary.convertedLayers;
    return result;
}

QVariantList convertRoute(const ProfileXml& xml, int route, const QString& model,
                          const Catalog& catalog, Conversion& summary) {
    QVariantList result;
    const int data = xml.data(xml.child(route, "value"));
    const int channels = xml.child(data, "channelProperties");
    if (channels < 0)
        return result;
    const int maximum = catalog.definition(model).value("MaxSwLightingLayersCount").toInt(32);
    for (const int channel : xml.nodes.at(channels).children) {
        if (xml.value(channel, "key") != "0") {
            summary.warnings.insert("unsupported-lighting-channel");
            continue;
        }
        const int groups = xml.child(channel, "value");
        if (groups < 0)
            continue;
        for (const int group : xml.nodes.at(groups).children) {
            const int layers = xml.inheritedField(xml.data(group), "layers");
            if (layers < 0)
                continue;
            for (const int layer : xml.nodes.at(layers).children) {
                if (result.size() >= maximum) {
                    summary.warnings.insert("device-lighting-layer-limit");
                    continue;
                }
                const auto converted = convertLayer(xml, layer, model, catalog, summary);
                if (converted)
                    result.append(*converted);
            }
        }
    }
    // iCUE serializes topmost first; the native compositor stores bottommost first.
    std::reverse(result.begin(), result.end());
    return result;
}

void convertLighting(CerealProfile& result, const ProfileXml& xml, int profile) {
    const Catalog catalog;
    Conversion summary;
    int sourceLayers = 0;
    for (const auto& node : xml.nodes)
        sourceLayers += node.name == "lighting";
    const int categories = xml.child(profile, "properties");
    if (categories >= 0) {
        for (const auto& entry : catalog.models()) {
            const auto model = entry.toMap().value("id").toString();
            int selected = -1, selectedRank = 0;
            for (const int category : xml.nodes.at(categories).children) {
                const auto categoryName = xml.value(category, "key");
                const int properties = xml.path(category, {"value", "properties"});
                if (properties < 0)
                    continue;
                for (const int property : xml.nodes.at(properties).children) {
                    if (xml.type(property) != "AggregatedLightingsProperty::Proxy")
                        continue;
                    const int routes = xml.child(xml.data(property), "properties");
                    if (routes < 0)
                        continue;
                    for (const int route : xml.nodes.at(routes).children) {
                        const int rank =
                            routeRank(xml, xml.child(route, "key"), categoryName, model, catalog);
                        if (rank > selectedRank) {
                            selected = route;
                            selectedRank = rank;
                        }
                    }
                }
            }
            if (selected < 0)
                continue;
            const auto layers = convertRoute(xml, selected, model, catalog, summary);
            if (!layers.isEmpty())
                result.devices.insert(model, QVariantMap{{"lightingLayers", layers}});
        }
    }
    QStringList warnings(summary.warnings.cbegin(), summary.warnings.cend());
    warnings.sort();
    const int preservedLayers = std::max(0, sourceLayers - summary.convertedLayers);
    result.metadata.insert("conversion",
                           QVariantMap{{"version", 2},
                                       {"sourceLightingLayers", sourceLayers},
                                       {"convertedLightingLayers", summary.convertedLayers},
                                       {"preservedLightingLayers", preservedLayers},
                                       {"deviceCount", result.devices.size()},
                                       {"unmappedZones", summary.unmappedZones},
                                       {"warnings", warnings},
                                       {"isPartial", preservedLayers > 0 || !warnings.isEmpty()},
                                       {"isHardwareApplied", false}});
}
} // namespace

CerealProfile readCerealProfile(const QString& path) {
    QFile file(path);
    if (file.size() > MAX_PROFILE_BYTES || !file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot read profile within size limit");
    CerealProfile result{file.read(MAX_PROFILE_BYTES + 1), {}, {}, {}};
    if (result.bytes.size() > MAX_PROFILE_BYTES)
        throw std::runtime_error("Profile grew beyond size limit while reading");
    const auto xml = parseXml(result.bytes);
    const int profile = profileNode(xml);
    result.name = xml.value(profile, "name");
    if (profile < 0 || result.name.trimmed().isEmpty())
        throw std::invalid_argument("Invalid Cereal profile XML");
    for (const auto& name : {"name", "id", "exporterVersion"})
        result.metadata.insert(name, xml.value(profile, name));
    QStringList types(xml.types.cbegin(), xml.types.cend());
    types.removeDuplicates();
    types.sort();
    result.metadata.insert("propertyTypes", types);
    result.metadata.insert("isBytePreserved", true);
    result.metadata.insert("isHardwareMapped", false);
    convertLighting(result, xml, profile);
    return result;
}
} // namespace open_icue
