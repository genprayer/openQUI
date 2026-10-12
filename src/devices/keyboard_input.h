#pragma once
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <functional>
#include <optional>

namespace open_icue {
/** One physical press retained for overlapping ripple animation. */
struct LightingKeyPress {
    int slot;
    double seconds;
};
/** Parse the descriptor-verified K70 array/NKRO input reports into Bragi LED slots. */
[[nodiscard]] std::optional<QSet<int>> k70PressedLedSlots(const QByteArray& report);
/** Exact observed input descriptor; unknown revisions must not be decoded by guessing. */
[[nodiscard]] bool isK70KeyboardDescriptor(const QByteArray& descriptor);

/** Passive, bounded keyboard input for reactive lighting; never grabs or writes input. */
class KeyboardLightingInput {
  public:
    using ReportReader = std::function<QList<QByteArray>()>;
    explicit KeyboardLightingInput(ReportReader reader = {});
    ~KeyboardLightingInput();
    KeyboardLightingInput(const KeyboardLightingInput&) = delete;
    KeyboardLightingInput& operator=(const KeyboardLightingInput&) = delete;
    /** Read only the keyboard interface belonging to the selected command USB device. */
    [[nodiscard]] QMap<int, double> poll(const QString& usbPath, double seconds);
    /** Bounded history; read only on the serialized lighting worker after poll(). */
    [[nodiscard]] const QList<LightingKeyPress>& recentPresses() const {
        return recentPresses_;
    }
    void close();
    [[nodiscard]] quint64 eventCount() const {
        return eventCount_;
    }

  private:
    void openForUsb(const QString& usbPath);
    [[nodiscard]] QList<QByteArray> reports();
    ReportReader reader_;
    QString usbPath_;
    int fd_ = -1;
    QSet<int> held_;
    QMap<int, double> presses_;
    QList<LightingKeyPress> recentPresses_;
    quint64 eventCount_ = 0;
};
} // namespace open_icue
