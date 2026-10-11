import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

RowLayout {
    spacing: 8
    CuePanel {
        Layout.preferredWidth: 376
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Gestenkonfiguration")
        ColumnLayout {
            anchors.fill: parent
            spacing: 14
            RowLayout {
                Text {
                    text: qsTranslate("OpenICUE", "Mehrere Neigungsgesten zulassen")
                    color: Theme.text
                    font.pixelSize: 12
                }
                CueToggle {
                    objectName: "tilt-multiple"
                    checked: icue.document.isMultipleTilt || false
                    onToggled: icue.setDocumentValue("isMultipleTilt", checked)
                }
            }
            Text {
                text: qsTranslate("OpenICUE",
                                  "Eine weitere Neigungsgeste erfordert sonst eine\nRückkehr in die neutrale Position.")
                color: Theme.muted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Item {
                Layout.fillHeight: true
            }
            CueButton {
                text: qsTranslate("OpenICUE", "Gestenneigung kalibrieren")
                enabled: false
            }
        }
    }
    CuePanel {
        Layout.preferredWidth: 688
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Neigungs-Setup")
        color: Theme.raised
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            y: 20
            spacing: 16
            Repeater {
                model: [qsTranslate("OpenICUE", "Links neigen"), qsTranslate("OpenICUE",
                                                                             "Rechts neigen"),
                    qsTranslate("OpenICUE", "Vorne neigen"), qsTranslate("OpenICUE",
                                                                         "Zurück neigen")]
                delegate: Item {
                    required property string modelData
                    required property int index
                    Layout.preferredWidth: 148
                    Layout.preferredHeight: 260
                    Text {
                        width: parent.width
                        text: modelData + " (°)"
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    Row {
                        y: 30
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 4
                        CueButton {
                            text: "−"
                            enabled: icue.document.tiltAngles[index] > 10
                            onClicked: icue.setTiltAngle(index, icue.document.tiltAngles[index]
                                                         - 10)
                        }
                        CueField {
                            width: 54
                            text: icue.document.tiltAngles[index]
                            horizontalAlignment: Text.AlignHCenter
                            validator: IntValidator {
                                bottom: 10
                                top: 80
                            }
                            onEditingFinished: icue.setTiltAngle(index, Math.round(Number(text)
                                                                                   / 10) * 10)
                        }
                        CueButton {
                            objectName: "tilt-plus-" + index
                            text: "+"
                            enabled: icue.document.tiltAngles[index] < 80
                            onClicked: icue.setTiltAngle(index, icue.document.tiltAngles[index]
                                                         + 10)
                        }
                    }
                    Image {
                        id: mouseTilt
                        y: 146
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: index < 2 ? 74 : 113
                        height: 40
                        source: index < 2 ? icue.device.tiltHorizontal : icue.device.tiltVertical
                        fillMode: Image.PreserveAspectCrop
                        transform: Rotation {
                            origin.x: index === 0 || index === 2 ? 0 : mouseTilt.width
                            origin.y: mouseTilt.height
                            axis.z: index === 0 || index === 2 ? -1 : 1
                            angle: icue.document.tiltAngles[index]
                        }
                    }
                    Rectangle {
                        y: 192
                        width: parent.width
                        height: 1
                        color: "#484848"
                    }
                }
            }
        }
    }
    Item {
        Layout.fillWidth: true
    }
}
