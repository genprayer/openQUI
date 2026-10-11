import QtQuick
import QtQuick.Controls
import "."

TextField {
    id: root
    implicitHeight: 26
    font.pixelSize: 12
    color: Theme.text
    selectionColor: Theme.selected
    background: Rectangle {
        color: "#292929"
        radius: 3
        border.color: root.activeFocus ? "#808080" : "#484848"
    }
}
