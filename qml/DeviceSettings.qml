import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Popup {
    id: root
    objectName: "settingsPopup"
    width: Math.min(1060, parent.width - 60)
    height: Math.min(768, parent.height - 60)
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    property string pageId: "general"
    function openGeneral() {
        pageId = "general";
        open();
    }
    function openDevice() {
        pageId = "device";
        open();
    }
    background: Rectangle {
        color: "#303030"
        radius: 4
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16
        RowLayout {
            Text {
                text: qsTranslate("OpenICUE", "Einstellungen")
                color: Theme.text
                font.pixelSize: 20
                Layout.fillWidth: true
            }
            Text {
                objectName: "settings-current-device"
                visible: root.pageId !== "general"
                text: icue.device.name
                color: Theme.text
                font.pixelSize: 14
            }
            CueButton {
                objectName: "settings-close"
                text: "×"
                onClicked: root.close()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18
            ColumnLayout {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                spacing: 8
                CueButton {
                    objectName: "settings-general"
                    text: qsTranslate("OpenICUE", "Allgemeine Einstellungen")
                    Layout.fillWidth: true
                    isAccent: root.pageId === "general"
                    onClicked: root.pageId = "general"
                }
                CueButton {
                    objectName: "settings-device"
                    text: qsTranslate("OpenICUE", "Geräteeinstellungen")
                    Layout.fillWidth: true
                    isAccent: root.pageId === "device"
                    onClicked: root.pageId = "device"
                }
                CueButton {
                    objectName: "settings-connections"
                    text: qsTranslate("OpenICUE", "Verbindungen")
                    Layout.fillWidth: true
                    isAccent: root.pageId === "connections"
                    onClicked: root.pageId = "connections"
                }
                Item {
                    Layout.fillHeight: true
                }
                Text {
                    text: "OpenQUI " + Qt.application.version
                    color: Theme.muted
                    font.pixelSize: 11
                }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.pageId === "general" ? 0 : root.pageId === "device" ? 1 : 2
                ColumnLayout {
                    spacing: 16
                    Text {
                        objectName: "language-label"
                        text: qsTranslate("OpenICUE", "Sprache")
                        color: Theme.text
                        font.pixelSize: 16
                    }
                    ComboBox {
                        objectName: "language-selector"
                        Layout.preferredWidth: 240
                        model: i18n.languages
                        textRole: "name"
                        valueRole: "code"
                        currentIndex: i18n.languages.findIndex(language => language.code
                                                                           === i18n.languageCode)
                        onActivated: i18n.setLanguage(currentValue)
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "Der Sprachwechsel wird sofort übernommen und für den nächsten Start gespeichert.")
                        color: Theme.muted
                        Layout.fillWidth: true
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "Gerätenamen und eigene Profilnamen bleiben unverändert.")
                        color: Theme.muted
                        Layout.fillWidth: true
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        visible: i18n.error.length > 0
                        text: i18n.error
                        color: "#ff9797"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                }
                ColumnLayout {
                    spacing: 14
                    Text {
                        text: icue.device.name
                        color: Theme.text
                        font.pixelSize: 16
                    }
                    Text {
                        text: qsTranslate("OpenICUE", "Firmware")
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    Text {
                        text: qsTranslate("OpenICUE", "Nicht aus dem Gerät ausgelesen")
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                    Text {
                        text: qsTranslate("OpenICUE", "USB-Verbindung")
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    Text {
                        text: icue.device.isConnected ? qsTranslate("OpenICUE", "Erkannt") :
                                                        qsTranslate("OpenICUE", "Nicht verbunden")
                        color: icue.device.isConnected ? Theme.green : Theme.muted
                        font.pixelSize: 12
                    }
                    Text {
                        visible: icue.device.maxPollingRate > 0
                        text: qsTranslate("OpenICUE", "Polling-Rate: bis %1 Hz").arg(
                                  icue.device.maxPollingRate)
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    Text {
                        text: qsTranslate("OpenICUE",
                                          "Die Funkverbindung und Firmware werden damit nicht bestätigt.")
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                    CueButton {
                        text: qsTranslate("OpenICUE", "Gerätestatus aktualisieren")
                        onClicked: icue.refresh()
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                }
                ConnectionPanels {}
            }
        }
    }
}
