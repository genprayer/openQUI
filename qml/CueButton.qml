import QtQuick
import QtQuick.Controls
import "."

Button {
    id: root
    property bool isAccent: false
    property bool isFlat: false
    property url imageSource: ""
    implicitHeight: 26
    implicitWidth: Math.max(26, contentItem.implicitWidth + 16)
    font.pixelSize: 12
    contentItem: Item {
        implicitWidth: caption.implicitWidth
        Text {
            id: caption
            anchors.fill: parent
            visible: !root.imageSource.toString().length
            text: root.text
            color: !root.enabled ? Theme.muted : root.isAccent ? "#141414" : Theme.text
            font: root.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Image {
            anchors.centerIn: parent
            width: 18
            height: 18
            visible: root.imageSource.toString().length > 0
            source: root.imageSource
            opacity: root.enabled ? 1 : 0.5
        }
    }
    background: Rectangle {
        radius: 4
        color: root.isAccent ? Theme.accent : root.down ? "#555555" : root.hovered ? "#474747" :
                                                                                     root.isFlat
                                                                                     ? "transparent" :
                                                                                       "#393939"
        opacity: root.enabled ? 1 : 0.6
    }
}
