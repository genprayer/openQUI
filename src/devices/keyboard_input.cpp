#include "keyboard_input.h"
#include "hid_transport.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cerrno>
#include <cmath>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace open_icue {
namespace {
constexpr int MAX_INPUT_REPORTS = 64;
constexpr int MAX_INPUT_REPORT_BYTES = 64;
constexpr auto HID_SYS_DIRECTORY = "/sys/class/hidraw";
constexpr auto HID_DEV_DIRECTORY = "/dev";
int ledSlot(int usage) {
    if (usage >= 0xe0 && usage <= 0xe7)
        return 105 + usage - 0xe0;
    return usage >= 4 && usage <= 101 ? usage : -1;
}
} // namespace
bool isK70KeyboardDescriptor(const QByteArray& descriptor) {
    return descriptor ==
           QByteArray::fromHex("05010906a1018508050719e029e7150025017501950881021900299715002501"
                               "7508950d81000508190129032501750195039102750595019101c005010906a101"
                               "8501050719e029e7150025017501950881021900299715002501750195988102"
                               "c0050c0901a1018502050c19002aff0f150026ff0f751095028100c0");
}
std::optional<QSet<int>> k70PressedLedSlots(const QByteArray& report) {
    if (report.isEmpty())
        return std::nullopt;
    const auto id = quint8(report[0]);
    if ((id != 1 && id != 8) || report.size() != (id == 1 ? 21 : 15))
        return std::nullopt;
    QSet<int> keys;
    const auto modifiers = quint8(report[1]);
    for (int bit = 0; bit < 8; ++bit)
        if (modifiers & (1 << bit))
            keys.insert(ledSlot(0xe0 + bit));
    if (id == 8) {
        for (int index = 2; index < report.size(); ++index) {
            const auto usage = quint8(report[index]);
            if (usage >= 1 && usage <= 3)
                return std::nullopt;
            if (const int slot = ledSlot(usage); slot >= 0)
                keys.insert(slot);
        }
    } else {
        for (int usage = 1; usage < 152; ++usage) {
            if (!(quint8(report[2 + usage / 8]) & (1 << (usage % 8))))
                continue;
            if (usage <= 3)
                return std::nullopt;
            if (const int slot = ledSlot(usage); slot >= 0)
                keys.insert(slot);
        }
    }
    return keys;
}
KeyboardLightingInput::KeyboardLightingInput(ReportReader reader) : reader_(std::move(reader)) {}
KeyboardLightingInput::~KeyboardLightingInput() {
    close();
}
void KeyboardLightingInput::close() {
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
    usbPath_.clear();
    held_.clear();
    presses_.clear();
    eventCount_ = 0;
}
void KeyboardLightingInput::openForUsb(const QString& usbPath) {
    if (fd_ >= 0 && usbPath_ == usbPath)
        return;
    close();
    const auto selected = QFileInfo(usbPath).canonicalFilePath();
    if (selected.isEmpty())
        throw std::runtime_error("K70 keyboard input is unavailable");
    for (const auto& name :
         QDir(HID_SYS_DIRECTORY)
             .entryList({"hidraw*"}, QDir::Dirs | QDir::System | QDir::NoDotAndDotDot)) {
        const auto path =
            QFileInfo(QString(HID_SYS_DIRECTORY) + '/' + name + "/device").canonicalFilePath();
        if (!path.startsWith(selected + '/'))
            continue;
        QFile descriptor(path + "/report_descriptor");
        if (!descriptor.open(QIODevice::ReadOnly) || !isK70KeyboardDescriptor(descriptor.readAll()))
            continue;
        const auto device = QFile::encodeName(QString(HID_DEV_DIRECTORY) + '/' + name);
        const int fd = ::open(device.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0)
            continue;
        hidraw_devinfo info{};
        int size = 0;
        hidraw_report_descriptor actual{};
        bool isValid = ::ioctl(fd, HIDIOCGRAWINFO, &info) == 0 && info.bustype == BUS_USB &&
                       quint16(info.vendor) == CORSAIR_VENDOR_ID &&
                       quint16(info.product) == K70_CORE_PRODUCT_ID &&
                       ::ioctl(fd, HIDIOCGRDESCSIZE, &size) == 0 && size == 125;
        if (isValid) {
            actual.size = quint32(size);
            isValid = ::ioctl(fd, HIDIOCGRDESC, &actual) == 0 &&
                      isK70KeyboardDescriptor(
                          QByteArray(reinterpret_cast<const char*>(actual.value), size));
        }
        if (!isValid) {
            ::close(fd);
            continue;
        }
        fd_ = fd;
        usbPath_ = usbPath;
        return;
    }
    throw std::runtime_error("K70 keyboard input is unavailable");
}
QList<QByteArray> KeyboardLightingInput::reports() {
    if (reader_)
        return reader_();
    QList<QByteArray> result;
    for (int index = 0; index < MAX_INPUT_REPORTS; ++index) {
        char bytes[MAX_INPUT_REPORT_BYTES];
        const auto count = ::read(fd_, bytes, sizeof(bytes));
        if (count > 0) {
            result.append(QByteArray(bytes, count));
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        if (count < 0 && errno == EINTR)
            continue;
        close();
        throw std::runtime_error("K70 keyboard input disconnected");
    }
    return result;
}
QMap<int, double> KeyboardLightingInput::poll(const QString& usbPath, double seconds) {
    if (!std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("Invalid lighting input timestamp");
    if (!reader_)
        openForUsb(usbPath);
    for (const auto& report : reports()) {
        const auto keys = k70PressedLedSlots(report);
        if (!keys)
            continue;
        for (const int key : *keys)
            if (!held_.contains(key)) {
                presses_[key] = seconds;
                ++eventCount_;
            }
        held_ = *keys;
    }
    for (auto it = presses_.begin(); it != presses_.end();) {
        if (seconds < it.value())
            it = presses_.erase(it);
        else
            ++it;
    }
    return presses_;
}
} // namespace open_icue
