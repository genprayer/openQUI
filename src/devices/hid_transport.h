#pragma once
#include <QByteArray>
#include <QString>
#include <QVariantMap>
#include <functional>
#include <memory>

namespace open_icue {
constexpr quint16 CORSAIR_VENDOR_ID = 0x1b1c;
constexpr quint16 K70_CORE_PRODUCT_ID = 0x1bfd;
constexpr quint16 DARKSTAR_PRODUCT_ID = 0x1bb2;
constexpr quint16 SLIPSTREAM_PRODUCT_ID = 0x1bdc;
constexpr quint16 VIRTUOSO_RECEIVER_PRODUCT_ID = 0x0a42;

/** A descriptor-verified command interface, independent of changing hidraw numbers. */
struct HidEndpoint {
    QString path, sysPath, serial;
    quint16 product = 0;
    QByteArray descriptor;
    bool isV2w = false;
};
/** Enumerate only the supported vendor command interfaces; never read input events. */
QList<HidEndpoint> discoverCommandEndpoints();
/** Descriptor allowlist shared by enumeration and the open-time identity guard. */
bool isCommandDescriptor(quint16 product, const QByteArray& descriptor);
/** Construct a Linux output report, including the unnumbered-report zero prefix. */
QByteArray bragiReport(quint8 endpoint, quint8 command, const QByteArray& arguments = {});
/** Reject stale responses, firmware errors and truncated property values. */
quint32 bragiProperty(const QByteArray& response, quint8 endpoint);

/** Exclusive, bounded HID transaction session. Restores owned software modes on close. */
class HidSession {
  public:
    using ReportExchange = std::function<QByteArray(const QByteArray&)>;
    explicit HidSession(const HidEndpoint& endpoint);
    /** Supply a report transport for deterministic protocol and recovery tests. */
    HidSession(const HidEndpoint& endpoint, ReportExchange transport);
    ~HidSession();
    HidSession(const HidSession&) = delete;
    HidSession& operator=(const HidSession&) = delete;
    QByteArray exchange(const QByteArray& report,
                        const std::function<bool(const QByteArray&)>& matches);
    QByteArray bragi(quint8 endpoint, quint8 command, const QByteArray& arguments = {});
    quint32 get(quint8 endpoint, quint8 property);
    void set(quint8 endpoint, quint8 property, quint16 value);
    void enterSoftwareMode(quint8 endpoint = 8);
    void restoreMode();
    QByteArray readResource(quint16 resource, int expectedLength);
    void writeResource(quint16 resource, const QByteArray& data, quint8 handle = 1);
    void writeLighting(const QByteArray& data, quint16 resource = 1, quint8 endpoint = 8);
    QByteArray v2w(quint8 endpoint, quint8 command, const QByteArray& arguments);
    const HidEndpoint& endpoint() const {
        return endpoint_;
    }

  private:
    void openResource(quint8 endpoint, quint8 handle, quint16 resource);
    void closeResource(quint8 endpoint, quint8 handle);
    void writeHandle(quint8 endpoint, quint8 handle, const QByteArray& data);
    HidEndpoint endpoint_;
    int fd_ = -1;
    int originalMode_ = -1;
    quint8 modeEndpoint_ = 8;
    ReportExchange transport_;
};
} // namespace open_icue
