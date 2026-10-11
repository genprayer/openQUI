import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

RowLayout {
    id: root
    objectName: "connectionPanel"
    spacing: 8
    readonly property bool isDarkstarScope: icue.selectedModelId === "darkstar_wireless"
                                            || icue.selectedModelId
                                            === "slipstream_wireless_dongle_ss"
    readonly property bool isWirelessScope: root.isDarkstarScope || icue.selectedModelId
                                            === "virtuoso_black" || icue.selectedModelId
                                            === "virtuoso_black_dongle"
    readonly property var selectedHardware: (icue.hardwareState.devices || []).find(device => {
        if (!device.isVerified)
            return false;
        if (icue.selectedModelId === "virtuoso_black_dongle")
            return device.modelId === "virtuoso_black" && icue.usbInventory.some(usb => usb.product
                                                                                        === device.product);
        return device.modelId === icue.selectedModelId;
    }) || ({})
    CuePanel {
        Layout.preferredWidth: 360
        Layout.fillWidth: true
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "USB-Geräte und Treiber")
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Text {
                visible: root.isWirelessScope
                Layout.fillWidth: true
                text: qsTranslate("OpenICUE", "USB-Erkennung bestätigt keine Funkverbindung.")
                color: Theme.muted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            ListView {
                objectName: "usb-inventory"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: icue.usbInventory
                spacing: 8
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: details.implicitHeight + 20
                    radius: 4
                    color: Theme.raised
                    Column {
                        id: details
                        x: 10
                        y: 10
                        width: parent.width - 20
                        spacing: 6
                        Text {
                            width: parent.width
                            text: modelData.productName
                            color: Theme.text
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: "USB " + modelData.vendor.toString(16).padStart(4, "0") + ":"
                                  + modelData.product.toString(16).padStart(4, "0")
                            color: Theme.muted
                            font.pixelSize: 11
                        }
                        Text {
                            width: parent.width
                            text: qsTranslate("OpenICUE", "Treiber: %1").arg([...new Set(
                                                                                  modelData.usbInterfaces.concat(
                                                                                      modelData.hidInterfaces).map(
                                                                                      item => item.driver).filter(
                                                                                      name => name.length))].join(
                                                                                 ", ") || qsTranslate(
                                                                                 "OpenICUE",
                                                                                 "Unbekannt"))
                            color: Theme.text
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            width: parent.width
                            text: qsTranslate("OpenICUE", "Eingabeschnittstellen: %1").arg(
                                      modelData.inputNodes.length)
                            color: Theme.text
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            width: parent.width
                            text: qsTranslate("OpenICUE", "Energieverwaltung: %1 · %2").arg(
                                      modelData.powerControl || "—").arg(modelData.runtimeStatus
                                                                         || "—")
                            color: Theme.muted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
            Text {
                visible: icue.usbInventory.length === 0
                text: qsTranslate("OpenICUE", "Kein USB-Gerät für %1 erkannt").arg(icue.device.name)
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            CueButton {
                objectName: "connections-refresh"
                text: qsTranslate("OpenICUE", "Verbindungen aktualisieren")
                Layout.fillWidth: true
                onClicked: icue.refresh()
            }
        }
    }
    CuePanel {
        Layout.fillWidth: true
        Layout.preferredWidth: 480
        Layout.fillHeight: true
        title: root.isDarkstarScope ? "SLIPSTREAM" : icue.device.name
        Flickable {
            anchors.fill: parent
            contentHeight: content.implicitHeight
            clip: true
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: content
                width: parent.width
                spacing: 12
                ColumnLayout {
                    id: instructions
                    visible: root.isDarkstarScope
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        objectName: "pairing-status"
                        text: {
                            const receiver = (icue.hardwareState.devices || []).find(d => d.product === 0x1bdc
                                                                                          && d.isVerified);
                            if (!receiver)
                                return qsTranslate("OpenICUE", "Funkstatus nicht ausgelesen");
                            return receiver.isRadioConnected ? qsTranslate("OpenICUE",
                                                                           "Darkstar über 2,4 GHz verbunden") :
                                                               qsTranslate("OpenICUE",
                                                                           "Empfänger antwortet; keine Darkstar-Funkverbindung");
                        }
                        color: Theme.text
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE", "Empfänger: %1").arg(
                                  "SLIPSTREAM WIRELESS USB Receiver")
                        color: Theme.muted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "Sobald gekoppelt, nutzt die Maus die vorhandenen Linux-HID-Treiber. Für das Koppeln sind herstellerspezifische Befehle erforderlich.")
                        color: Theme.text
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    CueButton {
                        objectName: "pairing-start"
                        text: qsTranslate("OpenICUE", "Geräte koppeln")
                        enabled: root.isDarkstarScope && icue.isPairingImplemented &&
                                 !icue.isHardwareBusy
                        Layout.fillWidth: true
                        onClicked: icue.pairDarkstar()
                    }
                    Text {
                        visible: Boolean(icue.hardwareState.recoveryError)
                        text: qsTranslate("OpenICUE", icue.hardwareState.recoveryError || "")
                        color: Theme.accent
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "Maus per Kabel und Empfänger gleichzeitig anschließen. Nach bestätigter Übertragung das Kabel entfernen und 2,4 GHz testen.")
                        color: Theme.muted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE", "Kopplung und Hardwarestatus")
                        color: Theme.text
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "1. Maus auf 2,4 GHz stellen. Maus per USB-Kabel und den gewünschten Empfänger direkt an den PC anschließen.")
                        color: Theme.text
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "2. Geräte koppeln drücken. Die App prüft Antworten und liest den Kopplungsbezeichner zurück.")
                        color: Theme.text
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "3. Erst nach Abschluss das Kabel entfernen und 2,4 GHz testen. Eine weiße Status-LED an Darkstar zeigt die Verbindung an.")
                        color: Theme.text
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    CueButton {
                        text: qsTranslate("OpenICUE", "Corsair-Anleitung öffnen")
                        Layout.fillWidth: true
                        onClicked: Qt.openUrlExternally(SupportLinks.mousePairing)
                    }
                    CueButton {
                        text: qsTranslate("OpenICUE", "Hardwarestatus aktualisieren")
                        Layout.fillWidth: true
                        enabled: !icue.isHardwareBusy
                        onClicked: icue.probeHardware()
                    }
                    CueButton {
                        text: qsTranslate("OpenICUE", "Darkstar-Handbuch öffnen")
                        Layout.fillWidth: true
                        onClicked: Qt.openUrlExternally(SupportLinks.darkstarManual)
                    }
                }
                ColumnLayout {
                    visible: !root.isDarkstarScope
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        objectName: "selected-connection-status"
                        text: root.selectedHardware.isVerified ? qsTranslate("OpenICUE",
                                                                             "Gerät antwortet") :
                                                                 qsTranslate("OpenICUE",
                                                                             "Nicht aus dem Gerät ausgelesen")
                        color: root.selectedHardware.isVerified ? Theme.green : Theme.muted
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        objectName: "selected-connection-battery"
                        visible: Boolean(root.selectedHardware.hasBattery)
                        text: qsTranslate("OpenICUE", "Akkustand: %1 %").arg(
                                  root.selectedHardware.batteryPercent)
                        color: Theme.text
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    CueButton {
                        objectName: "selected-connection-refresh"
                        text: qsTranslate("OpenICUE", "Hardwarestatus aktualisieren")
                        Layout.fillWidth: true
                        enabled: !icue.isHardwareBusy
                        onClicked: icue.probeHardware()
                    }
                }
            }
        }
    }
}
