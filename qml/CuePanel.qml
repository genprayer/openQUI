import QtQuick
import QtQuick.Controls
import "."

Rectangle {
    id: root
    property string title: ""
    property int titleRightMargin: hasAdd ? 48 : 16
    property bool hasAdd: false
    signal addClicked
    default property alias content: container.data
    color: Theme.panel
    radius: 4
    implicitWidth: 256
    implicitHeight: 312
    Text {
        x: 16
        y: 11
        width: root.width - x - root.titleRightMargin
        text: root.title
        elide: Text.ElideRight
        color: Theme.text
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }
    CueButton {
        x: root.width - 40
        y: 4
        width: 32
        height: 32
        text: "+"
        visible: root.hasAdd
        onClicked: root.addClicked()
    }
    Item {
        id: container
        x: 16
        y: 44
        width: parent.width - 32
        height: parent.height - 60
    }
}
