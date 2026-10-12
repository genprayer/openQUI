#include "catalog.h"

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QResource>
#include <QSet>
#include <QXmlStreamReader>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace open_icue {
namespace {
constexpr auto VENDOR_PREFIX = ":/vendor/";
const QStringList MODEL_IDS = {"slipstream_wireless_dongle_ss", "virtuoso_black_dongle",
                               "darkstar_wireless", "virtuoso_black", "k70_core_rgb"};

class RegisteredBundle final {
  public:
    explicit RegisteredBundle(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error("Original device RCC is missing");
        bytes_ = file.readAll();
        if (!QResource::registerResource(reinterpret_cast<const uchar*>(bytes_.constData())))
            throw std::runtime_error("Original device RCC cannot be registered");
    }
    ~RegisteredBundle() {
        QResource::unregisterResource(reinterpret_cast<const uchar*>(bytes_.constData()));
    }
    RegisteredBundle(const RegisteredBundle&) = delete;
    RegisteredBundle& operator=(const RegisteredBundle&) = delete;

  private:
    QByteArray bytes_;
};

QString label(const QString& key) {
    static const QMap<QString, QString> labels = {{"Lightings", "Beleuchtungseffekte"},
                                                  {"Assignments", "Tastenzuweisungen"},
                                                  {"Gestures", "Gesten"},
                                                  {"Dpi", "DPI"},
                                                  {"SurfaceCalibration", "Oberflächenkalibrierung"},
                                                  {"Equalizer", "Equalizer"},
                                                  {"NvMaxine", "NVIDIA"},
                                                  {"Connections", "Verbindungen"},
                                                  {"Mouse1", "Linksklick"},
                                                  {"Mouse2", "Rechtsklick"},
                                                  {"Mouse3", "Mittelklick"},
                                                  {"MouseWheelTiltLeft", "Scrollrad nach links"},
                                                  {"MouseWheelTiltRight", "Scrollrad nach rechts"},
                                                  {"Mouse5", "Weiterleiten"},
                                                  {"Mouse4", "Zurück"},
                                                  {"Mouse6", "DPI auf"},
                                                  {"Mouse7", "DPI ab"},
                                                  {"MouseProfileUp", "Profil nach oben"},
                                                  {"MouseProfileDown", "Profil nach unten"},
                                                  {"MouseSideFront1", "Vorderseite 1"},
                                                  {"MouseSideFront2", "Vorderseite 2"},
                                                  {"MouseSideBack1", "Rückseite 1"},
                                                  {"MouseSideBack2", "Rückseite 2"},
                                                  {"Gesture_TiltLeft", "Links neigen"},
                                                  {"Gesture_TiltRight", "Rechts neigen"},
                                                  {"Gesture_TiltForward", "Vorne neigen"},
                                                  {"Gesture_TiltBackward", "Zurück neigen"}};
    return labels.value(key, key);
}

QString resourceUrl(QString path) {
    return path.startsWith(":/") ? "qrc" + path : path;
}
} // namespace

void registerOriginalResources() {
    static const auto payloads = [] {
        std::vector<std::unique_ptr<RegisteredBundle>> data;
        QSet<QString> registeredNames;
        for (const auto& directory :
             {QString(VENDOR_PREFIX) + "library/bundles", QString(VENDOR_PREFIX)}) {
            const QDir root(directory);
            for (const auto& name :
                 root.entryList({"*.rcc", "cueres.bin"}, QDir::Files, QDir::Name)) {
                if (registeredNames.contains(name))
                    continue;
                data.push_back(std::make_unique<RegisteredBundle>(root.filePath(name)));
                registeredNames.insert(name);
            }
        }
        return data;
    }();
    Q_UNUSED(payloads);
}

Catalog::Catalog() : order_(MODEL_IDS) {
    if (isUsingLocalCorsairResources())
        registerOriginalResources();
    QFile keyboard(":/app/keyboard-layouts/de-iso.json");
    if (!keyboard.open(QIODevice::ReadOnly))
        throw std::runtime_error("Assignment keyboard layout is missing");
    QJsonParseError keyboardError;
    const auto keyboardJson = QJsonDocument::fromJson(keyboard.readAll(), &keyboardError);
    if (keyboardError.error != QJsonParseError::NoError || !keyboardJson.isObject())
        throw std::runtime_error("Assignment keyboard layout is invalid");
    keyboardLayout_ = keyboardJson.object().toVariantMap();
    if (!isUsingLocalCorsairResources()) {
        QFile file(":/app/devices/open-catalog.json");
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error("Open device catalog is missing");
        QJsonParseError error;
        const auto data = QJsonDocument::fromJson(file.readAll(), &error).object();
        const auto models = data.value("models").toObject();
        if (error.error != QJsonParseError::NoError || data.value("schemaVersion").toInt() != 1 ||
            models.size() != order_.size())
            throw std::runtime_error("Open device catalog is invalid");
        for (const auto& id : order_) {
            if (!models.value(id).isObject())
                throw std::runtime_error("Open device catalog is invalid");
            definitions_.insert(id, models.value(id).toObject());
        }
        defaultEqPresets_ = data.value("eqPresets").toArray().toVariantList();
        return;
    }
    for (const auto& id : order_) {
        QFile file(":/" + id + "/" + id + "_manifest.cuebmf");
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error("Original device manifest is missing");
        QJsonParseError error;
        const auto json = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !json.object().value("common").isObject())
            throw std::runtime_error("Invalid original device manifest");
        definitions_.insert(id, json.object().value("common").toObject());
    }
}

bool Catalog::isUsingLocalCorsairResources() const {
    return OPENQUI_LOCAL_CORSAIR_RESOURCES;
}

QJsonObject Catalog::definition(const QString& id) const {
    return definitions_.value(id);
}

QVariantList Catalog::models() const {
    QVariantList result;
    for (const auto& id : order_) {
        const auto item = definition(id);
        const auto layouts = item.value("Layouts").toObject();
        const auto layout = id == "k70_core_rgb" && layouts.contains("UK")
                                ? layouts.value("UK").toObject()
                            : layouts.contains("US") ? layouts.value("US").toObject()
                            : layouts.isEmpty()      ? QJsonObject{}
                                                     : layouts.begin().value().toObject();
        const auto images = layout.value("layoutImages").toObject();
        QJsonArray bodies;
        if (!images.isEmpty()) {
            const auto colors = images.contains("DE") ? images.value("DE").toObject()
                                                      : images.begin().value().toObject();
            if (!colors.isEmpty())
                bodies = colors.begin().value().toArray();
        }
        QStringList sources;
        for (const auto& body : bodies)
            sources.append(resourceUrl(body.toString()));
        const auto dimensions = item.value("ImageSize").toObject();
        QSize imageSize(dimensions.value("width").toInt(2956),
                        dimensions.value("height").toInt(1120));
        if (!bodies.isEmpty()) {
            QImageReader reader(bodies.first().toString());
            if (reader.size().isValid())
                imageSize = reader.size();
        }
        result.append(QVariantMap{
            {"id", id},
            {"name", item.value("UserModelName").toString()},
            {"kind", item.value("DeviceType").toString()},
            {"icon", resourceUrl(item.value("DeviceIcon").toObject().value("Black").toString())},
            {"bodies", sources},
            {"imageWidth", imageSize.width()},
            {"imageHeight", imageSize.height()},
            {"hullMasks",
             item.value("HullLayoutMasks").toObject().value("Black").toArray().toVariantList()},
            {"tiltHorizontal", resourceUrl(item.value("TiltGesturesCapabilities")
                                               .toObject()
                                               .value("horizontalSetupTiltImage")
                                               .toString())},
            {"tiltVertical", resourceUrl(item.value("TiltGesturesCapabilities")
                                             .toObject()
                                             .value("verticalSetupTiltImage")
                                             .toString())},
            {"minDpi", item.value("MinDpi").toInt()},
            {"maxDpi", item.value("MaxDpi").toInt()},
            {"hasIndependentDpiAxes", item.value("SupportsIndependentXYDpi").toBool()},
            {"maxLightingLayers", item.value("MaxSwLightingLayersCount").toInt(32)},
            {"maxPollingRate", item.value("MaxSupportedPollingRates")
                                   .toObject()
                                   .value("defaultDevice")
                                   .toString()
                                   .remove("PR_")
                                   .remove("Hz")
                                   .toInt()},
            {"isReceiver", id.contains("dongle")},
            {"isConnected", false}});
    }
    return result;
}

std::optional<QString> Catalog::modelForUsb(quint16 vendor, quint16 product) const {
    for (const auto& id : order_) {
        for (const auto& entry : definition(id).value("UsbIds").toArray()) {
            const auto pair = entry.toObject();
            if (pair.value("vid").toInt() == vendor && pair.value("pid").toInt() == product)
                return id;
        }
    }
    return std::nullopt;
}

QVariantList Catalog::navigation(const QString& id) const {
    QVariantList result;
    for (const auto& entry : definition(id).value("SubScreensListData").toArray()) {
        const auto item = entry.toObject();
        const auto type = item.value("type").toString();
        if (type.startsWith("Hardware"))
            continue;
        result.append(QVariantMap{{"id", type}, {"label", label(type)}});
    }
    return result;
}

QVariantList Catalog::keys(const QString& id) const {
    return layoutItems(id, "Key");
}

QVariantList Catalog::lightingKeys(const QString& id) const {
    return layoutItems(id, "Led");
}

QVariantList Catalog::layoutItems(const QString& id, const QString& type) const {
    const auto layout = definition(id).value("Layouts").toObject();
    const auto selected = id == "k70_core_rgb" && layout.contains("UK")
                              ? layout.value("UK").toObject()
                          : layout.contains("US") ? layout.value("US").toObject()
                          : layout.isEmpty()      ? QJsonObject{}
                                                  : layout.begin().value().toObject();
    QVariantList result;
    for (const auto& entry : selected.value("keyItems").toArray()) {
        const auto key = entry.toObject();
        if (key.value("type").toString() != type)
            continue;
        const auto keyId = key.value("id").toString();
        const QString name = key.value("displayName").toString();
        const bool isSide = keyId.startsWith("MouseSide") || keyId.startsWith("Gesture_") ||
                            keyId == "Mouse4" || keyId == "Mouse5";
        QVariantList geometry;
        const auto indices =
            key.value("geometryIndex").toObject().value("Black").toString().split(',');
        const auto shapes = definition(id)
                                .value("SvgStaticGeometryMap")
                                .toObject()
                                .value("RegularStatic")
                                .toObject();
        const auto polygons =
            definition(id).value("StaticGeometryMap").toObject().value("RegularStatic").toObject();
        for (const auto& index : indices) {
            const auto points = polygons.value(index.trimmed()).toArray();
            double left = std::numeric_limits<double>::max(), top = left, right = 0, bottom = 0;
            for (const auto& point : points) {
                const auto value = point.toObject();
                const double x = value.value("x").toDouble(), y = value.value("y").toDouble();
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
            geometry.append(QVariantMap{
                {"path", shapes.value(index.trimmed()).toObject().value("commands").toString()},
                {"points", points.toVariantList()},
                {"shape", shapes.value(index.trimmed()).toObject().toVariantMap()},
                {"mask",
                 definition(id).value("LedGlowMasks").toObject().value(index.trimmed()).toString()},
                {"x", points.isEmpty() ? 0 : left},
                {"y", points.isEmpty() ? 0 : top},
                {"width", points.isEmpty() ? 0 : right - left},
                {"height", points.isEmpty() ? 0 : bottom - top}});
        }
        result.append(QVariantMap{{"id", keyId},
                                  {"label", name.isEmpty() ? label(keyId) : name},
                                  {"isSide", isSide},
                                  {"view", key.value("viewIndex").toInt()},
                                  {"geometry", geometry}});
    }
    return result;
}

QVariantList Catalog::factoryEqPresets() const {
    if (!isUsingLocalCorsairResources())
        return defaultEqPresets_;
    QFile file(":/vendor/equalizer-presets.xml");
    if (!file.open(QIODevice::ReadOnly)) {
        QFile fallback(":/app/devices/open-catalog.json");
        if (!fallback.open(QIODevice::ReadOnly))
            throw std::runtime_error("Factory EQ presets are missing");
        return QJsonDocument::fromJson(fallback.readAll())
            .object()
            .value("eqPresets")
            .toArray()
            .toVariantList();
    }
    QXmlStreamReader xml(file.readAll());
    QVariantList result;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != "presets")
            continue;
        while (xml.readNextStartElement()) {
            QVariantMap preset{{"isFactory", true}};
            while (xml.readNextStartElement()) {
                const auto name = xml.name().toString();
                if (name == "name" || name == "id")
                    preset[name] = xml.readElementText();
                else if (name == "stages") {
                    QVariantList gains;
                    while (xml.readNextStartElement())
                        gains.append(xml.readElementText().toDouble());
                    preset["gains"] = gains;
                } else
                    xml.skipCurrentElement();
            }
            result.append(preset);
        }
    }
    return result;
}

QVariantList Catalog::lightingTypes(const QString& id) const {
    const QMap<QString, QPair<QString, QString>> descriptions = {
        {"Rainbow", {"Regenbogen", "rainbow"}},
        {"ColorPulse", {"Farbpulse", "color_pulse"}},
        {"ColorShift", {"Farbwechsel", "color_shift"}},
        {"StaticCustom", {"Statische Farbe", "static"}},
        {"Solid", {"Voll", "solid"}},
        {"Gradient", {"Gradient", "gradient"}},
        {"RainbowWave", {"Regenbogenwelle", "rainbow_wave"}},
        {"SpiralRainbow", {"Spiralregenbogen", "spiral_rainbow"}},
        {"Type", {"Tastenbeleuchtung", "type_lighting"}},
        {"Wave", {"Welle", "wave"}},
        {"Visor", {"Visier", "visor"}},
        {"Rain", {"Regen", "rain"}},
        {"ColorWave", {"Farbwelle", "color_wave"}},
        {"Ripple", {"Wellenimpuls", "ripple"}},
        {"FrequencyVisualizer", {"Audiovisualisierung", "frequency_visualizer"}}};
    QVariantList result;
    const auto implementations = definition(id).value("SupportedLightingTypes").toObject();
    QStringList seen;
    for (auto it = implementations.begin(); it != implementations.end(); ++it) {
        for (const auto& entry : it.value().toObject().value("Software").toArray()) {
            const auto type = entry.toString();
            if (seen.contains(type))
                continue;
            seen.append(type);
            const auto key = type.section("LightingTypeData", 0, 0);
            const auto description = descriptions.value(key, {key, "none"});
            QStringList controls{"colors"};
            if (key != "StaticCustom" && key != "FrequencyVisualizer")
                controls.append("speed");
            if (key == "StaticCustom" || key == "Rain")
                controls.append("opacity");
            if (QStringList{"Gradient", "Solid", "Ripple", "Type"}.contains(key))
                controls.append("duration");
            if (QStringList{"RainbowWave", "SpiralRainbow", "ColorWave", "Visor", "Gradient",
                            "Wave", "Rain"}
                    .contains(key))
                controls.append("direction");
            if (QStringList{"ColorPulse", "ColorShift", "ColorWave", "Visor", "Rain", "Type",
                            "Ripple"}
                    .contains(key))
                controls.append("random");
            if (key.startsWith("Rainbow") || key == "SpiralRainbow")
                controls.removeAll("colors");
            const QString ownIcon = ":/app/icons/lighting/" + key.toLower() + ".svg";
            const QString vendorIcon =
                ":/vendor/cue-icons/lightings/type/" + description.second + ".svg";
            const bool isOwnIcon = QFile::exists(ownIcon) || !QFile::exists(vendorIcon);
            result.append(QVariantMap{
                {"id", type},
                {"key", key},
                {"label", description.first},
                {"icon",
                 isOwnIcon ? "image://icons/lighting/" + key.toLower() : resourceUrl(vendorIcon)},
                {"controls", controls},
                {"isCustom", key == "Gradient" || key == "Solid" || key == "StaticCustom"}});
        }
    }
    return result;
}

QVariantMap Catalog::initialDocument(const QString& id) const {
    QVariantList stages;
    const auto factory = definition(id).value("Performances").toArray();
    if (!factory.isEmpty()) {
        const auto settings =
            factory.first().toObject().value("defaultDpi").toObject().value("settings").toObject();
        const QStringList ids = {"Dpi1", "Dpi2", "Dpi3", "Dpi4", "Dpi5", "SniperDpi"};
        for (const auto& stage : ids) {
            auto values = settings.value(stage).toObject().toVariantMap();
            values.insert("id", stage);
            values.insert("name",
                          stage == "SniperDpi" ? "Scharfschütze" : "Stufe " + stage.right(1));
            stages.append(values);
        }
    }
    return {{"dpiStages", stages},
            {"activeDpi", 2},
            {"eqPresets", factoryEqPresets()},
            {"activeEq", 0},
            {"dpiPresets", QVariantList{QVariantMap{{"name", "Default"}, {"stages", stages}}}},
            {"activeDpiPreset", 0},
            {"isLinkedEq", false},
            {"tiltAngles", QVariantList{20, 20, 20, 20}},
            {"isMultipleTilt", false},
            {"indicatorColors", QVariantList{"#00bfff", "#ffff00", "#ff0000"}},
            {"lightingLayers", QVariantList{}},
            {"assignments", QVariantList{}}};
}

} // namespace open_icue
