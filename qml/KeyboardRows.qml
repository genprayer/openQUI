import QtQuick
import QtQuick.Layouts
import "."

ColumnLayout {
    id: root
    property var rows: []
    property string selectedKey: ""
    signal chosen(string key)
    spacing: 6
    Repeater {
        model: root.rows
        RowLayout {
            required property var modelData
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            spacing: 5
            Item {
                Layout.fillWidth: true
                visible: modelData.length <= 1
            }
            Repeater {
                model: modelData
                CueButton {
                    required property var modelData
                    objectName: "virtual-key-" + modelData.id
                    text: qsTranslate("OpenICUE", modelData.label)
                    Layout.preferredWidth: 44 * modelData.units
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    isAccent: root.selectedKey === modelData.id
                    onClicked: root.chosen(modelData.id)
                }
            }
            Item {
                Layout.fillWidth: true
                visible: modelData.length <= 1
            }
        }
    }
}
