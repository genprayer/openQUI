import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

RowLayout {
    spacing: 8
    Dialog {
        id: addPreset
        title: qsTranslate("OpenICUE", "DPI-Voreinstellung")
        anchors.centerIn: parent
        modal: true
        footer: CueDialogButtons {}
        contentItem: CueField {
            id: name
            text: qsTranslate("OpenICUE", "Neue Voreinstellung")
            implicitWidth: 250
        }
        onAccepted: icue.addDpiPreset(name.text)
    }
    ColorEditor {
        id: colorEditor
        onChosen: color => {
            const colors = icue.document.indicatorColors.slice();
            colors[colorIndex] = color;
            icue.setDocumentValue("indicatorColors", colors);
        }
        property int colorIndex: 0
    }
    CuePanel {
        Layout.preferredWidth: 256
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "DPI-Voreinstellungen")
        Column {
            width: parent.width
            spacing: 4
            CueButton {
                width: parent.width
                text: "+"
                isAccent: true
                onClicked: addPreset.open()
            }
            Repeater {
                model: icue.document.dpiPresets || []
                Rectangle {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: 24
                    color: index === icue.document.activeDpiPreset ? Theme.selected : "transparent"
                    radius: 3
                    Text {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: icue.selectDpiPreset(index)
                    }
                    CueButton {
                        width: 24
                        height: 24
                        anchors.right: parent.right
                        text: "×"
                        isFlat: true
                        visible: icue.document.dpiPresets.length > 1
                        onClicked: icue.removeDpiPreset(index)
                    }
                }
            }
        }
    }
    CuePanel {
        Layout.preferredWidth: 520
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "DPI-Stufen")
        color: Theme.raised
        Flickable {
            objectName: "dpi-settings-scroll"
            anchors.fill: parent
            clip: true
            contentHeight: dpiControls.height
            flickableDirection: Flickable.VerticalFlick
            ScrollBar.vertical: ScrollBar {}
            Column {
                id: dpiControls
                width: parent.width
                spacing: 8
                Repeater {
                    model: icue.document.dpiStages || []
                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        width: parent.width
                        height: 28
                        spacing: 12
                        Text {
                            text: index === icue.document.activeDpi ? "⌂" : ""
                            color: Theme.text
                            Layout.preferredWidth: 12
                        }
                        CueButton {
                            objectName: "dpi-select-" + index
                            text: qsTranslate("OpenICUE", modelData.name)
                            isFlat: true
                            font.pixelSize: 11
                            Layout.preferredWidth: 68
                            onClicked: icue.selectDpiStage(index)
                        }
                        CueButton {
                            text: "⋮"
                            width: 16
                            isFlat: true
                            onClicked: stageMenu.popup()
                            Menu {
                                id: stageMenu
                                MenuItem {
                                    text: modelData.enabled ? qsTranslate("OpenICUE", "Deaktivieren") : qsTranslate("OpenICUE", "Aktivieren")
                                    onTriggered: icue.setDpiStage(index, "enabled", !modelData.enabled)
                                }
                                MenuItem {
                                    text: qsTranslate("OpenICUE", "X/Y unabhängig")
                                    checkable: true
                                    enabled: icue.device.hasIndependentDpiAxes || modelData.independentXY
                                    checked: modelData.independentXY
                                    onTriggered: icue.setDpiStage(index, "independentXY", checked)
                                }
                            }
                        }
                        CueSlider {
                            Layout.fillWidth: true
                            from: icue.device.minDpi
                            to: icue.device.maxDpi
                            stepSize: 1
                            value: modelData.dpiX
                            enabled: modelData.enabled
                            onMoved: icue.setDpiStage(index, "dpiX", Math.round(value))
                        }
                        CueField {
                            objectName: "dpi-x-" + index
                            Layout.preferredWidth: 58
                            text: modelData.dpiX
                            horizontalAlignment: Text.AlignHCenter
                            validator: IntValidator {
                                bottom: icue.device.minDpi
                                top: icue.device.maxDpi
                            }
                            onEditingFinished: icue.setDpiStage(index, "dpiX", Number(text))
                        }
                        CueField {
                            visible: modelData.independentXY
                            Layout.preferredWidth: 58
                            text: modelData.dpiY
                            validator: IntValidator {
                                bottom: icue.device.minDpi
                                top: icue.device.maxDpi
                            }
                            onEditingFinished: icue.setDpiStage(index, "dpiY", Number(text))
                        }
                    }
                }
                CueButton {
                    objectName: "dpi-reset"
                    text: qsTranslate("OpenICUE", "Standard stellen")
                    onClicked: icue.resetDpiPreset()
                }
                CueButton {
                    objectName: "dpi-apply"
                    text: qsTranslate("OpenICUE", "Aktiven DPI-Wert anwenden")
                    isAccent: true
                    enabled: icue.canApplyDpi && !icue.isHardwareBusy
                    onClicked: icue.applyDpiSettings()
                }
                Text {
                    width: parent.width
                    text: qsTranslate("OpenICUE", "Wendet den aktiven Wert über USB an. DPI-Stufen bleiben im lokalen Profil.")
                    color: Theme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width
                    visible: icue.dpiState.isDpiActive === true
                    text: qsTranslate("OpenICUE", "Bestätigter DPI-Wert: %1").arg(icue.dpiState.dpiX || 0)
                    color: Theme.green
                    font.pixelSize: 11
                }
                CueButton {
                    objectName: "dpi-release"
                    visible: icue.canApplyDpi
                    enabled: !icue.isHardwareBusy
                    text: qsTranslate("OpenICUE", "Software-Steuerung beenden")
                    onClicked: icue.stopHardwareLighting()
                }
                Text {
                    width: parent.width
                    visible: icue.canApplyDpi
                    text: qsTranslate("OpenICUE", "Beenden gibt die gemeinsame DPI- und RGB-Steuerung frei.")
                    color: Theme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width
                    visible: !icue.canApplyDpi
                    text: qsTranslate("OpenICUE", "Darkstar per USB-Kabel anschließen, um DPI anzuwenden.")
                    color: Theme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
    CuePanel {
        Layout.preferredWidth: 256
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Anzeigefarben")
        Column {
            width: parent.width
            spacing: 22
            y: 12
            Repeater {
                model: ["DPI", qsTranslate("OpenICUE", "Scharfschütze"), qsTranslate("OpenICUE", "Profil")]
                delegate: RowLayout {
                    required property string modelData
                    required property int index
                    width: parent.width
                    Text {
                        text: qsTranslate("OpenICUE", modelData)
                        color: Theme.text
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        width: 22
                        height: 22
                        radius: 11
                        color: (icue.document.indicatorColors || [])[index] || "white"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                colorEditor.colorIndex = index;
                                colorEditor.initialColor = parent.color;
                                colorEditor.open();
                            }
                        }
                    }
                }
            }
        }
    }
    Item {
        Layout.fillWidth: true
    }
}
