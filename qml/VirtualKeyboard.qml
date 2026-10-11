import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Popup {
    id: root
    width: 1180
    height: 455
    modal: true
    anchors.centerIn: Overlay.overlay
    property string selectedKey: ""
    signal chosen(string key)
    background: Rectangle {
        color: "#2b2b2b"
        radius: 4
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8
        Text {
            text: qsTranslate("OpenICUE", "Virtuelle Tastatur · %1").arg(qsTranslate("OpenICUE",
                                                                                     icue.assignmentKeyboard.label))
            color: Theme.text
            font.pixelSize: 16
        }
        KeyboardRows {
            Layout.fillWidth: true
            rows: icue.assignmentKeyboard.extendedRows
            selectedKey: root.selectedKey
            onChosen: key => root.selectedKey = key
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            KeyboardRows {
                Layout.fillWidth: true
                Layout.preferredWidth: 790
                rows: icue.assignmentKeyboard.mainRows
                selectedKey: root.selectedKey
                onChosen: key => root.selectedKey = key
            }
            KeyboardRows {
                Layout.preferredWidth: 150
                Layout.alignment: Qt.AlignTop
                rows: icue.assignmentKeyboard.navigationRows
                selectedKey: root.selectedKey
                onChosen: key => root.selectedKey = key
            }
            KeyboardRows {
                Layout.preferredWidth: 180
                Layout.alignment: Qt.AlignTop
                rows: icue.assignmentKeyboard.numpadRows
                selectedKey: root.selectedKey
                onChosen: key => root.selectedKey = key
            }
        }
        Text {
            text: root.selectedKey.length ? qsTranslate("OpenICUE", "Ausgewählt: %1").arg(
                                                root.selectedKey) : qsTranslate("OpenICUE",
                                                                                "Eine Taste auswählen")
            color: Theme.muted
            font.pixelSize: 11
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            CueButton {
                text: qsTranslate("OpenICUE", "Abbrechen")
                onClicked: root.close()
            }
            CueButton {
                objectName: "virtual-key-confirm"
                text: qsTranslate("OpenICUE", "OK")
                isAccent: true
                enabled: root.selectedKey.length > 0
                onClicked: {
                    root.chosen(root.selectedKey);
                    root.close();
                }
            }
        }
    }
}
