import QtQuick
import QtQuick.Controls
import "."

Switch {
    id: root
    implicitHeight: 24
    implicitWidth: 32
    padding: 0
    indicator: Rectangle {
        width: 24
        height: 9
        radius: 5
        y: (root.height - height) / 2
        color: root.checked ? Theme.green : "#292929"
        Rectangle {
            x: root.checked ? 12 : 0
            y: -2
            width: 13
            height: 13
            radius: 7
            color: root.checked ? "#dddddd" : "#454545"
        }
    }
}
