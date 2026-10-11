/** Check observed keyboard reports without reading the test runner's actual keyboard. */
#include "devices/keyboard_input.h"
#include <QtTest>
#include <limits>
#include <stdexcept>

namespace {
QByteArray arrayReport(quint8 usage = 0, quint8 modifiers = 0) {
    QByteArray report(15, 0);
    report[0] = 8;
    report[1] = char(modifiers);
    report[2] = char(usage);
    return report;
}
} // namespace

class NativeKeyboardInputTests final : public QObject {
    Q_OBJECT
  private slots:
    void arrayReportMapsOnlyPhysicalKeysAndModifiers() {
        auto report = arrayReport(4, 0x42);
        report[3] = 0x69; // F14 is not a physical K70 key or its left-shift LED slot.
        const auto keys = open_icue::k70PressedLedSlots(report);
        QVERIFY(keys);
        QCOMPARE(*keys, (QSet<int>{4, 106, 111}));
        QCOMPARE(*open_icue::k70PressedLedSlots(arrayReport()), QSet<int>{});
    }
    void nkroReportMapsBitsAndModifiers() {
        QByteArray report(21, 0);
        report[0] = 1;
        report[1] = char(0x81);
        report[2] = char(1 << 5);
        report[2 + 58 / 8] = char(1 << (58 % 8));
        report[2 + 0x69 / 8] = char(1 << (0x69 % 8));
        const auto keys = open_icue::k70PressedLedSlots(report);
        QVERIFY(keys);
        QCOMPARE(*keys, (QSet<int>{5, 58, 105, 112}));
    }
    void invalidRolloverAndMediaReportsDoNotReleaseHeldKeys() {
        QVERIFY(!open_icue::k70PressedLedSlots({}));
        QVERIFY(!open_icue::k70PressedLedSlots(arrayReport().first(14)));
        QVERIFY(!open_icue::k70PressedLedSlots(arrayReport(1)));
        QVERIFY(!open_icue::k70PressedLedSlots(QByteArray::fromHex("0201000000")));
        QByteArray nkro(21, 0);
        nkro[0] = 1;
        nkro[2] = 4;
        QVERIFY(!open_icue::k70PressedLedSlots(nkro));
    }
    void onlyExactObservedDescriptorIsAccepted() {
        auto descriptor =
            QByteArray::fromHex("05010906a1018508050719e029e7150025017501950881021900299715002501"
                                "7508950d81000508190129032501750195039102750595019101c005010906a101"
                                "8501050719e029e7150025017501950881021900299715002501750195988102"
                                "c0050c0901a1018502050c19002aff0f150026ff0f751095028100c0");
        QCOMPARE(descriptor.size(), 125);
        QVERIFY(open_icue::isK70KeyboardDescriptor(descriptor));
        descriptor[7] = 9;
        QVERIFY(!open_icue::isK70KeyboardDescriptor(descriptor));
        QVERIFY(!open_icue::isK70KeyboardDescriptor({}));
    }
    void heldReportsDoNotRetriggerAndReleasedKeysDo() {
        QList<QByteArray> queued;
        open_icue::KeyboardLightingInput input([&] {
            const auto reports = queued;
            queued.clear();
            return reports;
        });
        queued = {arrayReport(4)};
        QCOMPARE(input.poll("selected-usb", 2).value(4), 2.0);
        QCOMPARE(input.eventCount(), quint64(1));
        queued = {arrayReport(4), QByteArray::fromHex("0201000000"), arrayReport(1)};
        QCOMPARE(input.poll("selected-usb", 3).value(4), 2.0);
        QCOMPARE(input.eventCount(), quint64(1));
        queued = {arrayReport(), arrayReport(4)};
        QCOMPARE(input.poll("selected-usb", 4).value(4), 4.0);
        QCOMPARE(input.eventCount(), quint64(2));
        QCOMPARE(input.poll("selected-usb", 120).value(4), 4.0);
        input.close();
        QCOMPARE(input.eventCount(), quint64(0));
        QVERIFY(input.poll("selected-usb", 0).isEmpty());
    }
    void invalidTimeDoesNotConsumeReports() {
        int reads = 0;
        open_icue::KeyboardLightingInput input([&] {
            ++reads;
            return QList<QByteArray>{arrayReport(4)};
        });
        QVERIFY_EXCEPTION_THROWN(static_cast<void>(input.poll({}, -1)), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(
            static_cast<void>(input.poll({}, std::numeric_limits<double>::infinity())),
            std::invalid_argument);
        QCOMPARE(reads, 0);
    }
};
QTEST_GUILESS_MAIN(NativeKeyboardInputTests)
#include "test_keyboard_input.moc"
