#include "devices/hardware_backend.h"
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <algorithm>
#include <stdexcept>

namespace {
using namespace open_icue;

class DpiFixture {
  public:
    enum class Fault { None, RejectWrite, MissingReadback, ForeignReadback, LostAcknowledgement };
    QList<HidEndpoint> endpoints{{"mouse", {}, "mouse-serial", DARKSTAR_PRODUCT_ID, {}, false}};
    QList<QByteArray> reports;
    QMap<int, quint32> modes{{8, 1}, {9, 1}};
    quint32 dpi = 800;
    quint32 activeIndex = 2;
    quint16 childProduct = DARKSTAR_PRODUCT_ID;
    quint32 radioSlots = 2;
    int sessionCount = 0;
    bool hasValidIdentity = true;
    bool isDpiSupported = true;
    bool shouldRejectRollback = false;
    bool shouldRejectModeRestore = false;
    bool shouldIgnoreModeRestore = false;
    Fault fault = Fault::None;

    std::unique_ptr<HidSession> session(const HidEndpoint& endpoint) {
        ++sessionCount;
        return std::make_unique<HidSession>(
            endpoint, [this, product = endpoint.product](const QByteArray& request) {
                return exchange(product, request);
            });
    }
    QByteArray exchange(quint16 product, const QByteArray& request) {
        reports.append(request);
        const auto endpoint = quint8(request[1]);
        const auto command = quint8(request[2]);
        const auto property = quint8(request[3]);
        QByteArray response(64, 0);
        response[0] = char(endpoint & 7);
        response[1] = char(command);
        if (command == 2) {
            quint32 value = 0;
            switch (property) {
            case 0x11:
                value = CORSAIR_VENDOR_ID;
                break;
            case 0x12:
                value = hasValidIdentity ? (endpoint == 9 ? childProduct : product) : 0xffff;
                break;
            case 3:
                value = modes[endpoint];
                break;
            case 0x1e:
                value = activeIndex;
                break;
            case 0x20:
                value = dpi;
                if (!isDpiSupported)
                    response[2] = 5;
                if (fault == Fault::ForeignReadback && dpi != 800) {
                    response[0] = char((endpoint & 7) ^ 1);
                    fault = Fault::None;
                }
                break;
            case 0x36:
                value = radioSlots;
                break;
            case 0x0f:
                value = 800;
                break;
            case 0x10:
                value = 2;
                break;
            default:
                throw std::runtime_error("Fixture received an unexpected GET property");
            }
            qToLittleEndian(value, response.data() + 3);
        } else if (command == 1) {
            if (request[4] != 0)
                throw std::runtime_error("Fixture received a nonzero property high byte");
            const auto value = qFromLittleEndian<quint32>(request.constData() + 5);
            if (property == 3) {
                if (value == 1 && shouldRejectModeRestore)
                    response[2] = 5;
                else if (!(value == 1 && shouldIgnoreModeRestore))
                    modes[endpoint] = value;
            } else if (property == 0x20) {
                if (fault == Fault::RejectWrite || (shouldRejectRollback && value == 800)) {
                    response[2] = 5;
                    fault = Fault::None;
                } else if (fault == Fault::MissingReadback)
                    fault = Fault::None;
                else {
                    dpi = value;
                    if (fault == Fault::LostAcknowledgement) {
                        fault = Fault::None;
                        throw std::runtime_error("HID response timeout");
                    }
                }
            } else
                throw std::runtime_error("Fixture received an unexpected SET property");
        } else if (command != 0x0d && command != 6 && command != 7 && command != 5)
            throw std::runtime_error("Fixture received an unexpected HID command");
        return response;
    }
    int countSet(quint8 property) const {
        return std::count_if(reports.begin(), reports.end(), [property](const auto& report) {
            return quint8(report[2]) == 1 && quint8(report[3]) == property;
        });
    }
};
} // namespace

class NativeHardwareTests final : public QObject {
    Q_OBJECT
  private slots:
    void excludesBootAndUnrecognizedInterfaces() {
        const auto command =
            QByteArray::fromHex("0642ff0901a101150026ff007508954009018102954009019102c0");
        QVERIFY(open_icue::isCommandDescriptor(0x1bb2, command));
        QVERIFY(open_icue::isCommandDescriptor(0x1bdc, command));
        QVERIFY(!open_icue::isCommandDescriptor(0xffff, command));
        QVERIFY(!open_icue::isCommandDescriptor(0x1bb2, QByteArray::fromHex("05010902a101")));
        QVERIFY(!open_icue::isCommandDescriptor(
            0x1bb2, QByteArray::fromHex("0642ff0902a101150026ff007508954009028102c0")));
    }
    void rejectsFirmwareErrorsAndWrongChildResponses() {
        QByteArray response(64, 0);
        response[1] = 2;
        qToLittleEndian(quint32(1000), response.data() + 3);
        QCOMPARE(open_icue::bragiProperty(response, 8), 1000U);
        QVERIFY_EXCEPTION_THROWN(open_icue::bragiProperty(response, 9), std::runtime_error);
        response[2] = 5;
        QVERIFY_EXCEPTION_THROWN(open_icue::bragiProperty(response, 8), std::runtime_error);
        response[2] = 0;
        response[1] = 6;
        QVERIFY_EXCEPTION_THROWN(open_icue::bragiProperty(response, 8), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(open_icue::bragiProperty(response.first(4), 8),
                                 std::runtime_error);
    }
    void reportPrefixAndSizeBounds() {
        const auto packet = open_icue::bragiReport(9, 2, QByteArray(1, char(0x0f)));
        QCOMPARE(packet.size(), 65);
        QCOMPARE(quint8(packet[0]), quint8(0));
        QCOMPARE(quint8(packet[1]), quint8(9));
        QCOMPARE(quint8(packet[3]), quint8(0x0f));
        QVERIFY_EXCEPTION_THROWN(open_icue::bragiReport(7, 2), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(open_icue::bragiReport(8, 6, QByteArray(63, 0)),
                                 std::invalid_argument);
    }
    void unifiedDpiUsesTheObservedPropertyReport() {
        QList<QByteArray> reports;
        HidSession session({"test", {}, {}, DARKSTAR_PRODUCT_ID, {}, false},
                           [&reports](const QByteArray& request) {
                               reports.append(request);
                               QByteArray response(64, 0);
                               response[1] = request[2];
                               return response;
                           });
        session.set(8, 0x20, 26000);
        QCOMPARE(reports.size(), 1);
        QCOMPARE(reports.first().first(9), QByteArray::fromHex("000801200090650000"));
        QCOMPARE(reports.first().size(), 65);
        QCOMPARE(reports.first().mid(9), QByteArray(56, 0));
    }
    void activeDpiHasAcknowledgementReadbackAndBoundedLifetime() {
        QTemporaryDir directory;
        DpiFixture fixture;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        const auto result = backend.applyDpi("darkstar_wireless", 26000, 26000);
        QVERIFY(result.value("isOk").toBool());
        QVERIFY(result.value("hasAcknowledgement").toBool());
        QVERIFY(result.value("hasReadback").toBool());
        QCOMPARE(result.value("storage").toString(), "software");
        QCOMPARE(result.value("commandEndpoint").toInt(), 8);
        QCOMPARE(result.value("path").toString(), "mouse");
        QCOMPARE(result.value("activeDpiIndex").toUInt(), 2U);
        QCOMPARE(fixture.countSet(0x20), 1);
        QCOMPARE(fixture.countSet(0x1e), 0);
        QCOMPARE(fixture.dpi, 26000U);
        QCOMPARE(fixture.modes[8], 2U);
        backend.stopLighting();
        QCOMPARE(fixture.modes[8], 1U);
    }
    void invalidDpiNeverReachesDeviceDiscovery() {
        int discoveries = 0;
        HardwareBackend backend({}, [&] {
            ++discoveries;
            return QList<HidEndpoint>{};
        });
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("k70_core_rgb", 800, 800), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 99, 99),
                                 std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 26001, 26001),
                                 std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 800, 1200),
                                 std::invalid_argument);
        QCOMPARE(discoveries, 0);
    }
    void releaseReportsConfirmedModesAndNeverHidesRestoreErrors_data() {
        QTest::addColumn<bool>("isRejected");
        QTest::addColumn<bool>("isIgnored");
        QTest::newRow("confirmed-mode") << false << false;
        QTest::newRow("rejected-mode") << true << false;
        QTest::newRow("ack-without-mode-change") << false << true;
    }
    void releaseReportsConfirmedModesAndNeverHidesRestoreErrors() {
        QFETCH(bool, isRejected);
        QFETCH(bool, isIgnored);
        QTemporaryDir directory;
        DpiFixture fixture;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        QVERIFY(backend.applyDpi("darkstar_wireless", 1750, 1750).value("isOk").toBool());
        fixture.shouldRejectModeRestore = isRejected;
        fixture.shouldIgnoreModeRestore = isIgnored;
        const auto released = backend.releaseControl();
        QCOMPARE(released.value("isOk").toBool(), !isRejected && !isIgnored);
        QCOMPARE(released.value("hasModeReadback").toBool(), !isRejected && !isIgnored);
        QCOMPARE(released.value("releasedSessions").toInt(), 1);
        QCOMPARE(released.value("errors").toList().isEmpty(), !isRejected && !isIgnored);
        if (isRejected || isIgnored)
            QVERIFY(!released.value("error").toString().isEmpty());
        else
            QCOMPARE(fixture.modes[8], 1U);
        const auto empty = backend.releaseControl();
        QVERIFY(empty.value("isOk").toBool());
        QVERIFY(!empty.value("hasModeReadback").toBool());
        QCOMPARE(empty.value("releasedSessions").toInt(), 0);
    }
    void unsupportedDpiPropertyDoesNotTakeControl() {
        QTemporaryDir directory;
        DpiFixture fixture;
        fixture.isDpiSupported = false;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 3000, 3000),
                                 std::runtime_error);
        QCOMPARE(fixture.countSet(0x20), 0);
        QCOMPARE(fixture.countSet(3), 0);
        QCOMPARE(fixture.modes[8], 1U);
    }
    void dpiProbeRequiresOneVerifiedWiredMouse() {
        QTemporaryDir directory;
        DpiFixture fixture;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        QVERIFY(backend.probe().value("canApplyDpi").toBool());
        QCOMPARE(fixture.countSet(3), 0);
        fixture.hasValidIdentity = false;
        QVERIFY(!backend.probe().value("canApplyDpi").toBool());
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 3000, 3000),
                                 std::runtime_error);
        QCOMPARE(fixture.countSet(3), 0);
        QCOMPARE(fixture.countSet(0x20), 0);
        fixture.hasValidIdentity = true;
        fixture.endpoints.append(
            {"other-mouse", {}, "other-serial", DARKSTAR_PRODUCT_ID, {}, false});
        QVERIFY(!backend.probe().value("canApplyDpi").toBool());
        const auto sessionsBeforeApply = fixture.sessionCount;
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 3000, 3000),
                                 std::runtime_error);
        QCOMPARE(fixture.sessionCount, sessionsBeforeApply);
        fixture.endpoints = {{"receiver", {}, {}, SLIPSTREAM_PRODUCT_ID, {}, false}};
        QVERIFY(!backend.probe().value("canApplyDpi").toBool());
        QCOMPARE(fixture.countSet(3), 0);
    }
    void dpiFailuresRestorePreviousValueAndMode_data() {
        QTest::addColumn<int>("fault");
        QTest::newRow("firmware-rejected") << int(DpiFixture::Fault::RejectWrite);
        QTest::newRow("ack-without-application") << int(DpiFixture::Fault::MissingReadback);
        QTest::newRow("foreign-readback") << int(DpiFixture::Fault::ForeignReadback);
        QTest::newRow("lost-ack-after-application") << int(DpiFixture::Fault::LostAcknowledgement);
    }
    void dpiFailuresRestorePreviousValueAndMode() {
        QFETCH(int, fault);
        QTemporaryDir directory;
        DpiFixture fixture;
        fixture.fault = DpiFixture::Fault(fault);
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 3000, 3000),
                                 std::runtime_error);
        QCOMPARE(fixture.dpi, 800U);
        QCOMPARE(fixture.modes[8], 1U);
        QCOMPARE(fixture.countSet(0x20), 2);
    }
    void dpiRollbackFailureIsReported() {
        QTemporaryDir directory;
        DpiFixture fixture;
        fixture.fault = DpiFixture::Fault::RejectWrite;
        fixture.shouldRejectRollback = true;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        try {
            backend.applyDpi("darkstar_wireless", 3000, 3000);
            QFAIL("DPI rollback failure was not reported");
        } catch (const std::runtime_error& error) {
            QVERIFY(QString::fromUtf8(error.what()).contains("DPI rollback failed"));
        }
        QCOMPARE(fixture.modes[8], 1U);
    }
    void wirelessDpiTargetsOnlyTheVerifiedMouseChild() {
        QTemporaryDir directory;
        DpiFixture fixture;
        fixture.endpoints = {
            {"receiver", {}, "receiver-serial", SLIPSTREAM_PRODUCT_ID, {}, false},
            {"headset", {}, "headset-serial", VIRTUOSO_RECEIVER_PRODUCT_ID, {}, true}};
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) {
                if (endpoint.product != SLIPSTREAM_PRODUCT_ID)
                    throw std::runtime_error("An unrelated receiver was opened");
                return fixture.session(endpoint);
            });
        const auto result = backend.applyDpi("darkstar_wireless", 1200, 1200);
        QVERIFY(result.value("isWireless").toBool());
        QCOMPARE(result.value("commandEndpoint").toInt(), 9);
        QCOMPARE(fixture.modes[8], 1U);
        QCOMPARE(fixture.modes[9], 2U);
        for (const auto& report : fixture.reports)
            if (quint8(report[2]) == 1)
                QCOMPARE(quint8(report[1]), quint8(9));
        backend.stopLighting();
        QCOMPARE(fixture.modes[9], 1U);
    }
    void wirelessDpiRejectsUnconnectedOrDifferentChild_data() {
        QTest::addColumn<int>("product");
        QTest::addColumn<int>("radioSlots");
        QTest::newRow("different-child") << 0xffff << 2;
        QTest::newRow("disconnected-child") << int(DARKSTAR_PRODUCT_ID) << 0;
    }
    void wirelessDpiRejectsUnconnectedOrDifferentChild() {
        QFETCH(int, product);
        QFETCH(int, radioSlots);
        QTemporaryDir directory;
        DpiFixture fixture;
        fixture.childProduct = quint16(product);
        fixture.radioSlots = quint32(radioSlots);
        fixture.endpoints = {{"receiver", {}, {}, SLIPSTREAM_PRODUCT_ID, {}, false}};
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 1200, 1200),
                                 std::runtime_error);
        QCOMPARE(fixture.countSet(3), 0);
        QCOMPARE(fixture.countSet(0x20), 0);
    }
    void dpiSharesTheLightingSessionAndPreservesItOnFailure() {
        QTemporaryDir directory;
        DpiFixture fixture;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.endpoints; },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        const QVariantList layers{QVariantMap{{"type", "StaticCustomLightingTypeData"},
                                              {"zones", QStringList{"MouseLogoLed"}},
                                              {"colors", QVariantList{"#ff0000"}}}};
        backend.applyLighting("darkstar_wireless", layers, {QVariantMap{{"id", "MouseLogoLed"}}});
        backend.applyDpi("darkstar_wireless", 1200, 1200);
        QCOMPARE(fixture.sessionCount, 1);
        fixture.fault = DpiFixture::Fault::RejectWrite;
        QVERIFY_EXCEPTION_THROWN(backend.applyDpi("darkstar_wireless", 3000, 3000),
                                 std::runtime_error);
        QCOMPARE(fixture.sessionCount, 1);
        QCOMPARE(fixture.dpi, 1200U);
        QCOMPARE(fixture.modes[8], 2U);
        backend.stopLighting();
        QCOMPARE(fixture.modes[8], 1U);
    }
    void resourceWriteChunksAreBoundedAndPadded_data() {
        QTest::addColumn<int>("size");
        for (const int size : {1, 8, 16, 39, 56, 57, 58, 118, 119, 123, 369, 4096})
            QTest::newRow(qPrintable(QString::number(size))) << size;
    }
    void resourceWriteChunksAreBoundedAndPadded() {
        QFETCH(int, size);
        QByteArray source(size, 0);
        for (int index = 0; index < size; ++index)
            source[index] = char((index * 17 + 1) & 255);
        QList<QByteArray> writes;
        const HidEndpoint endpoint{"fixture", {}, {}, DARKSTAR_PRODUCT_ID, {}, false};
        HidSession session(endpoint, [&](const QByteArray& report) {
            const auto command = quint8(report[2]);
            if (command == 6 || command == 7)
                writes.append(report);
            QByteArray reply(64, 0);
            reply[0] = char(quint8(report[1]) & 7);
            reply[1] = char(command);
            return reply;
        });
        session.writeResource(5, source);
        const int firstLength = std::min(57, size);
        QCOMPARE(writes.size(), 1 + (std::max(0, size - firstLength) + 60) / 61);
        QCOMPARE(qFromLittleEndian<quint32>(writes.first().constData() + 4), quint32(size));
        QByteArray reconstructed;
        int consumed = 0;
        for (int index = 0; index < writes.size(); ++index) {
            const auto& report = writes.at(index);
            QCOMPARE(report.size(), 65);
            QCOMPARE(quint8(report[0]), quint8(0));
            QCOMPARE(quint8(report[1]), quint8(8));
            QCOMPARE(quint8(report[2]), quint8(index == 0 ? 6 : 7));
            QCOMPARE(quint8(report[3]), quint8(1));
            const int payloadOffset = index == 0 ? 8 : 4;
            const int payloadLength = std::min(index == 0 ? 57 : 61, size - consumed);
            reconstructed += report.mid(payloadOffset, payloadLength);
            const int paddingOffset = payloadOffset + payloadLength;
            QCOMPARE(report.mid(paddingOffset), QByteArray(65 - paddingOffset, 0));
            consumed += payloadLength;
        }
        QCOMPARE(reconstructed, source);
    }
    void onlySelectedZonesReceiveLayerColor() {
        const QVariantList zones{QVariantMap{{"id", "A"}}, QVariantMap{{"id", "B"}}};
        const QVariantList layers{QVariantMap{{"type", "StaticCustomLightingTypeData"},
                                              {"zones", QStringList{"A"}},
                                              {"colors", QVariantList{"#ff0000"}},
                                              {"opacity", 100}}};
        const auto frame = open_icue::lightingFrame(layers, zones, 0);
        QCOMPARE(frame[0], qRgb(255, 0, 0));
        QCOMPARE(frame[1], qRgb(0, 0, 0));
        QCOMPARE(open_icue::k70LedIndex("A"), 4);
        QCOMPARE(open_icue::k70LedIndex("F12"), 69);
        QCOMPARE(open_icue::k70LedIndex("LeftShift"), 106);
        QCOMPARE(open_icue::k70LedIndex("F24"), -1);
        QCOMPARE(open_icue::k70LedIndex("Mouse1"), -1);
    }
    void reactiveEffectsFailBeforeDeviceAccess() {
        const QVariantList layers{
            QVariantMap{{"type", "RippleLightingTypeData"}, {"zones", QStringList{"all"}}}};
        QVERIFY_EXCEPTION_THROWN(open_icue::lightingFrame(layers, {QVariantMap{{"id", "A"}}}, 0),
                                 std::runtime_error);
    }
    void keyTriggerPreservesOtherLayersAndUsesOnlyActualPresses() {
        const QVariantList zones{QVariantMap{{"id", "A"}}, QVariantMap{{"id", "B"}}};
        const QVariantMap base{{"type", "StaticCustomLightingTypeData"},
                               {"zones", QStringList{"all"}},
                               {"colors", QVariantList{"#0000ff"}}};
        QVariantMap trigger{{"type", "TypeLightingTypeData"},
                            {"zones", QStringList{"all"}},
                            {"colors", QVariantList{"#ff0000"}},
                            {"duration", 2}};
        QMap<int, double> presses;
        QCOMPARE(lightingFrame({base, trigger}, zones, 10, &presses),
                 (QList<QRgb>{qRgb(0, 0, 255), qRgb(0, 0, 255)}));
        presses.insert(k70LedIndex("A"), 10);
        QCOMPARE(lightingFrame({base, trigger}, zones, 10, &presses),
                 (QList<QRgb>{qRgb(255, 0, 0), qRgb(0, 0, 255)}));
        const auto fading = lightingFrame({base, trigger}, zones, 11, &presses);
        QVERIFY(qRed(fading[0]) >= 127 && qRed(fading[0]) <= 128);
        QVERIFY(qBlue(fading[0]) >= 127 && qBlue(fading[0]) <= 128);
        QCOMPARE(lightingFrame({base, trigger}, zones, 12, &presses),
                 (QList<QRgb>{qRgb(0, 0, 255), qRgb(0, 0, 255)}));
        trigger["zones"] = QStringList{"B"};
        QCOMPARE(lightingFrame({base, trigger}, zones, 10, &presses),
                 (QList<QRgb>{qRgb(0, 0, 255), qRgb(0, 0, 255)}));
    }
    void keyTriggerUsesInjectedInputAndClosesWithOwnedControl() {
        DpiFixture fixture;
        fixture.endpoints[0].product = K70_CORE_PRODUCT_ID;
        QByteArray down(15, 0);
        down[0] = 8;
        down[2] = 4;
        QList<QByteArray> queued{down};
        auto input = std::make_shared<KeyboardLightingInput>([&] {
            auto reports = queued;
            queued.clear();
            return reports;
        });
        HardwareBackend backend(
            {}, [&] { return fixture.endpoints; },
            [&](const auto& endpoint) { return fixture.session(endpoint); }, input);
        const QVariantList zones{QVariantMap{{"id", "A"}}, QVariantMap{{"id", "B"}}};
        const QVariantList layers{QVariantMap{{"type", "StaticCustomLightingTypeData"},
                                              {"zones", QStringList{"all"}},
                                              {"colors", QVariantList{"#0000ff"}}},
                                  QVariantMap{{"type", "TypeLightingTypeData"},
                                              {"zones", QStringList{"all"}},
                                              {"colors", QVariantList{"#ff0000"}}}};
        const auto result = backend.applyLighting("k70_core_rgb", layers, zones, 4);
        QVERIFY(result.value("hasAcknowledgement").toBool());
        QCOMPARE(result.value("reactiveEventCount").toULongLong(), quint64(1));
        QCOMPARE(result.value("previewColors").toMap().value("A").toString(), QString("#ff0000"));
        QCOMPARE(result.value("previewColors").toMap().value("B").toString(), QString("#0000ff"));
        QByteArray payload;
        for (const auto& report : fixture.reports) {
            if (report[2] == 6)
                payload += report.mid(8, 57);
            else if (report[2] == 7)
                payload += report.mid(4, std::min(61, 369 - int(payload.size())));
        }
        QCOMPARE(payload.size(), 369);
        QCOMPARE(quint8(payload[4]), quint8(255));
        QCOMPARE(quint8(payload[250]), quint8(0));
        QCOMPARE(quint8(payload[251]), quint8(255));
        QVERIFY(backend.releaseControl().value("isOk").toBool());
        QCOMPARE(input->eventCount(), quint64(0));
        QCOMPARE(fixture.modes.value(8), quint32(1));
    }
    void keyboardPressStateSurvivesFramesForOtherProfileDevices() {
        DpiFixture fixture;
        fixture.endpoints = {{"keyboard", {}, "keyboard-serial", K70_CORE_PRODUCT_ID, {}, false},
                             {"mouse", {}, "mouse-serial", DARKSTAR_PRODUCT_ID, {}, false}};
        QByteArray down(21, 0);
        down[0] = 1;
        down[2] = char(1 << 4);
        auto input = std::make_shared<KeyboardLightingInput>([down] { return QList{down}; });
        HardwareBackend backend(
            {}, [&] { return fixture.endpoints; },
            [&](const auto& endpoint) { return fixture.session(endpoint); }, input);
        const QVariantList keyboardLayers{QVariantMap{{"type", "TypeLightingTypeData"},
                                                      {"zones", QStringList{"all"}},
                                                      {"colors", QVariantList{"#ff0000"}},
                                                      {"duration", 2}}};
        const QVariantList mouseLayers{QVariantMap{{"type", "StaticCustomLightingTypeData"},
                                                   {"zones", QStringList{"all"}},
                                                   {"colors", QVariantList{"#00ff00"}}}};
        backend.applyLighting("k70_core_rgb", keyboardLayers, {QVariantMap{{"id", "A"}}}, 4);
        QCOMPARE(input->eventCount(), quint64(1));
        backend.applyLighting("darkstar_wireless", mouseLayers,
                              {QVariantMap{{"id", "MouseLogoLed"}}}, 4);
        const auto next =
            backend.applyLighting("k70_core_rgb", keyboardLayers, {QVariantMap{{"id", "A"}}}, 5);
        QCOMPARE(input->eventCount(), quint64(1));
        const QColor color(next.value("previewColors").toMap().value("A").toString());
        QVERIFY(std::abs(color.red() - 128) <= 1);
        QCOMPARE(color.green(), 0);
    }
    void importedGradientUsesUnevenTransitionTimesAndAlpha() {
        const QVariantList zones{QVariantMap{{"id", "A"}}, QVariantMap{{"id", "B"}}};
        const QVariantMap timeline{
            {"type", "GradientLightingTypeData.lightings.cue.corsair.com"},
            {"zones", QStringList{"A"}},
            {"colors", QVariantList{"#ffff0000", "#8000ff00", "#ff0000ff"}},
            {"duration", 4.0},
            {"isLooping", true},
            {"opacity", 50},
            {"transitions", QVariantList{QVariantMap{{"time", 0.0}, {"color", "#ffff0000"}},
                                         QVariantMap{{"time", 0.25}, {"color", "#8000ff00"}},
                                         QVariantMap{{"time", 1.0}, {"color", "#ff0000ff"}}}}};
        const auto start = lightingFrame({timeline}, zones, 0);
        QCOMPARE(start[0], qRgb(128, 0, 0));
        QCOMPARE(start[1], qRgb(0, 0, 0));
        const auto quarter = lightingFrame({timeline}, zones, 1);
        QCOMPARE(quarter[0], qRgb(0, 64, 0));
        const auto middle = lightingFrame({timeline}, zones, 2.5);
        QVERIFY(std::abs(qGreen(middle[0]) - 48) <= 1);
        QVERIFY(std::abs(qBlue(middle[0]) - 48) <= 1);
        QCOMPARE(qRed(middle[0]), 0);
        QCOMPARE(lightingFrame({timeline}, zones, 4), start);
    }
    void importedPerKeyTimelineUsesPressAgeAndPreservesOtherKeys() {
        const QVariantMap base{{"type", "StaticCustomLightingTypeData"},
                               {"zones", QStringList{"all"}},
                               {"colors", QVariantList{"#0000ff"}}};
        const QVariantMap reactive{
            {"type", "TypeLightingTypeData"},
            {"zones", QStringList{"all"}},
            {"duration", 2},
            {"isLooping", false},
            {"transitions", QVariantList{QVariantMap{{"time", 0.0}, {"color", "#ffff0000"}},
                                         QVariantMap{{"time", 1.0}, {"color", "#00ff0000"}}}}};
        const QVariantList zones{QVariantMap{{"id", "A"}}, QVariantMap{{"id", "B"}}};
        QMap<int, double> presses{{k70LedIndex("A"), 10.0}};
        QCOMPARE(lightingFrame({base, reactive}, zones, 10, &presses)[0], qRgb(255, 0, 0));
        QCOMPARE(lightingFrame({base, reactive}, zones, 10, &presses)[1], qRgb(0, 0, 255));
        const auto fading = lightingFrame({base, reactive}, zones, 11, &presses)[0];
        QVERIFY(std::abs(qRed(fading) - 128) <= 1);
        QVERIFY(std::abs(qBlue(fading) - 128) <= 1);
        QCOMPARE(lightingFrame({base, reactive}, zones, 12, &presses)[0], qRgb(0, 0, 255));
    }
    void importedWaveHasSpatialPhaseAndRejectsInvalidParameters() {
        QVariantMap wave{
            {"type", "WaveLightingTypeData"},
            {"zones", QStringList{"all"}},
            {"duration", 2.0},
            {"isLooping", true},
            {"waveVelocity", 100},
            {"waveTailLength", 200},
            {"waveAngle", 90},
            {"isWaveDoublesided", false},
            {"transitions", QVariantList{QVariantMap{{"time", 0.0}, {"color", "#ffff0000"}},
                                         QVariantMap{{"time", 1.0}, {"color", "#ff0000ff"}}}}};
        const QVariantList zones{
            QVariantMap{{"id", "A"},
                        {"geometry", QVariantList{QVariantMap{{"x", 0}, {"width", 0}}}}},
            QVariantMap{{"id", "B"},
                        {"geometry", QVariantList{QVariantMap{{"x", 100}, {"width", 0}}}}}};
        const auto frame = lightingFrame({wave}, zones, 0.5);
        QVERIFY(frame[0] != frame[1]);
        QVERIFY(lightingFrame({wave}, zones, 1.0)[0] != frame[0]);
        wave["waveVelocity"] = 0;
        QVERIFY_EXCEPTION_THROWN(lightingFrame({wave}, zones, 0), std::invalid_argument);
    }
    void importedStaticColorPreservesAlpha() {
        const QVariantList layers{QVariantMap{{"type", "StaticCustomLightingTypeData"},
                                              {"zones", QStringList{"all"}},
                                              {"colors", QVariantList{"#800000ff"}},
                                              {"opacity", 50}}};
        const auto frame = lightingFrame(layers, {QVariantMap{{"id", "A"}}}, 0);
        QCOMPARE(frame[0], qRgb(0, 0, 64));
    }
    void invalidImportedTimelineFailsBeforeDeviceDiscovery() {
        QVariantMap timeline{
            {"type", "GradientLightingTypeData"},
            {"zones", QStringList{"all"}},
            {"duration", 2},
            {"isLooping", true},
            {"transitions", QVariantList{QVariantMap{{"time", 0.0}, {"color", "#ff0000"}},
                                         QVariantMap{{"time", 0.0}, {"color", "#0000ff"}}}}};
        int discoveries = 0;
        HardwareBackend backend({}, [&] {
            ++discoveries;
            return QList<HidEndpoint>{};
        });
        QVERIFY_EXCEPTION_THROWN(
            backend.applyLighting("darkstar_wireless", {timeline}, {QVariantMap{{"id", "A"}}}),
            std::invalid_argument);
        QCOMPARE(discoveries, 0);
        timeline["transitions"] = QVariantList{QVariantMap{{"time", 0.1}, {"color", "#ff0000"}},
                                               QVariantMap{{"time", 1.0}, {"color", "#0000ff"}}};
        QVERIFY_EXCEPTION_THROWN(lightingFrame({timeline}, {}, 0), std::invalid_argument);
        timeline["transitions"] = QVariantList{QVariantMap{{"time", 0.0}, {"color", "#ff0000"}},
                                               QVariantMap{{"time", 1.0}, {"color", "#0000ff"}}};
        timeline["duration"] = 0;
        QVERIFY_EXCEPTION_THROWN(lightingFrame({timeline}, {}, 0), std::invalid_argument);
    }
};
QTEST_GUILESS_MAIN(NativeHardwareTests)
#include "test_hardware.moc"
