#include "audio/filter_chain.h"
#include "core/workspace.h"
#include "devices/catalog.h"
#include "gui/application_model.h"
#include "profiles/cereal.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

namespace {
QByteArray cerealLightingLayer(const QByteArray& type, const QByteArray& name,
                               const QByteArray& keys, const QByteArray& effect,
                               bool isContinuous = true) {
    const auto typeId = type == "StaticLighting" ? "15" : "16";
    return "<value0><polymorphic_id>2147483662</polymorphic_id>"
           "<polymorphic_name>AdvancedLightingLayer</polymorphic_name>"
           "<ptr_wrapper><valid>1</valid><data><base><id>source-layer</id><name>" +
           name + "</name><keys>" + keys +
           "</keys><enabled>true</enabled><executionHints>"
           "<startOnKeyPress>" +
           (isContinuous ? "false" : "true") + "</startOnKeyPress><startWithProfile>" +
           (isContinuous ? "true" : "false") +
           "</startWithProfile><stopOnKeyPress>false</stopOnKeyPress>"
           "<stopOnKeyRelease>false</stopOnKeyRelease><stopAfterTimes>" +
           (isContinuous ? "-1" : "1") +
           "</stopAfterTimes><playOption>Standard</playOption></executionHints><option>" +
           (keys.isEmpty() ? "AllLeds" : "ConcreteLeds") +
           "</option></base><lighting><polymorphic_id>" + typeId +
           "</polymorphic_id><polymorphic_name>" + type +
           "</polymorphic_name><ptr_wrapper><valid>1</valid><data>" + effect +
           "</data></ptr_wrapper></lighting></data></ptr_wrapper></value0>";
}
QByteArray cerealLightingRoute(const QByteArray& layers, int vendor = 0, int product = 0,
                               const QByteArray& group = "Default") {
    return "<value0><key><deviceId></deviceId><modelId><usbVid>" + QByteArray::number(vendor) +
           "</usbVid><usbPid>" + QByteArray::number(product) +
           "</usbPid></modelId><hidCaps>StandardRgb</hidCaps><deviceLightingType>" + group +
           "</deviceLightingType></key><value><ptr_wrapper><valid>1</valid><data>"
           "<channelProperties><value0><key>0</key><value><value0><ptr_wrapper><valid>1</valid>"
           "<data><base><layers>" +
           layers +
           "</layers></base></data></ptr_wrapper></value0></value></value0></channelProperties>"
           "</data></ptr_wrapper></value></value0>";
}
QByteArray cerealLightingCategory(const QByteArray& category, const QByteArray& routes) {
    return "<value0><key>" + category +
           "</key><value><properties><value0><polymorphic_id>2147483661</polymorphic_id>"
           "<polymorphic_name>AggregatedLightingsProperty::Proxy</polymorphic_name>"
           "<ptr_wrapper><valid>1</valid><data><properties>" +
           routes + "</properties></data></ptr_wrapper></value0></properties></value></value0>";
}
QByteArray cerealProfile(const QByteArray& categories) {
    return "<cereal><value0><profile><name>Lighting import</name><id>source-profile</id>"
           "<exporterVersion>5.52.93</exporterVersion><properties>" +
           categories +
           "</properties><unknownFutureProperty>retained</unknownFutureProperty>"
           "</profile></value0></cereal>";
}
QByteArray staticCerealEffect(const QByteArray& color = "#ff123456", int opacity = 255) {
    return "<base><base><duration>1000</duration><brightness>10</brightness></base>"
           "<transitions><value0><time>0</time><color>" +
           color + "</color></value0></transitions></base><opacity>" + QByteArray::number(opacity) +
           "</opacity>";
}
} // namespace

class NativeCoreTests : public QObject {
    Q_OBJECT
  private slots:
    void deviceCatalogAndDefaultPresets() {
        open_icue::Catalog catalog;
        QCOMPARE(catalog.models().size(), 5);
        QCOMPARE(catalog.modelForUsb(0x1b1c, 0x1bb2).value(), QString("darkstar_wireless"));
        QCOMPARE(catalog.modelForUsb(0x1b1c, 0x0a42).value(), QString("virtuoso_black_dongle"));
        QVERIFY(!catalog.modelForUsb(0x0001, 0x0002).has_value());
        QCOMPARE(catalog.definition("darkstar_wireless").value("MaxDpi").toInt(), 26000);
        QCOMPARE(catalog.definition("darkstar_wireless")
                     .value("MaxSupportedPollingRates")
                     .toObject()
                     .value("defaultDevice")
                     .toString(),
                 QString("PR_1000Hz"));
        QVERIFY(catalog.keys("darkstar_wireless").size() >= 15);
        QCOMPARE(catalog.factoryEqPresets().size(), 5);
        const auto presets = catalog.factoryEqPresets();
        if (catalog.isUsingLocalCorsairResources() &&
            QFile::exists(":/vendor/equalizer-presets.xml")) {
            QCOMPARE(presets.at(1).toMap().value("gains").toList().at(0).toDouble(), -1.0);
            QCOMPARE(presets.at(4).toMap().value("gains").toList().at(1).toDouble(), 6.0);
        } else {
            QCOMPARE(presets.first().toMap().value("name").toString(), QString("Flat"));
            QCOMPARE(presets.first().toMap().value("gains").toList().size(), 10);
            for (const auto& gain : presets.first().toMap().value("gains").toList())
                QCOMPARE(gain.toDouble(), 0.0);
            QCOMPARE(presets.at(1).toMap().value("gains").toList().at(0).toDouble(), 3.0);
            QCOMPARE(presets.at(4).toMap().value("gains").toList().at(1).toDouble(), 1.0);
        }
    }
    void selectedAssetModeIsSelfContained() {
        open_icue::Catalog catalog;
        if (catalog.isUsingLocalCorsairResources()) {
            for (const auto& value : catalog.models()) {
                const auto model = value.toMap();
                const auto id = model.value("id").toString();
                const auto bodies = model.value("bodies").toStringList();
                QVERIFY(!bodies.isEmpty());
                for (const auto& body : bodies) {
                    QVERIFY(body.startsWith("qrc:/" + id + "/"));
                    QVERIFY(QFile::exists(body.mid(3)));
                    QVERIFY(!body.startsWith("qrc:/app/devices/"));
                }
            }
            QVERIFY(QFile::exists(":/vendor/darkstar_wireless.rcc") ||
                    QFile::exists(":/vendor/library/bundles/darkstar_wireless.rcc"));
            for (const auto& id : {QString("govee"), QString("nanoleaf"), QString("philips_hue")}) {
                if (!QFile::exists(":/vendor/library/bundles/" + id + ".rcc"))
                    continue;
                QVERIFY(QFile::exists(":/" + id + "/" + id + "_manifest.cuebmf"));
                QVERIFY(!catalog.modelForUsb(0xffff, 0xffff).has_value());
            }
            return;
        }
        QVERIFY(!QFile::exists(":/vendor/darkstar_wireless.rcc"));
        QVERIFY(!QFile::exists(":/vendor/equalizer-presets.xml"));
        for (const auto& value : catalog.models()) {
            const auto model = value.toMap();
            QVERIFY(model.value("imageWidth").toInt() > 0);
            QVERIFY(model.value("imageHeight").toInt() > 0);
            const auto bodies = model.value("bodies").toStringList();
            QVERIFY(!bodies.isEmpty());
            for (const auto& body : bodies) {
                QVERIFY(body.startsWith("qrc:/app/devices/"));
                QVERIFY(QFile::exists(body.mid(3)));
            }
        }
        for (const auto& value : catalog.factoryEqPresets()) {
            QList<double> gains;
            for (const auto& gain : value.toMap().value("gains").toList())
                gains.append(gain.toDouble());
            open_icue::validateEq(gains);
        }
    }
    void profilesSurviveRestart() {
        QTemporaryDir directory;
        open_icue::Workspace first(directory.path());
        first.createProfile("Test");
        const auto id = first.snapshot().value("activeProfileId").toString();
        first.updateDevice("darkstar_wireless", "tiltAngles", QVariantList{10, 20, 30, 40}, {});
        open_icue::Workspace second(directory.path());
        QCOMPARE(second.snapshot().value("activeProfileId").toString(), id);
        QCOMPARE(second.device("darkstar_wireless", {}).value("tiltAngles").toList().at(2).toInt(),
                 30);
        second.renameProfile(id, "Renamed");
        second.removeProfile(id);
        QCOMPARE(second.snapshot().value("profiles").toList().size(), 1);
        QVERIFY_EXCEPTION_THROWN(
            second.removeProfile(second.snapshot().value("activeProfileId").toString()),
            std::invalid_argument);
    }
    void corruptWorkspaceIsPreserved() {
        QTemporaryDir directory;
        const QByteArray corrupt("{not-json");
        QFile file(directory.filePath("workspace.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(corrupt);
        file.close();
        QVERIFY_EXCEPTION_THROWN(open_icue::Workspace(directory.path()), std::runtime_error);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), corrupt);
    }
    void duplicatedProfilesKeepAllDevicesAndIndependentEdits() {
        QTemporaryDir directory;
        open_icue::Workspace workspace(directory.path());
        const auto original = workspace.snapshot().value("activeProfileId").toString();
        workspace.updateDevice("k70_core_rgb", "lightingLayers", QVariantList{"keyboard"}, {});
        workspace.updateDevice("darkstar_wireless", "lightingLayers", QVariantList{"mouse"}, {});
        workspace.updateDevice("virtuoso_black", "eqPresets", QVariantList{"headset"}, {});
        workspace.duplicateProfile(original, "Copy");
        const auto duplicate = workspace.snapshot().value("activeProfileId").toString();
        QVERIFY(original != duplicate);
        QCOMPARE(workspace.device("virtuoso_black", {}).value("eqPresets").toList(),
                 QVariantList{"headset"});
        workspace.updateDevice("k70_core_rgb", "lightingLayers", QVariantList{"edited"}, {});
        workspace.selectProfile(original);
        QCOMPARE(workspace.device("k70_core_rgb", {}).value("lightingLayers").toList(),
                 QVariantList{"keyboard"});
        workspace.selectProfile(duplicate);
        QCOMPARE(workspace.device("darkstar_wireless", {}).value("lightingLayers").toList(),
                 QVariantList{"mouse"});
    }
    void cerealImportIsIdempotent() {
        const QString source =
            QString::fromUtf8(OPENQUI_SOURCE_DIR) + "/tests/fixtures/minimal.cueprofile";
        QVERIFY2(!source.isEmpty(), "Cereal test fixture is missing");
        const auto parsed = open_icue::readCerealProfile(source);
        QCOMPARE(parsed.name, QString("Example profile"));
        QTemporaryDir directory;
        open_icue::Workspace workspace(directory.path());
        workspace.importProfile(source);
        workspace.importProfile(source);
        QCOMPARE(workspace.snapshot().value("profiles").toList().size(), 2);
        const auto imported = workspace.snapshot().value("profiles").toList().last().toMap();
        QFile preserved(directory.filePath("imports/" + imported.value("sourceSha256").toString() +
                                           ".cueprofile"));
        QVERIFY(preserved.open(QIODevice::ReadOnly));
        QCOMPARE(preserved.readAll(), parsed.bytes);
    }
    void rejectsEntityProfiles() {
        QTemporaryDir directory;
        QFile file(directory.filePath("bad.cueprofile"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(
            "<!DOCTYPE cereal [<!ENTITY leak SYSTEM "
            "'file:///etc/passwd'>]><cereal><profile><name>&leak;</name></profile></cereal>");
        file.close();
        QVERIFY_EXCEPTION_THROWN((void)open_icue::readCerealProfile(file.fileName()),
                                 std::invalid_argument);
    }
    void cerealLightingConversionPreservesTimingAlphaAndSource() {
        const auto staticLayer = cerealLightingLayer(
            "StaticLighting", "Named static", "<value0>A</value0><value1>UnknownKey</value1>",
            staticCerealEffect("rgba(1 0 0 0.5)", 128));
        const auto gradient = cerealLightingLayer(
            "GradientLighting", "Timed gradient", {},
            "<base><base><duration>2000</duration><brightness>10</brightness></base>"
            "<transitions><value0><time>0</time><color>rgba(0 0 0 0)</color></value0>"
            "<value1><time>0.25</time><color>rgb(1 0 0)</color></value1>"
            "<value2><time>1</time><color>#ff00ff00</color></value2></transitions></base>");
        const auto reactive =
            cerealLightingLayer("StaticLighting", "On press", {}, staticCerealEffect(), false);
        const auto bytes = cerealProfile(cerealLightingCategory(
            "Keyboard", cerealLightingRoute(staticLayer + gradient + reactive)));
        QTemporaryDir directory;
        QFile source(directory.filePath("lighting.cueprofile"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(bytes), bytes.size());
        source.close();
        const auto parsed = open_icue::readCerealProfile(source.fileName());
        QCOMPARE(parsed.bytes, bytes);
        QCOMPARE(parsed.devices.keys(), QStringList{"k70_core_rgb"});
        const auto layers =
            parsed.devices.value("k70_core_rgb").toMap().value("lightingLayers").toList();
        QCOMPARE(layers.size(), 2);
        QCOMPARE(layers.last().toMap().value("name").toString(), QString("Named static"));
        QCOMPARE(layers.last().toMap().value("zones").toStringList(), QStringList{"A"});
        QCOMPARE(layers.last().toMap().value("colors").toList(), QVariantList{"#80ff0000"});
        QCOMPARE(layers.last().toMap().value("opacity").toDouble(), 128 * 100.0 / 255);
        const auto importedGradient = layers.first().toMap();
        QCOMPARE(importedGradient.value("duration").toDouble(), 2.0);
        QCOMPARE(importedGradient.value("transitions").toList().size(), 3);
        QCOMPARE(
            importedGradient.value("transitions").toList().at(1).toMap().value("time").toDouble(),
            0.25);
        QVERIFY(importedGradient.value("isLooping").toBool());
        const auto summary = parsed.metadata.value("conversion").toMap();
        QCOMPARE(summary.value("sourceLightingLayers").toInt(), 3);
        QCOMPARE(summary.value("convertedLightingLayers").toInt(), 2);
        QCOMPARE(summary.value("preservedLightingLayers").toInt(), 1);
        QCOMPARE(summary.value("unmappedZones").toInt(), 1);
        QVERIFY(summary.value("isPartial").toBool());
        QVERIFY(!summary.value("isHardwareApplied").toBool());
        open_icue::Workspace workspace(directory.filePath("state"));
        workspace.importProfile(source.fileName());
        const auto imported = workspace.snapshot().value("profiles").toList().last().toMap();
        QCOMPARE(imported.value("devices").toMap(), parsed.devices);
        QCOMPARE(workspace.device("k70_core_rgb", {}).value("lightingLayers").toList(), layers);
        workspace.updateDevice("k70_core_rgb", "lightingLayers", QVariantList{}, {});
        workspace.importProfile(source.fileName());
        QCOMPARE(workspace.snapshot().value("profiles").toList().size(), 2);
        QVERIFY(workspace.device("k70_core_rgb", {}).value("lightingLayers").toList().isEmpty());
    }
    void cerealRoutesPreferExactModelAndNeverTargetReceivers() {
        const auto generic =
            cerealLightingLayer("StaticLighting", "Generic", {}, staticCerealEffect());
        const auto exact =
            cerealLightingLayer("StaticLighting", "Exact", {}, staticCerealEffect("#ffff0000"));
        const auto unrelated = cerealLightingLayer("StaticLighting", "Other model", {},
                                                   staticCerealEffect("#ff00ff00"));
        const auto categories =
            cerealLightingCategory("Keyboard", cerealLightingRoute(generic) +
                                                   cerealLightingRoute(unrelated, 0x1b1c, 0x1b13) +
                                                   cerealLightingRoute(exact, 0x1b1c, 0x1bfd)) +
            cerealLightingCategory("Headset", cerealLightingRoute(generic, 0, 0, "Default") +
                                                  cerealLightingRoute(exact, 0, 0, "Virtuoso"));
        QTemporaryDir directory;
        QFile source(directory.filePath("routes.cueprofile"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(cerealProfile(categories));
        source.close();
        const auto parsed = open_icue::readCerealProfile(source.fileName());
        QCOMPARE(parsed.devices.size(), 2);
        QVERIFY(!parsed.devices.contains("virtuoso_black_dongle"));
        QVERIFY(!parsed.devices.contains("slipstream_wireless_dongle_ss"));
        for (const auto& model : {"k70_core_rgb", "virtuoso_black"})
            QCOMPARE(parsed.devices.value(model)
                         .toMap()
                         .value("lightingLayers")
                         .toList()
                         .first()
                         .toMap()
                         .value("name")
                         .toString(),
                     QString("Exact"));
    }
    void cerealUnknownEffectsAndInvalidTimelinesRemainPreserved() {
        const auto effect =
            "<base><base><duration>2000</duration><brightness>10</brightness></base>"
            "<transitions><value0><time>0.25</time><color>#ffff0000</color></value0>"
            "<value1><time>1</time><color>#ff00ff00</color></value1></transitions></base>";
        const auto gradient =
            cerealLightingLayer("GradientLighting", "Missing boundary", {}, effect);
        const auto unknown =
            cerealLightingLayer("FutureLighting", "Unknown", {}, staticCerealEffect());
        const auto bytes = cerealProfile(
            cerealLightingCategory("Keyboard", cerealLightingRoute(gradient + unknown)));
        QTemporaryDir directory;
        QFile source(directory.filePath("unknown.cueprofile"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(bytes);
        source.close();
        const auto parsed = open_icue::readCerealProfile(source.fileName());
        QCOMPARE(parsed.bytes, bytes);
        QVERIFY(parsed.devices.isEmpty());
        const auto summary = parsed.metadata.value("conversion").toMap();
        QCOMPARE(summary.value("convertedLightingLayers").toInt(), 0);
        QCOMPARE(summary.value("preservedLightingLayers").toInt(), 2);
        const auto warnings = summary.value("warnings").toStringList();
        QVERIFY(warnings.contains("unsupported-transition-boundaries"));
        QVERIFY(warnings.contains("unsupported-lighting-effect"));
    }
    void importedMovingAndPerKeyEffectsTargetAllThreePeripherals() {
        const QByteArray timeline =
            "<base><base><duration>2000</duration><brightness>10</brightness></base>"
            "<transitions><value0><time>0</time><color>#ffff0000</color></value0>"
            "<value1><time>1</time><color>#00000000</color></value1></transitions></base>";
        auto reactive = cerealLightingLayer("GradientLighting", "Per key", {}, timeline, false);
        reactive.replace("<playOption>Standard</playOption>",
                         "<playOption>PlayOnKeyCenter</playOption>");
        const auto wave = cerealLightingLayer(
            "WaveLighting", "Wave", "<value0>A</value0><value1>G18</value1>",
            timeline + "<velocity>80</velocity><tailLength>300</tailLength>"
                       "<angle>135</angle><isDoublesided>false</isDoublesided>");
        const auto background = cerealLightingLayer("StaticLighting", "Background", {},
                                                    staticCerealEffect("#ff000000"));
        const auto pulse = cerealLightingLayer(
            "ColorPulseLighting", "Pulse", {},
            "<base><duration>1000</duration><brightness>10</brightness></base>"
            "<colors><first>4294901760</first><second>4278255360</second></colors>"
            "<random>false</random><speed>2</speed>");
        const auto bytes = cerealProfile(
            cerealLightingCategory("Keyboard", cerealLightingRoute(reactive + wave + background)) +
            cerealLightingCategory("Mouse", cerealLightingRoute(pulse)) +
            cerealLightingCategory("Headset", cerealLightingRoute(pulse, 0, 0, "Virtuoso")));
        QTemporaryDir directory;
        QFile source(directory.filePath("multi-device.cueprofile"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(bytes);
        source.close();
        const auto parsed = open_icue::readCerealProfile(source.fileName());
        QCOMPARE(parsed.devices.size(), 3);
        const auto layers =
            parsed.devices.value("k70_core_rgb").toMap().value("lightingLayers").toList();
        QCOMPARE(layers.size(), 3);
        QCOMPARE(layers.first().toMap().value("name").toString(), QString("Background"));
        QCOMPARE(layers.at(1).toMap().value("waveAngle").toDouble(), 135.0);
        QCOMPARE(layers.at(1).toMap().value("zones").toStringList(), QStringList{"A"});
        QVERIFY(layers.last().toMap().value("type").toString().startsWith("TypeLightingTypeData"));
        QVERIFY(!layers.last().toMap().value("isLooping").toBool());
        QCOMPARE(parsed.devices.value("virtuoso_black")
                     .toMap()
                     .value("lightingLayers")
                     .toList()
                     .first()
                     .toMap()
                     .value("colors")
                     .toList(),
                 (QVariantList{"#ffff0000", "#ff00ff00"}));
        const auto conversion = parsed.metadata.value("conversion").toMap();
        QCOMPARE(conversion.value("version").toInt(), 2);
        QVERIFY(conversion.value("isPartial").toBool());
    }
    void converterUpgradePreservesEarlierImportedProfileEdits() {
        QTemporaryDir directory;
        open_icue::Workspace workspace(directory.path());
        const auto source =
            QString::fromUtf8(OPENQUI_SOURCE_DIR) + "/tests/fixtures/minimal.cueprofile";
        workspace.importProfile(source);
        const auto firstId = workspace.snapshot().value("activeProfileId").toString();
        workspace.updateDevice("k70_core_rgb", "settings", QVariantMap{{"edited", true}}, {});
        const auto path = directory.filePath("workspace.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto saved = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        auto profiles = saved.value("profiles").toArray();
        auto earlier = profiles.last().toObject();
        auto metadata = earlier.value("sourceMetadata").toObject();
        auto conversion = metadata.value("conversion").toObject();
        conversion["version"] = 1;
        metadata["conversion"] = conversion;
        earlier["sourceMetadata"] = metadata;
        profiles[profiles.size() - 1] = earlier;
        saved["profiles"] = profiles;
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(saved).toJson());
        file.close();
        workspace.importProfile(source);
        QVERIFY(workspace.snapshot().value("activeProfileId").toString() != firstId);
        QCOMPARE(workspace.snapshot().value("profiles").toList().size(), 3);
        workspace.importProfile(source);
        QCOMPARE(workspace.snapshot().value("profiles").toList().size(), 3);
        workspace.selectProfile(firstId);
        QVERIFY(workspace.device("k70_core_rgb", {})
                    .value("settings")
                    .toMap()
                    .value("edited")
                    .toBool());
    }
    void equalizerGraphAndBounds() {
        QList<double> gains(10, 0.0);
        const auto bytes = open_icue::equalizerConfig(gains, "alsa_output.test");
        QVERIFY(bytes.contains("eqL9:Out"));
        QVERIFY(bytes.contains("eqR9:Out"));
        QVERIFY(bytes.contains("bq_peaking"));
        QVERIFY_EXCEPTION_THROWN((void)open_icue::equalizerConfig(gains, "open-icue.output"),
                                 std::invalid_argument);
        gains[0] = std::numeric_limits<double>::quiet_NaN();
        QVERIFY_EXCEPTION_THROWN(open_icue::validateEq(gains), std::invalid_argument);
        gains[0] = 12.5;
        QVERIFY_EXCEPTION_THROWN(open_icue::validateEq(gains), std::invalid_argument);
        gains[0] = 0.1;
        QVERIFY_EXCEPTION_THROWN(open_icue::validateEq(gains), std::invalid_argument);
    }
    void nativeDpiEditsAndPresets() {
        QTemporaryDir directory;
        open_icue::ApplicationModel model(directory.path());
        model.setDpiStage(0, "dpiX", 1400);
        auto stages = model.document().value("dpiStages").toList();
        QCOMPARE(stages.first().toMap().value("dpiY").toInt(), 1400);
        const auto unified = model.document();
        model.setDpiStage(0, "independentXY", true);
        QVERIFY(!model.lastOperation().value("isOk").toBool());
        QCOMPARE(model.document(), unified);
        model.setDpiStage(0, "dpiY", 2200);
        QCOMPARE(model.document().value("dpiStages").toList().first().toMap().value("dpiX").toInt(),
                 2200);
        model.setDpiStage(0, "dpiX", 999999);
        QCOMPARE(model.document().value("dpiStages").toList().first().toMap().value("dpiX").toInt(),
                 2200);
        QVERIFY(!model.notice().isEmpty());
        model.addDpiPreset("Second");
        model.selectDpiPreset(0);
        QCOMPARE(model.document().value("dpiStages").toList().first().toMap().value("dpiY").toInt(),
                 2200);
        model.resetDpiPreset();
        QCOMPARE(model.document().value("dpiStages").toList().first().toMap().value("dpiX").toInt(),
                 400);
    }
    void measuredSurroundGraphAndBooleanValidation() {
        open_icue::Catalog catalog;
        QTemporaryDir directory;
        const auto hrir = open_icue::ensureKemarHrir(directory.path());
        QFile file(hrir);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto bytes = file.readAll();
        QCOMPARE(bytes.first(4), QByteArray("RIFF"));
        const auto config =
            open_icue::equalizerConfig(QList<double>(10, 0), "alsa_output.test", hrir);
        QCOMPARE(config.count("\"convolver\""), 16);
        QVERIFY(config.contains("\"audio.channels\" = 8"));
        QVERIFY(config.contains("hrirSRR"));
        QVERIFY(config.contains("lfeLowPass"));
        QVERIFY(config.contains("OpenQUI 7.1 Surround"));
        open_icue::ApplicationModel model(directory.path());
        model.setDocumentValue("isSurroundEnabled", true);
        QVERIFY(model.document().value("isSurroundEnabled").toBool());
        model.setDocumentValue("isSurroundEnabled", "true");
        QVERIFY(!model.lastOperation().value("isOk").toBool());
    }
    void linkedEqualizerAndFactoryRestore() {
        QTemporaryDir directory;
        open_icue::ApplicationModel model(directory.path());
        model.selectDevice("virtuoso_black");
        model.selectEqPreset(1);
        const auto originalGains =
            model.document().value("eqPresets").toList().at(1).toMap().value("gains").toList();
        model.setDocumentValue("isLinkedEq", true);
        model.setEqGain(0, originalGains.at(0).toDouble() + 1.0);
        auto preset = model.document().value("eqPresets").toList().at(1).toMap();
        QCOMPARE(preset.value("gains").toList().at(1).toDouble(),
                 originalGains.at(1).toDouble() + 1.0);
        model.resetEqPreset();
        preset = model.document().value("eqPresets").toList().at(1).toMap();
        QCOMPARE(preset.value("gains").toList(), originalGains);
        model.addEqPreset("Custom");
        model.removeEqPreset(5);
        QCOMPARE(model.document().value("activeEq").toInt(), 0);
    }
    void rejectedHardwareApplyDoesNotMutateProfile() {
        QTemporaryDir directory;
        open_icue::ApplicationModel model(directory.path());
        const auto state = model.document();
        model.applyHardware();
        QCOMPARE(model.document(), state);
        QVERIFY(!model.lastOperation().value("isOk").toBool());
        QVERIFY(model.notice().contains("disabled"));
        model.setTiltAngle(0, 85);
        QCOMPARE(model.document().value("tiltAngles").toList().first().toInt(), 20);
    }
    void lightingZonesAndKeyboardAssignmentsAreValidated() {
        QTemporaryDir directory;
        open_icue::ApplicationModel model(directory.path());
        model.selectDevice("k70_core_rgb");
        QVERIFY(model.lightingKeys().size() >= 104);
        QVariantMap layer{{"name", "A"},
                          {"type", "StaticCustomLightingTypeData.lightings.cue.corsair.com"},
                          {"zones", QVariantList{"A"}},
                          {"colors", QVariantList{"#ff0000"}},
                          {"opacity", 100}};
        model.setDocumentValue("lightingLayers", QVariantList{layer});
        QVERIFY(model.lastOperation().value("isOk").toBool());
        const auto before = model.document();
        layer["zones"] = QVariantList{"NonexistentKey"};
        model.setDocumentValue("lightingLayers", QVariantList{layer});
        QVERIFY(!model.lastOperation().value("isOk").toBool());
        QCOMPARE(model.document(), before);
        layer["zones"] = QVariantList{"A"};
        layer["opacity"] = 101;
        model.setDocumentValue("lightingLayers", QVariantList{layer});
        QVERIFY(!model.lastOperation().value("isOk").toBool());
        QCOMPARE(model.document(), before);
        model.selectDevice("darkstar_wireless");
        int physicalKeys = 0;
        for (const auto& entry : model.keys())
            if (!entry.toMap().value("id").toString().startsWith("Gesture_"))
                ++physicalKeys;
        QCOMPARE(physicalKeys, 15);
        QSet<QString> outputIds;
        for (const auto& group : {"extendedRows", "mainRows", "navigationRows", "numpadRows"})
            for (const auto& row : model.assignmentKeyboard().value(group).toList())
                for (const auto& key : row.toList())
                    outputIds.insert(key.toMap().value("id").toString());
        QCOMPARE(outputIds.size(), 118);
        QVERIFY(outputIds.contains("F24"));
        QVERIFY(outputIds.contains("KeypadEnter"));
        QVariantMap assignment{
            {"input", "Mouse1"}, {"type", "Keyboard"}, {"params", QVariantMap{{"key", "F24"}}}};
        model.setDocumentValue("assignments", QVariantList{assignment});
        QVERIFY(model.lastOperation().value("isOk").toBool());
        const auto valid = model.document();
        assignment["params"] = QVariantMap{{"key", "NonexistentOutput"}};
        model.setDocumentValue("assignments", QVariantList{assignment});
        QVERIFY(!model.lastOperation().value("isOk").toBool());
        QCOMPARE(model.document(), valid);
        open_icue::ApplicationModel restored(directory.path());
        restored.selectDevice("k70_core_rgb");
        QCOMPARE(restored.document(), before);
    }
};
QTEST_GUILESS_MAIN(NativeCoreTests)
#include "test_core.moc"
