import QtQuick
import QtQuick.Controls
import "."

Slider {
    id: root
    implicitHeight: orientation === Qt.Horizontal ? 24 : 180
    implicitWidth: orientation === Qt.Horizontal ? 160 : 24
    padding: 8
    snapMode: Slider.SnapAlways
    background: Rectangle {
        x: root.orientation === Qt.Horizontal ? root.leftPadding : (root.width - width) / 2
        y: root.orientation === Qt.Horizontal ? (root.height - height) / 2 : root.topPadding
        width: root.orientation === Qt.Horizontal ? root.availableWidth : 4
        height: root.orientation === Qt.Horizontal ? 4 : root.availableHeight
        color: "#424242"
        radius: 2
    }
    handle: Rectangle {
        x: root.orientation === Qt.Horizontal ? root.leftPadding + root.visualPosition * (
                                                    root.availableWidth - width) : (root.width
                                                                                    - width) / 2
        y: root.orientation === Qt.Horizontal ? (root.height - height) / 2 : root.topPadding + root.visualPosition
                                                * (root.availableHeight - height)
        width: 16
        height: 16
        radius: 8
        color: root.enabled ? "#ededed" : "#686868"
    }
}
