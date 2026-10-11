import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

RowLayout {
    id: root
    spacing: 8
    property var presets: icue.document.eqPresets || []
    property int activePreset: icue.document.activeEq || 0
    property var gains: presets.length ? presets[activePreset].gains : []
    function presetName(preset) {
        return preset.isFactory ? qsTranslate("OpenICUE", preset.name) : preset.name;
    }
    onGainsChanged: graph.requestPaint()
    Dialog {
        id: addPreset
        anchors.centerIn: parent
        title: qsTranslate("OpenICUE", "EQ-Voreinstellung")
        modal: true
        footer: CueDialogButtons {}
        contentItem: CueField {
            id: newName
            text: qsTranslate("OpenICUE", "Neue Voreinstellung")
            implicitWidth: 250
        }
        onAccepted: icue.addEqPreset(newName.text)
    }
    CuePanel {
        Layout.preferredWidth: 160
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Allgemeine")
        Row {
            width: parent.width
            height: parent.height - 85
            Repeater {
                model: [
                    {
                        key: "microphone",
                        label: qsTranslate("OpenICUE", "Lautstärke"),
                        icon: "mic"
                    },
                    {
                        key: "sidetone",
                        label: qsTranslate("OpenICUE", "Nebenton"),
                        icon: "mic_off"
                    }
                ]
                delegate: Column {
                    required property var modelData
                    property var controlState: icue.mixer[modelData.key] || {}
                    width: 64
                    spacing: 5
                    CueSlider {
                        id: volume
                        width: 48
                        height: parent.parent.height - 48
                        orientation: Qt.Vertical
                        from: 0
                        to: 100
                        stepSize: 1
                        value: controlState.percent || 0
                        enabled: controlState.isAvailable || false
                        onMoved: icue.setMixerControl(modelData.key, Math.round(value), controlState.isMuted || false)
                    }
                    Image {
                        x: 16
                        width: 16
                        height: 16
                        source: "image://icons/controls/microphone"
                        opacity: controlState.isMuted ? 0.5 : 1
                        MouseArea {
                            anchors.fill: parent
                            enabled: controlState.isAvailable || false
                            onClicked: icue.setMixerControl(modelData.key, controlState.percent || 0, !controlState.isMuted)
                        }
                    }
                    Text {
                        width: 62
                        text: modelData.label
                        color: Theme.muted
                        elide: Text.ElideRight
                        font.pixelSize: 11
                    }
                }
            }
        }
        Row {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 30
            spacing: 4
            Text {
                width: 94
                text: qsTranslate("OpenICUE", "Automatische\nAudio-Neupositionierung")
                color: Theme.muted
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            CueToggle {
                enabled: false
            }
        }
        CueButton {
            objectName: "audio-options"
            anchors.bottom: parent.bottom
            width: parent.width
            text: icue.document.isSurroundEnabled ? "♬  7.1 Sound" : qsTranslate("OpenICUE", "♬  Stereo")
            onClicked: icue.toggleSurround()
        }
    }
    CuePanel {
        Layout.preferredWidth: 256
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "EQ-Voreinstellungen")
        Column {
            width: parent.width
            spacing: 4
            CueButton {
                width: parent.width
                text: "+"
                font.pixelSize: 24
                isAccent: true
                onClicked: addPreset.open()
            }
            Repeater {
                model: root.presets
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: 25
                    radius: 3
                    color: index === root.activePreset ? Theme.selected : "transparent"
                    Text {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.presetName(modelData)
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    MouseArea {
                        objectName: "eq-preset-" + index
                        anchors.fill: parent
                        onClicked: icue.selectEqPreset(index)
                    }
                    CueButton {
                        anchors.right: parent.right
                        width: 24
                        height: 24
                        text: "⋮"
                        visible: index === root.activePreset
                        onClicked: menu.popup()
                        Menu {
                            id: menu
                            MenuItem {
                                text: qsTranslate("OpenICUE", "Löschen")
                                enabled: root.presets.length > 1
                                onTriggered: icue.removeEqPreset(index)
                            }
                        }
                    }
                }
            }
        }
    }
    CuePanel {
        id: editor
        Layout.fillWidth: true
        Layout.fillHeight: true
        title: root.presets.length ? root.presetName(root.presets[root.activePreset]) : ""
        CueButton {
            x: parent.width - width
            y: -36
            text: qsTranslate("OpenICUE", "♙ Voicemod abrufen")
            enabled: false
        }
        Item {
            id: plot
            anchors.top: parent.top
            anchors.bottom: footer.top
            anchors.bottomMargin: 8
            width: parent.width
            property real leftMargin: 52
            property real rightMargin: 40
            property real graphHeight: height - 32
            Canvas {
                id: graph
                anchors.fill: parent
                onPaint: {
                    if (root.gains.length !== 10)
                        return;
                    const ctx = getContext("2d");
                    ctx.reset();
                    const zero = plot.graphHeight / 2;
                    const step = (plot.width - plot.leftMargin - plot.rightMargin) / 9;
                    let ys = root.gains.map(g => (12 - g) / 24 * plot.graphHeight);
                    let gradient = ctx.createLinearGradient(0, 0, 0, plot.graphHeight);
                    gradient.addColorStop(0, "#70e6027d");
                    gradient.addColorStop(0.5, "#50c120a9");
                    gradient.addColorStop(1, "#00785aff");
                    ctx.beginPath();
                    ctx.moveTo(plot.leftMargin, plot.graphHeight);
                    ctx.lineTo(plot.leftMargin, ys[0]);
                    for (let i = 1; i < 10; ++i) {
                        let x = plot.leftMargin + i * step;
                        ctx.bezierCurveTo(x - step / 2, ys[i - 1], x - step / 2, ys[i], x, ys[i]);
                    }
                    ctx.lineTo(plot.leftMargin + 9 * step, plot.graphHeight);
                    ctx.closePath();
                    ctx.fillStyle = gradient;
                    ctx.fill();
                    ctx.beginPath();
                    ctx.moveTo(plot.leftMargin, ys[0]);
                    for (let i = 1; i < 10; ++i) {
                        let x = plot.leftMargin + i * step;
                        ctx.bezierCurveTo(x - step / 2, ys[i - 1], x - step / 2, ys[i], x, ys[i]);
                    }
                    ctx.lineWidth = 2;
                    ctx.strokeStyle = "#dddddd";
                    ctx.stroke();
                }
            }
            Rectangle {
                x: 36
                y: plot.graphHeight / 2
                width: plot.width - 60
                height: 1
                color: "#303030"
            }
            Repeater {
                model: [12, 0, -12]
                delegate: Item {
                    required property int modelData
                    y: (12 - modelData) / 24 * plot.graphHeight - 6
                    width: plot.width
                    Text {
                        text: (modelData > 0 ? "+" : modelData < 0 ? "−" : "") + Math.abs(modelData) + " dB"
                        color: Theme.muted
                        font.pixelSize: 10
                    }
                    Text {
                        anchors.right: parent.right
                        text: (modelData > 0 ? "+" : modelData < 0 ? "−" : "") + Math.abs(modelData) + " dB"
                        color: Theme.muted
                        font.pixelSize: 10
                    }
                }
            }
            Repeater {
                model: [32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]
                delegate: Item {
                    required property int index
                    required property int modelData
                    x: plot.leftMargin + index * (plot.width - plot.leftMargin - plot.rightMargin) / 9 - 12
                    width: 24
                    height: plot.height
                    CueSlider {
                        objectName: "eq-band-" + index
                        x: 0
                        y: -8
                        height: plot.graphHeight + 16
                        width: 24
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                        stepSize: 0.5
                        value: root.gains[index] || 0
                        onMoved: icue.setEqGain(index, Math.abs(value) < 0.5 ? 0 : value)
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: plot.graphHeight + 12
                        text: modelData < 1000 ? modelData : (modelData / 1000) + "k"
                        color: Theme.muted
                        font.pixelSize: 10
                    }
                }
            }
        }
        RowLayout {
            id: footer
            anchors.bottom: parent.bottom
            width: parent.width
            spacing: 8
            Text {
                text: qsTranslate("OpenICUE", "EQ-Schieberegler verknüpfen")
                color: Theme.muted
                font.pixelSize: 11
            }
            CueToggle {
                objectName: "eq-linked"
                checked: icue.document.isLinkedEq || false
                onToggled: icue.setDocumentValue("isLinkedEq", checked)
            }
            Item {
                Layout.fillWidth: true
            }
            CueButton {
                objectName: "eq-reset"
                text: qsTranslate("OpenICUE", "Standard stellen")
                onClicked: icue.resetEqPreset()
            }
        }
    }
}
