import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

RowLayout {
    id: root
    objectName: "lightingPanel"
    spacing: 8
    property int layerIndex: 0
    property var layers: icue.document.lightingLayers || []
    property var selectedLayer: layers[layerIndex] || ({})
    property var effect: {
        for (const e of icue.lightingTypes)
            if (e.id === selectedLayer.type)
                return e;
        return {};
    }
    property int editingColorIndex: 0
    property var pendingSelection: []
    signal pendingSelectionRequested(var ids)
    readonly property var selectedZoneIds: layers.length ? (selectedLayer.zones || []).includes("all") ? icue.lightingKeys.map(k => k.id) : selectedLayer.zones || [] : pendingSelection
    function hasControl(name) {
        return (effect.controls || []).indexOf(name) >= 0;
    }
    function update(field, value) {
        let values = layers.slice();
        if (!values.length)
            return;
        let item = Object.assign({}, values[layerIndex]);
        item[field] = value;
        if (field === "colors" && item.transitions) {
            if (value.length !== item.transitions.length)
                return;
            item.transitions = item.transitions.map((point, index) => Object.assign({}, point, {
                    color: value[index]
                }));
        } else if (field === "transitions") {
            item.colors = value.map(point => point.color);
        } else if (field === "type" && value !== selectedLayer.type) {
            delete item.transitions;
            delete item.isLooping;
        }
        values[layerIndex] = item;
        icue.setDocumentValue("lightingLayers", values);
    }
    function selectZones(ids) {
        if (layers.length)
            update("zones", ids);
        else
            pendingSelectionRequested(ids);
    }
    function addLayer() {
        const values = layers.slice();
        if (values.length >= icue.device.maxLightingLayers)
            return;
        const zones = selectedZoneIds.length ? selectedZoneIds.slice() : ["all"];
        values.push({
            name: qsTranslate("OpenICUE", "Beleuchtungsebene %1").arg(values.length + 1),
            type: icue.lightingTypes.length ? icue.lightingTypes[0].id : "",
            colors: ["#ff0000", "#00bfff"],
            speed: 1,
            duration: 1,
            opacity: 100,
            isRandom: true,
            direction: 0,
            zones: zones
        });
        icue.setDocumentValue("lightingLayers", values);
        if (icue.document.lightingLayers.length === values.length)
            layerIndex = values.length - 1;
    }
    function duplicateLayer() {
        if (!layers.length || layers.length >= icue.device.maxLightingLayers)
            return;
        const values = layers.slice();
        const copy = Object.assign({}, selectedLayer);
        copy.name = qsTranslate("OpenICUE", "%1 Kopie").arg(copy.name);
        values.push(copy);
        icue.setDocumentValue("lightingLayers", values);
        layerIndex = values.length - 1;
    }
    ColorEditor {
        id: colors
        onChosen: color => {
            let values = (root.selectedLayer.colors || []).slice();
            values[root.editingColorIndex] = color;
            root.update("colors", values);
        }
    }
    CuePanel {
        Layout.preferredWidth: 256
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Beleuchtungsebenen")
        Column {
            width: parent.width
            spacing: 4
            CueButton {
                objectName: "lighting-add"
                text: "+"
                isAccent: true
                width: parent.width
                enabled: root.layers.length < icue.device.maxLightingLayers
                onClicked: root.addLayer()
            }
            Repeater {
                model: root.layers
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: 28
                    radius: 3
                    color: root.layerIndex === index ? Theme.selected : "transparent"
                    Text {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    MouseArea {
                        objectName: "lighting-layer-" + index
                        anchors.fill: parent
                        onClicked: root.layerIndex = index
                    }
                    CueButton {
                        width: 26
                        anchors.right: parent.right
                        text: "×"
                        onClicked: {
                            let values = root.layers.slice();
                            values.splice(index, 1);
                            root.layerIndex = Math.max(0, root.layerIndex - 1);
                            icue.setDocumentValue("lightingLayers", values);
                        }
                    }
                }
            }
            CueButton {
                objectName: "lighting-duplicate"
                text: qsTranslate("OpenICUE", "Ebene duplizieren")
                width: parent.width
                enabled: root.layers.length > 0 && root.layers.length < icue.device.maxLightingLayers
                onClicked: root.duplicateLayer()
            }
        }
    }
    CuePanel {
        Layout.preferredWidth: 256
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Beleuchtungstyp")
        Flickable {
            objectName: "lighting-type-list"
            anchors.fill: parent
            contentHeight: typeGroups.implicitHeight
            contentWidth: width
            clip: true
            ScrollBar.vertical: ScrollBar {}
            Column {
                id: typeGroups
                width: parent.width
                spacing: 12
                Repeater {
                    model: [false, true]
                    delegate: Column {
                        required property bool modelData
                        width: parent.width
                        spacing: 8
                        Text {
                            text: modelData ? qsTranslate("OpenICUE", "Benutzerdefiniert") : qsTranslate("OpenICUE", "Voreinstellung")
                            color: Theme.text
                            font.pixelSize: 12
                        }
                        Flow {
                            width: parent.width
                            spacing: 8
                            Repeater {
                                model: icue.lightingTypes.filter(e => e.isCustom === modelData)
                                delegate: Column {
                                    required property var modelData
                                    width: 64
                                    spacing: 3
                                    Rectangle {
                                        width: 64
                                        height: 40
                                        radius: 3
                                        color: root.selectedLayer.type === modelData.id ? "#eeeeee" : "#303030"
                                        ThemedIcon {
                                            anchors.centerIn: parent
                                            width: 36
                                            height: 30
                                            iconSource: modelData.icon
                                            isSelected: root.selectedLayer.type === modelData.id
                                        }
                                        MouseArea {
                                            objectName: "lighting-type-" + modelData.key
                                            anchors.fill: parent
                                            enabled: root.layers.length > 0
                                            onClicked: root.update("type", modelData.id)
                                        }
                                    }
                                    Text {
                                        width: parent.width
                                        height: 28
                                        text: modelData.label
                                        color: Theme.muted
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    CuePanel {
        Layout.preferredWidth: 216
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Schnellbeleuchtungszone")
        Column {
            width: parent.width
            spacing: 18
            CueButton {
                objectName: "lighting-select-all"
                text: qsTranslate("OpenICUE", "Alle")
                width: parent.width
                onClicked: root.selectZones(icue.lightingKeys.map(k => k.id))
            }
            CueButton {
                objectName: "lighting-clear-selection"
                text: qsTranslate("OpenICUE", "Auswahl aufheben")
                width: parent.width
                onClicked: root.selectZones([])
            }
            Text {
                text: qsTranslate("OpenICUE", "Ausgewählte Tasten / LEDs: %1").arg(root.selectedZoneIds.length)
                color: Theme.text
                font.pixelSize: 11
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Text {
                text: qsTranslate("OpenICUE", "Shift + Klick fügt hinzu oder entfernt. Mit gedrückter linker Maustaste einen Bereich ziehen.")
                color: Theme.muted
                font.pixelSize: 11
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
    }
    CuePanel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        title: root.effect.label || qsTranslate("OpenICUE", "Beleuchtung")
        color: Theme.raised
        Flickable {
            objectName: "lighting-settings-scroll"
            anchors.fill: parent
            clip: true
            contentHeight: lightingEditor.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: lightingEditor
                width: parent.width
                spacing: 14
                enabled: root.layers.length > 0
                Text {
                    visible: root.layers.length === 0
                    text: qsTranslate("OpenICUE", "Beleuchtungsebene hinzufügen")
                    color: Theme.muted
                    font.pixelSize: 12
                }
                CueField {
                    objectName: "lighting-layer-name"
                    Layout.fillWidth: true
                    text: root.selectedLayer.name || ""
                    visible: root.layers.length > 0
                    onEditingFinished: root.update("name", text)
                }
                RowLayout {
                    visible: root.hasControl("random") && root.layers.length > 0
                    spacing: 8
                    RadioButton {
                        objectName: "lighting-random-colors"
                        text: qsTranslate("OpenICUE", "Zufällige Farben")
                        checked: root.selectedLayer.isRandom || false
                        onClicked: root.update("isRandom", true)
                    }
                    RadioButton {
                        objectName: "lighting-fixed-colors"
                        text: qsTranslate("OpenICUE", "Eigene Farben")
                        checked: !root.selectedLayer.isRandom
                        onClicked: root.update("isRandom", false)
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.hasControl("colors") && (!root.hasControl("random") || !root.selectedLayer.isRandom)
                    Repeater {
                        model: root.effect.key === "StaticCustom" ? (root.selectedLayer.colors || []).slice(0, 1) : root.selectedLayer.colors || []
                        Rectangle {
                            required property string modelData
                            required property int index
                            width: 32
                            height: 32
                            radius: 3
                            color: modelData
                            MouseArea {
                                objectName: "lighting-color-" + index
                                anchors.fill: parent
                                onClicked: {
                                    root.editingColorIndex = index;
                                    colors.initialColor = modelData;
                                    colors.open();
                                }
                            }
                        }
                    }
                    CueButton {
                        objectName: "lighting-color-add"
                        text: "+"
                        visible: root.effect.key === "Gradient"
                        onClicked: {
                            let values = root.selectedLayer.colors.slice();
                            if (values.length < 12) {
                                if (root.selectedLayer.transitions) {
                                    let points = root.selectedLayer.transitions.slice();
                                    const last = points.length - 1;
                                    points.splice(last, 0, {
                                        time: (points[last - 1].time + points[last].time) / 2,
                                        color: "#ffffff"
                                    });
                                    root.update("transitions", points);
                                    return;
                                }
                                values.push("#ffffff");
                                root.update("colors", values);
                            }
                        }
                    }
                }
                ColumnLayout {
                    visible: !!root.selectedLayer.transitions
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: qsTranslate("OpenICUE", "Übergänge (% der Dauer)")
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    Repeater {
                        model: root.selectedLayer.transitions || []
                        RowLayout {
                            required property var modelData
                            required property int index
                            Text {
                                text: index + 1
                                color: Theme.muted
                                Layout.preferredWidth: 24
                            }
                            CueField {
                                objectName: "lighting-transition-time-" + index
                                text: String(modelData.time * 100)
                                enabled: index > 0 && index < root.selectedLayer.transitions.length - 1
                                validator: DoubleValidator {
                                    bottom: index > 0 ? root.selectedLayer.transitions[index - 1].time * 100 : 0
                                    top: index + 1 < root.selectedLayer.transitions.length ? root.selectedLayer.transitions[index + 1].time * 100 : 100
                                }
                                onEditingFinished: {
                                    let points = root.selectedLayer.transitions.slice();
                                    points[index] = Object.assign({}, points[index], {
                                        time: Number(text) / 100
                                    });
                                    root.update("transitions", points);
                                }
                            }
                            Text {
                                text: "%"
                                color: Theme.muted
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
                Text {
                    visible: root.hasControl("speed") && !root.selectedLayer.transitions
                    text: qsTranslate("OpenICUE", "Geschwindigkeit")
                    color: Theme.text
                    font.pixelSize: 12
                }
                RowLayout {
                    visible: root.hasControl("speed") && !root.selectedLayer.transitions
                    Layout.fillWidth: true
                    Image {
                        source: "image://icons/controls/speed-slow"
                        sourceSize.width: 22
                        sourceSize.height: 22
                    }
                    CueSlider {
                        objectName: "lighting-speed"
                        Layout.fillWidth: true
                        from: 0
                        to: 2
                        stepSize: 1
                        value: root.selectedLayer.speed || 0
                        onMoved: root.update("speed", Math.round(value))
                    }
                    Image {
                        source: "image://icons/controls/speed-fast"
                        sourceSize.width: 22
                        sourceSize.height: 22
                    }
                }
                RowLayout {
                    visible: root.hasControl("duration")
                    Text {
                        text: qsTranslate("OpenICUE", "Dauer")
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    CueField {
                        objectName: "lighting-duration"
                        text: root.selectedLayer.duration || 1
                        validator: DoubleValidator {
                            bottom: 0.1
                            top: 60
                        }
                        onEditingFinished: root.update("duration", Number(text))
                    }
                    Text {
                        text: qsTranslate("OpenICUE", "Sekunden")
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                }
                ComboBox {
                    objectName: "lighting-direction"
                    visible: root.hasControl("direction") && !root.selectedLayer.transitions
                    Layout.fillWidth: true
                    model: [qsTranslate("OpenICUE", "Nach rechts"), qsTranslate("OpenICUE", "Nach links"), qsTranslate("OpenICUE", "Nach unten"), qsTranslate("OpenICUE", "Nach oben")]
                    currentIndex: root.selectedLayer.direction || 0
                    onActivated: root.update("direction", currentIndex)
                }
                Text {
                    visible: root.effect.key === "FrequencyVisualizer"
                    text: qsTranslate("OpenICUE", "Audioanalyse noch nicht verfügbar")
                    color: Theme.muted
                    font.pixelSize: 11
                }
                ColumnLayout {
                    visible: root.effect.key === "StaticCustom"
                    Layout.fillWidth: true
                    Text {
                        text: qsTranslate("OpenICUE", "Deckkraft")
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    CueSlider {
                        objectName: "lighting-opacity"
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: root.selectedLayer.opacity === undefined ? 100 : root.selectedLayer.opacity
                        onMoved: root.update("opacity", Math.round(value))
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
