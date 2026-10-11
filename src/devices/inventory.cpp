#include "inventory.h"
#include <QFile>
#include <libudev.h>
#include <memory>
#include <stdexcept>

namespace open_icue {
namespace {
QString attr(udev_device* device, const char* name) {
    if (!device)
        return {};
    const auto value = udev_device_get_sysattr_value(device, name);
    return value ? QString::fromUtf8(value) : QString();
}
QString readAttribute(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}
QVariantList children(udev* context, udev_device* parent, const char* subsystem) {
    std::unique_ptr<udev_enumerate, decltype(&udev_enumerate_unref)> scan(
        udev_enumerate_new(context), udev_enumerate_unref);
    if (!scan || udev_enumerate_add_match_parent(scan.get(), parent) < 0 ||
        udev_enumerate_add_match_subsystem(scan.get(), subsystem) < 0 ||
        udev_enumerate_scan_devices(scan.get()) < 0)
        throw std::runtime_error("USB child enumeration failed");
    QVariantList result;
    udev_list_entry* entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(scan.get())) {
        std::unique_ptr<udev_device, decltype(&udev_device_unref)> child(
            udev_device_new_from_syspath(context, udev_list_entry_get_name(entry)),
            udev_device_unref);
        if (!child)
            continue;
        const QString name = QString::fromUtf8(udev_device_get_sysname(child.get()));
        const auto driver = udev_device_get_driver(child.get());
        const auto node = udev_device_get_devnode(child.get());
        if (QByteArray(subsystem) == "input") {
            if (!name.startsWith("event") || !node)
                continue;
            const auto mouse = udev_device_get_property_value(child.get(), "ID_INPUT_MOUSE");
            const auto keyboard = udev_device_get_property_value(child.get(), "ID_INPUT_KEYBOARD");
            result.append(QVariantMap{{"path", QString::fromUtf8(node)},
                                      {"name", attr(udev_device_get_parent(child.get()), "name")},
                                      {"isMouse", mouse && QByteArray(mouse) == "1"},
                                      {"isKeyboard", keyboard && QByteArray(keyboard) == "1"}});
            continue;
        }
        if (QByteArray(subsystem) == "usb") {
            const auto type = udev_device_get_devtype(child.get());
            if (!type || QByteArray(type) != "usb_interface")
                continue;
        }
        result.append(
            QVariantMap{{"id", name}, {"driver", driver ? QString::fromUtf8(driver) : QString()}});
    }
    return result;
}
} // namespace

QVariantList discoverUsb(const Catalog& catalog) {
    std::unique_ptr<udev, decltype(&udev_unref)> context(udev_new(), udev_unref);
    if (!context)
        throw std::runtime_error("libudev context cannot be created");
    std::unique_ptr<udev_enumerate, decltype(&udev_enumerate_unref)> scan(
        udev_enumerate_new(context.get()), udev_enumerate_unref);
    if (!scan || udev_enumerate_add_match_subsystem(scan.get(), "usb") < 0 ||
        udev_enumerate_scan_devices(scan.get()) < 0)
        throw std::runtime_error("USB enumeration failed");
    QVariantList result;
    udev_list_entry* entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(scan.get())) {
        std::unique_ptr<udev_device, decltype(&udev_device_unref)> device(
            udev_device_new_from_syspath(context.get(), udev_list_entry_get_name(entry)),
            udev_device_unref);
        if (!device)
            continue;
        bool isVendorValid = false, isProductValid = false;
        const auto vendor = attr(device.get(), "idVendor").toUShort(&isVendorValid, 16);
        const auto product = attr(device.get(), "idProduct").toUShort(&isProductValid, 16);
        if (!isVendorValid || !isProductValid)
            continue;
        const auto model = catalog.modelForUsb(vendor, product);
        if (!model)
            continue;
        const QString path = QString::fromUtf8(udev_device_get_syspath(device.get()));
        result.append(QVariantMap{{"modelId", *model},
                                  {"vendor", vendor},
                                  {"product", product},
                                  {"sysPath", path},
                                  {"serial", attr(device.get(), "serial")},
                                  {"productName", attr(device.get(), "product")},
                                  {"isReceiver", model->contains("dongle")},
                                  {"usbInterfaces", children(context.get(), device.get(), "usb")},
                                  {"hidInterfaces", children(context.get(), device.get(), "hid")},
                                  {"inputNodes", children(context.get(), device.get(), "input")},
                                  {"powerControl", readAttribute(path + "/power/control")},
                                  {"runtimeStatus", readAttribute(path + "/power/runtime_status")},
                                  {"isUsbPresent", true}});
    }
    return result;
}
} // namespace open_icue
