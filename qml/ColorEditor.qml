import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Popup {
    id: root
    objectName: "colorEditorPopup"
    width: 286
    height: 390
    modal: true
    anchors.centerIn: Overlay.overlay
    property color initialColor: "#00bfff"
    readonly property bool isValidColor: /^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/.test(hex.text)
    property bool isAccepted: false
    property bool isInitializing: false
    property string originalColor: ""
    signal chosen(string color)
    background: Rectangle {
        color: "#262626"
        radius: 4
        border.color: "#454545"
    }
    onOpened: {
        isInitializing = true;
        isAccepted = false;
        originalColor = initialColor.toString();
        hex.text = originalColor;
        brightness.value = initialColor.hsvValue;
        isInitializing = false;
    }
    onClosed: {
        if (!isAccepted && originalColor.length)
            chosen(originalColor);
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Canvas {
            id: wheel
            Layout.alignment: Qt.AlignHCenter
            width: 218
            height: 218
            onPaint: {
                const c = getContext("2d");
                const radius = width / 2;
                for (let angle = 0; angle < 360; angle += 2) {
                    c.beginPath();
                    c.moveTo(radius, radius);
                    c.arc(radius, radius, radius - 2, angle * Math.PI / 180, (angle + 3) * Math.PI / 180);
                    c.closePath();
                    c.fillStyle = Qt.hsva(angle / 360, 1, 1, 1);
                    c.fill();
                }
                const gradient = c.createRadialGradient(radius, radius, 0, radius, radius, radius);
                gradient.addColorStop(0, "white");
                gradient.addColorStop(1, "transparent");
                c.fillStyle = gradient;
                c.beginPath();
                c.arc(radius, radius, radius - 2, 0, 2 * Math.PI);
                c.fill();
            }
            MouseArea {
                objectName: "color-wheel"
                anchors.fill: parent
                onPressed: event => update(event)
                onPositionChanged: event => {
                    if (pressed)
                        update(event);
                }
                function update(event) {
                    const x = event.x - width / 2, y = event.y - height / 2;
                    const hue = (Math.atan2(y, x) / (Math.PI * 2) + 1) % 1;
                    const sat = Math.min(1, Math.sqrt(x * x + y * y) / (width / 2));
                    hex.text = Qt.hsva(hue, sat, brightness.value, 1).toString();
                }
            }
        }
        CueSlider {
            id: brightness
            Layout.fillWidth: true
            from: 0
            to: 1
            value: 1
            onMoved: {
                let c = Qt.color(hex.text);
                hex.text = Qt.hsva(c.hsvHue, c.hsvSaturation, value, 1).toString();
            }
        }
        RowLayout {
            Text {
                text: "HEX"
                color: Theme.muted
            }
            CueField {
                id: hex
                objectName: "color-hex"
                text: root.initialColor.toString()
                Layout.fillWidth: true
                maximumLength: 9
                onTextChanged: {
                    if (root.opened && !root.isInitializing && root.isValidColor)
                        root.chosen(text);
                }
            }
            Rectangle {
                width: 26
                height: 26
                color: root.isValidColor ? Qt.color(hex.text) : root.initialColor
            }
        }
        RowLayout {
            Repeater {
                model: ["#ff0000", "#ff9900", "#ffff00", "#00ff00", "#00ffff", "#0000ff", "#ff00ff", "#ffffff"]
                Rectangle {
                    required property string modelData
                    width: 24
                    height: 18
                    color: modelData
                    MouseArea {
                        objectName: "color-preset-" + modelData.slice(1)
                        anchors.fill: parent
                        onClicked: hex.text = modelData
                    }
                }
            }
        }
        RowLayout {
            Item {
                Layout.fillWidth: true
            }
            CueButton {
                objectName: "color-cancel"
                text: qsTranslate("OpenICUE", "Abbrechen")
                onClicked: root.close()
            }
            CueButton {
                objectName: "color-confirm"
                text: qsTranslate("OpenICUE", "OK")
                isAccent: true
                enabled: root.isValidColor
                onClicked: {
                    root.isAccepted = true;
                    root.chosen(hex.text);
                    root.close();
                }
            }
        }
    }
}
