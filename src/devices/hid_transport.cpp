#include "hid_transport.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace open_icue {
namespace {
constexpr int TRANSACTION_TIMEOUT_MS = 500;
constexpr int REPORT_SIZE = 64;
constexpr int MAX_PENDING_REPORTS = 32;
constexpr auto HID_SYS_DIRECTORY = "/sys/class/hidraw";
constexpr auto HID_DEV_DIRECTORY = "/dev";
QByteArray readFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
void requireSuccess(const QByteArray& response, quint8 endpoint, quint8 command) {
    if (response.size() != REPORT_SIZE || quint8(response[0]) != (endpoint & 7) ||
        quint8(response[1]) != command)
        throw std::runtime_error("Malformed HID response");
    if (response[2] != 0)
        throw std::runtime_error(QString("HID command 0x%1 rejected: status 0x%2")
                                     .arg(command, 2, 16, QLatin1Char('0'))
                                     .arg(quint8(response[2]), 2, 16, QLatin1Char('0'))
                                     .toStdString());
}
QByteArray handleArgument(quint8 handle, quint16 resource) {
    QByteArray result(3, 0);
    result[0] = char(handle);
    qToLittleEndian(resource, result.data() + 1);
    return result;
}
} // namespace

bool isCommandDescriptor(quint16 product, const QByteArray& descriptor) {
    if (product == VIRTUOSO_RECEIVER_PRODUCT_ID)
        return descriptor.contains(
            QByteArray::fromHex("0642ff0901a1018501150026ff007508953f090181028502953f09019102c0"));
    if (product != K70_CORE_PRODUCT_ID && product != DARKSTAR_PRODUCT_ID &&
        product != SLIPSTREAM_PRODUCT_ID)
        return false;
    return descriptor ==
               QByteArray::fromHex("0642ff0901a101150026ff007508954009018102954009019102c0") ||
           descriptor ==
               QByteArray::fromHex("0642ff0901a101150026ff009540750809018102954009019102c0");
}

QList<HidEndpoint> discoverCommandEndpoints() {
    QList<HidEndpoint> endpoints;
    for (const auto& name :
         QDir(HID_SYS_DIRECTORY)
             .entryList({"hidraw*"}, QDir::Dirs | QDir::System | QDir::NoDotAndDotDot)) {
        const auto devicePath =
            QFileInfo(QString(HID_SYS_DIRECTORY) + '/' + name + "/device").canonicalFilePath();
        quint16 vendor = 0, product = 0;
        for (const auto& line : readFile(devicePath + "/uevent").split('\n')) {
            if (!line.startsWith("HID_ID="))
                continue;
            const auto values = line.mid(7).split(':');
            if (values.size() == 3) {
                vendor = values[1].toUInt(nullptr, 16);
                product = values[2].toUInt(nullptr, 16);
            }
        }
        if (vendor != CORSAIR_VENDOR_ID)
            continue;
        const auto descriptor = readFile(devicePath + "/report_descriptor");
        if (!isCommandDescriptor(product, descriptor))
            continue;
        QDir parent(devicePath);
        QString serial;
        while (parent.cdUp() && parent.path() != "/") {
            if (QFile::exists(parent.path() + "/idVendor")) {
                serial = QString::fromUtf8(readFile(parent.path() + "/serial")).trimmed();
                break;
            }
        }
        endpoints.append({QString(HID_DEV_DIRECTORY) + '/' + name, parent.path(), serial, product,
                          descriptor, product == VIRTUOSO_RECEIVER_PRODUCT_ID});
    }
    return endpoints;
}

QByteArray bragiReport(quint8 endpoint, quint8 command, const QByteArray& arguments) {
    if (endpoint < 8 || endpoint > 15 || arguments.size() > 62)
        throw std::invalid_argument("Invalid Bragi packet arguments");
    QByteArray output(65, 0);
    output[1] = char(endpoint);
    output[2] = char(command);
    std::copy(arguments.begin(), arguments.end(), output.begin() + 3);
    return output;
}
quint32 bragiProperty(const QByteArray& response, quint8 endpoint) {
    requireSuccess(response, endpoint, 2);
    return qFromLittleEndian<quint32>(response.constData() + 3);
}

HidSession::HidSession(const HidEndpoint& endpoint) : endpoint_(endpoint) {
    fd_ = ::open(QFile::encodeName(endpoint.path).constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0)
        throw std::runtime_error(QString("Cannot open HID command interface: %1")
                                     .arg(QString::fromLocal8Bit(strerror(errno)))
                                     .toStdString());
    hidraw_devinfo identity{};
    hidraw_report_descriptor descriptor{};
    int size = 0;
    const bool isIdentityValid =
        ioctl(fd_, HIDIOCGRAWINFO, &identity) == 0 && identity.vendor == CORSAIR_VENDOR_ID &&
        identity.product == endpoint.product && identity.bustype == 3 &&
        ioctl(fd_, HIDIOCGRDESCSIZE, &size) == 0 && size > 0 && size <= HID_MAX_DESCRIPTOR_SIZE;
    if (isIdentityValid)
        descriptor.size = size;
    if (!isIdentityValid || ioctl(fd_, HIDIOCGRDESC, &descriptor) < 0 ||
        QByteArray(reinterpret_cast<const char*>(descriptor.value), size) != endpoint.descriptor ||
        !isCommandDescriptor(endpoint.product, endpoint.descriptor) ||
        flock(fd_, LOCK_EX | LOCK_NB) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("HID identity, descriptor or exclusive-lock check failed");
    }
}
HidSession::HidSession(const HidEndpoint& endpoint, ReportExchange transport)
    : endpoint_(endpoint), transport_(std::move(transport)) {
    if (!transport_)
        throw std::invalid_argument("A report transport is required");
}
HidSession::~HidSession() {
    try {
        restoreMode();
    } catch (const std::exception& error) {
        qWarning("HID mode restoration: %s", error.what());
    }
    if (fd_ >= 0)
        ::close(fd_);
}
QByteArray HidSession::exchange(const QByteArray& report,
                                const std::function<bool(const QByteArray&)>& matches) {
    if (transport_) {
        const auto response = transport_(report);
        if (!matches(response))
            throw std::runtime_error("Unexpected HID transport response");
        return response;
    }
    QByteArray response(REPORT_SIZE, 0);
    for (int i = 0; i < MAX_PENDING_REPORTS && ::read(fd_, response.data(), response.size()) > 0;
         ++i) {
    }
    const auto written = ::write(fd_, report.constData(), report.size());
    if (written != report.size())
        throw std::runtime_error("Incomplete HID write or disconnected device");
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < TRANSACTION_TIMEOUT_MS) {
        pollfd descriptor{fd_, POLLIN, 0};
        const auto ready = ::poll(&descriptor, 1, TRANSACTION_TIMEOUT_MS - int(clock.elapsed()));
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0 || descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
            throw std::runtime_error("HID command interface disconnected");
        if (!ready)
            break;
        const auto count = ::read(fd_, response.data(), response.size());
        if (count < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        if (count <= 0)
            throw std::runtime_error("HID response read failed");
        const auto packet = response.first(count);
        if (matches(packet))
            return packet;
    }
    throw std::runtime_error("HID response timeout");
}
QByteArray HidSession::bragi(quint8 endpoint, quint8 command, const QByteArray& arguments) {
    return exchange(bragiReport(endpoint, command, arguments), [=](const QByteArray& packet) {
        return packet.size() >= 3 && quint8(packet[0]) == (endpoint & 7) &&
               quint8(packet[1]) == command;
    });
}
quint32 HidSession::get(quint8 endpoint, quint8 property) {
    return bragiProperty(bragi(endpoint, 2, QByteArray(1, char(property))), endpoint);
}
void HidSession::set(quint8 endpoint, quint8 property, quint16 value) {
    QByteArray arguments(4, 0);
    arguments[0] = char(property);
    qToLittleEndian(value, arguments.data() + 2);
    requireSuccess(bragi(endpoint, 1, arguments), endpoint, 1);
}
void HidSession::enterSoftwareMode(quint8 endpoint) {
    if (originalMode_ >= 0)
        return;
    const auto mode = get(endpoint, 3);
    if (mode != 1 && mode != 2)
        throw std::runtime_error("Unknown firmware control mode");
    originalMode_ = int(mode);
    modeEndpoint_ = endpoint;
    if (mode == 1)
        set(endpoint, 3, 2);
}
void HidSession::restoreMode() {
    if (originalMode_ < 0)
        return;
    set(modeEndpoint_, 3, originalMode_);
    if (get(modeEndpoint_, 3) != quint32(originalMode_))
        throw std::runtime_error("Firmware mode restoration failed");
    originalMode_ = -1;
}
void HidSession::closeResource(quint8 endpoint, quint8 handle) {
    QByteArray arguments(2, 0);
    arguments[0] = 1;
    arguments[1] = char(handle);
    requireSuccess(bragi(endpoint, 5, arguments), endpoint, 5);
}
void HidSession::openResource(quint8 endpoint, quint8 handle, quint16 resource) {
    auto response = bragi(endpoint, 0x0d, handleArgument(handle, resource));
    if (response.size() == REPORT_SIZE && response[2] == 3) {
        closeResource(endpoint, handle);
        response = bragi(endpoint, 0x0d, handleArgument(handle, resource));
    }
    requireSuccess(response, endpoint, 0x0d);
}
QByteArray HidSession::readResource(quint16 resource, int expectedLength) {
    constexpr quint8 handle = 1;
    openResource(8, handle, resource);
    try {
        const auto probe = bragi(8, 9, QByteArray(1, char(handle)));
        requireSuccess(probe, 8, 9);
        if (expectedLength < 1 || expectedLength > 61 ||
            qFromLittleEndian<quint32>(probe.constData() + 5) != quint32(expectedLength))
            throw std::runtime_error("Unexpected pairing resource length");
        auto response = bragi(8, 8, QByteArray(1, char(handle)));
        requireSuccess(response, 8, 8);
        QByteArray data(response.constData() + 3, expectedLength);
        explicit_bzero(response.data(), response.size());
        closeResource(8, handle);
        return data;
    } catch (...) {
        try {
            closeResource(8, handle);
        } catch (const std::exception& error) {
            qWarning("HID resource cleanup: %s", error.what());
        }
        throw;
    }
}
void HidSession::writeHandle(quint8 endpoint, quint8 handle, const QByteArray& data) {
    if (data.isEmpty() || data.size() > 4096)
        throw std::invalid_argument("Invalid HID resource write size");
    QByteArray arguments(5, 0);
    arguments[0] = char(handle);
    qToLittleEndian(quint32(data.size()), arguments.data() + 1);
    arguments += data.first(std::min<qsizetype>(57, data.size()));
    requireSuccess(bragi(endpoint, 6, arguments), endpoint, 6);
    for (int offset = 57; offset < data.size(); offset += 61)
        requireSuccess(bragi(endpoint, 7, QByteArray(1, char(handle)) + data.mid(offset, 61)),
                       endpoint, 7);
}
void HidSession::writeResource(quint16 resource, const QByteArray& data, quint8 handle) {
    openResource(8, handle, resource);
    try {
        writeHandle(8, handle, data);
        closeResource(8, handle);
    } catch (...) {
        try {
            closeResource(8, handle);
        } catch (const std::exception& error) {
            qWarning("HID resource cleanup: %s", error.what());
        }
        throw;
    }
}
void HidSession::writeLighting(const QByteArray& data, quint16 resource, quint8 endpoint) {
    openResource(endpoint, 0, resource);
    try {
        writeHandle(endpoint, 0, data);
        closeResource(endpoint, 0);
    } catch (...) {
        try {
            closeResource(endpoint, 0);
        } catch (const std::exception& error) {
            qWarning("HID lighting cleanup: %s", error.what());
        }
        throw;
    }
}
QByteArray HidSession::v2w(quint8 endpoint, quint8 command, const QByteArray& arguments) {
    if (!endpoint_.isV2w || arguments.size() > 61 || (endpoint != 8 && endpoint != 9))
        throw std::invalid_argument("Invalid V2W request");
    QByteArray report(REPORT_SIZE, 0);
    report[0] = 2;
    report[1] = char(endpoint);
    report[2] = char(command);
    std::copy(arguments.begin(), arguments.end(), report.begin() + 3);
    return exchange(report, [=](const QByteArray& packet) {
        return packet.size() == REPORT_SIZE && packet[0] == 1 &&
               quint8(packet[1]) == (endpoint & 7) && quint8(packet[2]) == command;
    });
}
} // namespace open_icue
