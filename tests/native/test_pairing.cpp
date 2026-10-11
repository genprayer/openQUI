#include "devices/hardware_backend.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

namespace {
using namespace open_icue;
struct DeviceState {
    int mode = 1;
    int childMode = 1;
    QMap<int, int> handles;
    QMap<int, QByteArray> resources{{5, QByteArray(8, 'x')}, {6, QByteArray(16, 'y')}};
};
struct WrittenResource {
    quint16 product;
    int resource;
    QByteArray bytes;
};
// Responses follow the observed Bragi report layout; faults never access host HID.
class PairingFixture {
  public:
    QMap<quint16, DeviceState> devices;
    QList<WrittenResource> writes;
    quint32 radioSlots = 0;
    bool shouldFailMouseKey = false;
    bool hasWiredMouse = true;
    QList<HidEndpoint> discover() const {
        QList<HidEndpoint> endpoints;
        for (const auto product : {SLIPSTREAM_PRODUCT_ID, DARKSTAR_PRODUCT_ID}) {
            if (product == DARKSTAR_PRODUCT_ID && !hasWiredMouse)
                continue;
            endpoints.append(
                {QString::number(product), {}, QString::number(product), product, {}, false});
        }
        return endpoints;
    }
    std::unique_ptr<HidSession> session(const HidEndpoint& endpoint) {
        return std::make_unique<HidSession>(
            endpoint, [this, product = endpoint.product](const QByteArray& request) {
                return exchange(product, request);
            });
    }
    QByteArray exchange(quint16 product, const QByteArray& request) {
        auto& device = devices[product];
        const int child = quint8(request[1]) & 7;
        const int command = quint8(request[2]);
        const int argument = quint8(request[3]);
        QByteArray response(64, 0);
        response[0] = char(child);
        response[1] = char(command);
        switch (command) {
        case 2: {
            quint32 value = 0;
            switch (argument) {
            case 3:
                value = child ? device.childMode : device.mode;
                break;
            case 0x11:
                value = CORSAIR_VENDOR_ID;
                break;
            case 0x12:
                value = child ? DARKSTAR_PRODUCT_ID : product;
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
                response[2] = 5;
                break;
            }
            qToLittleEndian(value, response.data() + 3);
            break;
        }
        case 1:
            if (argument != 3)
                throw std::runtime_error("Fixture received an unexpected SET property");
            (child ? device.childMode : device.mode) =
                qFromLittleEndian<quint16>(request.constData() + 5);
            break;
        case 0x0d:
            device.handles[argument] = qFromLittleEndian<quint16>(request.constData() + 4);
            break;
        case 5:
            device.handles.remove(quint8(request[4]));
            break;
        case 9:
            qToLittleEndian(quint32(device.resources[device.handles[argument]].size()),
                            response.data() + 5);
            break;
        case 8: {
            const auto bytes = device.resources[device.handles[argument]];
            std::copy(bytes.begin(), bytes.end(), response.begin() + 3);
            break;
        }
        case 6: {
            const int resource = device.handles[argument];
            const auto length = qFromLittleEndian<quint32>(request.constData() + 4);
            const auto bytes = request.mid(8, length);
            writes.append({product, resource, bytes});
            if (product == DARKSTAR_PRODUCT_ID && resource == 6 && shouldFailMouseKey) {
                shouldFailMouseKey = false;
                response[2] = 9;
            } else
                device.resources[resource] = bytes;
            break;
        }
        default:
            throw std::runtime_error("Fixture received an unexpected HID command");
        }
        return response;
    }
};
QJsonObject readJournal(const QString& directory) {
    QFile file(directory + "/hardware/pairing-1bdc.json");
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Test journal missing");
    return QJsonDocument::fromJson(file.readAll()).object();
}
} // namespace

class NativePairingTests final : public QObject {
    Q_OBJECT
  private slots:
    void retriesInterruptedPairingWithTheSameCredentials() {
        QTemporaryDir directory;
        PairingFixture fixture;
        fixture.shouldFailMouseKey = true;
        const auto discovery = [&] { return fixture.discover(); };
        const auto sessions = [&](const HidEndpoint& endpoint) {
            return fixture.session(endpoint);
        };
        {
            HardwareBackend backend(directory.path(), discovery, sessions);
            QVERIFY_EXCEPTION_THROWN(backend.pairDarkstar(), std::runtime_error);
        }
        QCOMPARE(readJournal(directory.path()).value("phase").toString(), "receiver-written");
        QCOMPARE(fixture.devices[SLIPSTREAM_PRODUCT_ID].mode, 1);
        QCOMPARE(fixture.devices[DARKSTAR_PRODUCT_ID].mode, 1);
        const auto originalKey = fixture.devices[SLIPSTREAM_PRODUCT_ID].resources[6];
        // A killed process cannot run destructors; leave both devices in owned software mode.
        fixture.devices[SLIPSTREAM_PRODUCT_ID].mode = 2;
        fixture.devices[DARKSTAR_PRODUCT_ID].mode = 2;
        HardwareBackend recovered(directory.path(), discovery, sessions);
        QVERIFY(recovered.restoreInterruptedPairing().isEmpty());
        QCOMPARE(fixture.devices[SLIPSTREAM_PRODUCT_ID].mode, 1);
        QCOMPARE(fixture.devices[DARKSTAR_PRODUCT_ID].mode, 1);
        QVERIFY(recovered.pairDarkstar().value("isCredentialsWritten").toBool());
        QVERIFY(fixture.devices[DARKSTAR_PRODUCT_ID].resources[6] == originalKey);
        QVERIFY(fixture.devices[SLIPSTREAM_PRODUCT_ID].resources[6] == originalKey);
        QCOMPARE(readJournal(directory.path()).value("phase").toString(), "credentials-written");
        const auto permissions =
            QFileInfo(directory.path() + "/hardware/pairing-1bdc.json").permissions();
        QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                 QFileDevice::ReadOther | QFileDevice::WriteOther)));
        for (int index = 0; index < fixture.writes.size(); index += 2) {
            QCOMPARE(fixture.writes[index].resource, 5);
            QCOMPARE(fixture.writes[index + 1].resource, 6);
            QCOMPARE(fixture.writes[index].product, fixture.writes[index + 1].product);
        }
    }
    void rejectsAnUnrelatedLiveRadioDeviceWithoutWriting() {
        QTemporaryDir directory;
        PairingFixture fixture;
        fixture.radioSlots = 4;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.discover(); },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        QVERIFY_EXCEPTION_THROWN(backend.pairDarkstar(), std::runtime_error);
        QVERIFY(fixture.writes.isEmpty());
        QVERIFY(!QFile::exists(directory.path() + "/hardware/pairing-1bdc.json"));
    }
    void anAlreadyPairedMouseDoesNotRewriteKeys() {
        QTemporaryDir directory;
        PairingFixture fixture;
        fixture.radioSlots = 2;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.discover(); },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        const auto result = backend.pairDarkstar();
        QVERIFY(result.value("isAlreadyPaired").toBool());
        QVERIFY(fixture.writes.isEmpty());
        QCOMPARE(fixture.devices[SLIPSTREAM_PRODUCT_ID].mode, 1);
        QCOMPARE(fixture.devices[DARKSTAR_PRODUCT_ID].mode, 1);
    }
    void wirelessLightingUsesTheMouseEndpointAndRestoresItsMode() {
        QTemporaryDir directory;
        PairingFixture fixture;
        fixture.hasWiredMouse = false;
        HardwareBackend backend(
            directory.path(), [&] { return fixture.discover(); },
            [&](const HidEndpoint& endpoint) { return fixture.session(endpoint); });
        const QVariantList zones{QVariantMap{{"id", "MouseLogoLed"}}};
        const QVariantList layers{QVariantMap{{"type", "StaticCustomLightingTypeData"},
                                              {"zones", QStringList{"MouseLogoLed"}},
                                              {"colors", QVariantList{"#ff0000"}}}};
        QVERIFY(backend.applyLighting("darkstar_wireless", layers, zones)
                    .value("hasAcknowledgement")
                    .toBool());
        QCOMPARE(fixture.devices[SLIPSTREAM_PRODUCT_ID].mode, 1);
        QCOMPARE(fixture.devices[SLIPSTREAM_PRODUCT_ID].childMode, 2);
        QCOMPARE(fixture.writes.size(), 1);
        QCOMPARE(fixture.writes.first().bytes.size(), 39);
        QCOMPARE(quint8(fixture.writes.first().bytes[9]), quint8(255));
        backend.stopLighting();
        QCOMPARE(fixture.devices[SLIPSTREAM_PRODUCT_ID].childMode, 1);
    }
};
QTEST_GUILESS_MAIN(NativePairingTests)
#include "test_pairing.moc"
