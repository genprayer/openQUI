#include "alsa_mixer.h"
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <alsa/asoundlib.h>
#include <memory>
#include <stdexcept>

namespace open_icue {
namespace {
using Mixer = std::unique_ptr<snd_mixer_t, decltype(&snd_mixer_close)>;
void check(int result) {
    if (result < 0)
        throw std::runtime_error(snd_strerror(result));
}
Mixer mixerForUsb(const QString& usbSysPath) {
    const auto usb = QFileInfo(usbSysPath).canonicalFilePath();
    if (usb.isEmpty() || !usb.startsWith("/sys/devices/"))
        throw std::invalid_argument("USB sysfs path is unavailable");
    for (const auto& name :
         QDir("/sys/class/sound").entryList({"card[0-9]*"}, QDir::Dirs | QDir::System)) {
        const auto card = QFileInfo("/sys/class/sound/" + name).canonicalFilePath();
        if (!card.startsWith(usb + "/"))
            continue;
        const QFileInfo control("/dev/snd/controlC" + name.mid(4));
        if (!control.isReadable() || !control.isWritable())
            throw std::runtime_error("The selected USB headset has no accessible ALSA mixer card");
        snd_mixer_t* raw = nullptr;
        check(snd_mixer_open(&raw, 0));
        Mixer mixer(raw, snd_mixer_close);
        const auto id = ("hw:" + name.mid(4)).toUtf8();
        check(snd_mixer_attach(mixer.get(), id.constData()));
        check(snd_mixer_selem_register(mixer.get(), nullptr, nullptr));
        check(snd_mixer_load(mixer.get()));
        return mixer;
    }
    throw std::runtime_error("The selected USB headset has no ALSA mixer card");
}
QVariantMap readElement(snd_mixer_elem_t* element, bool isCapture) {
    long minimum = 0, maximum = 0, value = 0;
    int isOn = 1;
    if (isCapture) {
        check(snd_mixer_selem_get_capture_volume_range(element, &minimum, &maximum));
        check(snd_mixer_selem_get_capture_volume(element, SND_MIXER_SCHN_FRONT_LEFT, &value));
        if (snd_mixer_selem_has_capture_switch(element))
            check(snd_mixer_selem_get_capture_switch(element, SND_MIXER_SCHN_FRONT_LEFT, &isOn));
    } else {
        check(snd_mixer_selem_get_playback_volume_range(element, &minimum, &maximum));
        check(snd_mixer_selem_get_playback_volume(element, SND_MIXER_SCHN_FRONT_LEFT, &value));
        if (snd_mixer_selem_has_playback_switch(element))
            check(snd_mixer_selem_get_playback_switch(element, SND_MIXER_SCHN_FRONT_LEFT, &isOn));
    }
    if (maximum <= minimum)
        throw std::runtime_error("Mixer has no adjustable volume range");
    return {{"percent", qRound(100.0 * (value - minimum) / (maximum - minimum))},
            {"isMuted", isOn == 0},
            {"isAvailable", true}};
}
} // namespace

QVariantMap readHeadsetMixer(const QString& usbSysPath) {
    auto mixer = mixerForUsb(usbSysPath);
    QVariantMap result;
    for (auto* element = snd_mixer_first_elem(mixer.get()); element;
         element = snd_mixer_elem_next(element)) {
        if (!snd_mixer_selem_is_active(element))
            continue;
        const QString name = QString::fromUtf8(snd_mixer_selem_get_name(element));
        if (name == "Sidetone" && snd_mixer_selem_has_playback_volume(element))
            result["sidetone"] = readElement(element, false);
        if (snd_mixer_selem_has_capture_volume(element) && !result.contains("microphone")) {
            auto value = readElement(element, true);
            value["control"] = name;
            result["microphone"] = value;
        }
    }
    return result;
}

QVariantMap setHeadsetControl(const QString& usbSysPath, const QString& control, int percent,
                              bool isMuted) {
    if (percent < 0 || percent > 100 || (control != "sidetone" && control != "microphone"))
        throw std::invalid_argument("Invalid mixer control");
    auto mixer = mixerForUsb(usbSysPath);
    for (auto* element = snd_mixer_first_elem(mixer.get()); element;
         element = snd_mixer_elem_next(element)) {
        if (!snd_mixer_selem_is_active(element))
            continue;
        const bool isCapture = control == "microphone";
        const QString name = QString::fromUtf8(snd_mixer_selem_get_name(element));
        if ((isCapture && !snd_mixer_selem_has_capture_volume(element)) ||
            (!isCapture && name != "Sidetone"))
            continue;
        long minimum = 0, maximum = 0;
        check(isCapture ? snd_mixer_selem_get_capture_volume_range(element, &minimum, &maximum)
                        : snd_mixer_selem_get_playback_volume_range(element, &minimum, &maximum));
        const auto value = minimum + qRound64((maximum - minimum) * percent / 100.0);
        if (isCapture) {
            check(snd_mixer_selem_set_capture_volume_all(element, value));
            if (snd_mixer_selem_has_capture_switch(element))
                check(snd_mixer_selem_set_capture_switch_all(element, !isMuted));
        } else {
            check(snd_mixer_selem_set_playback_volume_all(element, value));
            if (snd_mixer_selem_has_playback_switch(element))
                check(snd_mixer_selem_set_playback_switch_all(element, !isMuted));
        }
        return readElement(element, isCapture);
    }
    throw std::runtime_error("The requested mixer control is unavailable");
}
} // namespace open_icue
