import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

RowLayout {
    id: root
    objectName: "assignmentPanel"
    spacing: 8
    property string selectedKey: "Mouse2"
    signal keySelectionRequested(string key)
    property bool isSide: false
    property bool isGestureGroup: false
    property bool isRecording: false
    property int macroTab: 0
    property double lastEventTime: 0
    property var assignments: icue.document.assignments || []
    property var assignment: {
        for (const a of assignments)
            if (a.input === selectedKey)
                return a;
        return ({
                    input: selectedKey,
                    type: "Keyboard",
                    name: qsTranslate("OpenICUE", "Zuweisung"),
                    params: {}
                });
    }
    property var types: [
        {
            key: "Keyboard",
            label: qsTranslate("OpenICUE", "Tastatur"),
            icon: "remap-keyboard",
            group: "Neu zuordnen"
        },
        {
            key: "Language",
            label: qsTranslate("OpenICUE", "Sprache"),
            icon: "remap-lang",
            group: "Neu zuordnen"
        },
        {
            key: "Mouse",
            label: qsTranslate("OpenICUE", "Maus"),
            icon: "remap-mouse",
            group: "Neu zuordnen"
        },
        {
            key: "Keystroke",
            label: qsTranslate("OpenICUE", "Tastendruck"),
            icon: "remap-keystroke",
            group: "Neu zuordnen"
        },
        {
            key: "Text",
            label: qsTranslate("OpenICUE", "Text"),
            icon: "text",
            group: "Regulär"
        },
        {
            key: "Media",
            label: qsTranslate("OpenICUE", "Medien"),
            icon: "play-pause",
            group: "Regulär"
        },
        {
            key: "Launch",
            label: qsTranslate("OpenICUE", "Anwendung starten"),
            icon: "launch",
            group: "Regulär"
        },
        {
            key: "Disable",
            label: qsTranslate("OpenICUE", "Deaktivieren"),
            icon: "block_dark",
            group: "Regulär"
        },
        {
            key: "Profile",
            label: qsTranslate("OpenICUE", "Profil wechseln"),
            icon: "switch",
            group: "Regulär"
        },
        {
            key: "Macro",
            label: qsTranslate("OpenICUE", "Makro"),
            icon: "macro",
            group: "Regulär"
        }
    ]
    function update(field, value) {
        let values = assignments.slice();
        let item = Object.assign({}, assignment);
        item[field] = value;
        let index = values.findIndex(a => a.input === selectedKey);
        if (index < 0)
            values.push(item);
        else
            values[index] = item;
        icue.setDocumentValue("assignments", values);
    }
    function param(key, value) {
        const params = Object.assign({}, assignment.params);
        params[key] = value;
        update("params", params);
    }
    function keyLabel(key) {
        for (const group of ["extendedRows", "mainRows", "navigationRows", "numpadRows"])
            for (const row of icue.assignmentKeyboard[group])
                for (const item of row)
                    if (item.id === key)
                        return (key.startsWith("Keypad") ? qsTranslate("OpenICUE", "Num ") : "") + qsTranslate(
                                    "OpenICUE", item.label);
        return key;
    }
    function record(event, type) {
        if (!isRecording || event.isAutoRepeat)
            return;
        const now = Date.now();
        let events = (assignment.params.events || []).slice();
        events.push({
                        type: type,
                        key: event.key,
                        text: event.text,
                        delayMs: lastEventTime > 0 ? now - lastEventTime : 0
                    });
        lastEventTime = now;
        param("events", events);
        event.accepted = true;
    }
    focus: isRecording
    Keys.onPressed: event => record(event, "keyDown")
    Keys.onReleased: event => record(event, "keyUp")
    VirtualKeyboard {
        id: keyboard
        onChosen: key => root.param("key", key)
    }
    CuePanel {
        Layout.preferredWidth: 256
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Zuweisungen")
        Column {
            width: parent.width
            spacing: 8
            CueButton {
                text: "+"
                width: parent.width
                isAccent: true
                onClicked: root.update("name", qsTranslate("OpenICUE", "Zuweisung %1").arg(
                                           root.assignments.length + 1))
            }
            Repeater {
                model: root.assignments
                Rectangle {
                    required property var modelData
                    width: parent.width
                    height: 28
                    radius: 3
                    color: root.selectedKey === modelData.input ? Theme.selected : "transparent"
                    Text {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name + " · " + modelData.input
                        color: Theme.text
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
    CuePanel {
        Layout.preferredWidth: 240
        Layout.fillHeight: true
        title: qsTranslate("OpenICUE", "Zuweisungstyp")
        Column {
            width: parent.width
            spacing: 12
            Repeater {
                model: ["Neu zuordnen", "Regulär"]
                Column {
                    required property string modelData
                    width: parent.width
                    spacing: 8
                    Text {
                        text: qsTranslate("OpenICUE", modelData)
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    Flow {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: root.types.filter(t => t.group === modelData)
                            Column {
                                required property var modelData
                                width: 62
                                spacing: 3
                                Rectangle {
                                    width: 62
                                    height: 40
                                    radius: 3
                                    color: root.assignment.type === modelData.key ? "#dddddd" :
                                                                                    "#303030"
                                    ThemedIcon {
                                        anchors.centerIn: parent
                                        width: 30
                                        height: 30
                                        iconSource: icue.iconSource("assignments",
                                                                    modelData.key.toLowerCase(),
                                                                    "qrc:/vendor/cue-icons/assignments/"
                                                                    + modelData.icon + ".svg")
                                        isSelected: root.assignment.type === modelData.key
                                    }
                                    MouseArea {
                                        objectName: "assignment-type-" + modelData.key
                                        anchors.fill: parent
                                        onClicked: root.update("type", modelData.key)
                                    }
                                }
                                Text {
                                    width: parent.width
                                    text: qsTranslate("OpenICUE", modelData.label)
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                    color: Theme.muted
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    CuePanel {
        Layout.preferredWidth: 184
        Layout.fillHeight: true
        title: root.isGestureGroup ? qsTranslate("OpenICUE", "Gesten") : root.isSide ? qsTranslate("OpenICUE",
                                                                                                   "Seitentasten") :
                                                                                       qsTranslate(
                                                                                           "OpenICUE",
                                                                                           "Kerntasten")
        Flickable {
            anchors.fill: parent
            contentHeight: keysColumn.implicitHeight
            clip: true
            Column {
                id: keysColumn
                width: parent.width
                spacing: 5
                CueButton {
                    objectName: "assignment-toggle-side"
                    width: parent.width
                    visible: icue.device.kind === "Mouse"
                    text: root.isSide ? qsTranslate("OpenICUE", "Kerntasten") : qsTranslate(
                                            "OpenICUE", "Seitentasten")
                    onClicked: {
                        root.isGestureGroup = false;
                        root.isSide = !root.isSide;
                    }
                }
                CueButton {
                    objectName: "assignment-toggle-gestures"
                    width: parent.width
                    visible: icue.device.kind === "Mouse"
                    text: root.isGestureGroup ? qsTranslate("OpenICUE", "Tasten") : qsTranslate(
                                                    "OpenICUE", "Gesten")
                    onClicked: root.isGestureGroup = !root.isGestureGroup
                }
                Repeater {
                    model: icue.keys.filter(k => root.isGestureGroup ? k.id.startsWith("Gesture_") :
                                                                       !k.id.startsWith("Gesture_")
                                                                       && (icue.device.kind
                                                                           !== "Mouse" || k.isSide
                                                                           === root.isSide))
                    CueButton {
                        required property var modelData
                        objectName: "assignment-input-" + modelData.id
                        text: qsTranslate("OpenICUE", modelData.label)
                        width: parent.width
                        isAccent: root.selectedKey === modelData.id
                        onClicked: root.keySelectionRequested(modelData.id)
                    }
                }
            }
        }
    }
    CuePanel {
        id: editor
        Layout.fillWidth: true
        Layout.fillHeight: true
        title: root.assignment.type === "Macro" ? qsTranslate("OpenICUE", "Makro") : qsTranslate("OpenICUE",
                                                                                                 "Zuweisung")
        color: Theme.raised
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            CueField {
                text: root.assignment.name
                Layout.fillWidth: true
                onEditingFinished: root.update("name", text)
            }
            ColumnLayout {
                visible: ["Keyboard", "Language", "Keystroke"].includes(root.assignment.type)
                Layout.fillWidth: true
                CueButton {
                    objectName: "assignment-keyboard"
                    text: root.assignment.params.key ? root.keyLabel(root.assignment.params.key) : qsTranslate(
                                                           "OpenICUE", "Taste auswählen")
                    onClicked: {
                        keyboard.selectedKey = root.assignment.params.key || "";
                        keyboard.open();
                    }
                }
                Row {
                    spacing: 8
                    Repeater {
                        model: ["Ctrl", "Alt", "Shift", "Win"]
                        CheckBox {
                            required property string modelData
                            text: qsTranslate("OpenICUE", modelData)
                            font.pixelSize: 10
                            checked: (root.assignment.params.modifiers || []).includes(modelData)
                            onToggled: {
                                let values = (root.assignment.params.modifiers || []).slice();
                                if (checked)
                                    values.push(modelData);
                                else
                                    values = values.filter(v => v !== modelData);
                                root.param("modifiers", values);
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                visible: root.assignment.type === "Text"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Text {
                    text: qsTranslate("OpenICUE", "Text eingeben")
                    color: Theme.text
                    font.pixelSize: 12
                }
                TextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: root.assignment.params.text || ""
                    color: Theme.text
                    font.pixelSize: 12
                    wrapMode: TextEdit.Wrap
                    background: Rectangle {
                        color: "#272727"
                        border.color: "#555555"
                    }
                    onEditingFinished: root.param("text", text)
                }
            }
            TranslatedComboBox {
                visible: root.assignment.type === "Mouse"
                sourceOptions: ["Linksklick", "Rechtsklick", "Mittelklick", "Zurück", "Weiterleiten",
                    "Nach oben scrollen", "Nach unten scrollen"]
                onActivated: root.param("button", sourceOptions[currentIndex])
            }
            TranslatedComboBox {
                visible: root.assignment.type === "Media"
                sourceOptions: ["Wiedergabe/Pause", "Nächster Titel", "Vorheriger Titel", "Lauter",
                    "Leiser", "Stumm"]
                onActivated: root.param("media", sourceOptions[currentIndex])
            }
            CueField {
                visible: root.assignment.type === "Launch"
                placeholderText: qsTranslate("OpenICUE", "Anwendung / Pfad")
                text: root.assignment.params.program || ""
                Layout.fillWidth: true
                onEditingFinished: root.param("program", text)
            }
            ComboBox {
                visible: root.assignment.type === "Profile"
                model: icue.profiles
                textRole: "name"
                valueRole: "id"
                onActivated: root.param("profileId", currentValue)
            }
            ColumnLayout {
                visible: root.assignment.type === "Macro"
                Layout.fillWidth: true
                Layout.fillHeight: true
                RowLayout {
                    Repeater {
                        model: [qsTranslate("OpenICUE", "Aufzeichnung"), qsTranslate("OpenICUE",
                                                                                     "Allgemeine"),
                            qsTranslate("OpenICUE", "Erweitert")]
                        CueButton {
                            required property string modelData
                            required property int index
                            text: qsTranslate("OpenICUE", modelData)
                            isAccent: root.macroTab === index
                            onClicked: root.macroTab = index
                        }
                    }
                }
                ColumnLayout {
                    visible: root.macroTab === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    CueButton {
                        text: root.isRecording ? qsTranslate("OpenICUE", "■ Stoppen") : qsTranslate(
                                                     "OpenICUE", "● Aufzeichnen")
                        onClicked: {
                            root.isRecording = !root.isRecording;
                            root.lastEventTime = 0;
                            if (root.isRecording)
                                root.forceActiveFocus();
                        }
                    }
                    Text {
                        text: qsTranslate("OpenICUE", "Tastenaufzeichnung in diesem Fenster")
                        color: Theme.muted
                        font.pixelSize: 10
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root.assignment.params.events || []
                        clip: true
                        delegate: Text {
                            required property var modelData
                            text: modelData.delayMs + " ms · " + qsTranslate("OpenICUE",
                                                                             modelData.type)
                                  + " · " + (modelData.text || modelData.key)
                            color: Theme.text
                            font.pixelSize: 11
                            height: 24
                        }
                    }
                    CueButton {
                        text: qsTranslate("OpenICUE", "Ereignisse löschen")
                        onClicked: root.param("events", [])
                    }
                }
                ColumnLayout {
                    visible: root.macroTab === 1
                    Layout.fillWidth: true
                    Repeater {
                        model: ["Verzögerungen", "Tastaturereignisse", "Mausklicks",
                            "Mausradbewegungen", "Mausbewegungen"]
                        CheckBox {
                            required property string modelData
                            text: qsTranslate("OpenICUE", modelData)
                            checked: root.assignment.params[modelData] || false
                            onToggled: root.param(modelData, checked)
                        }
                    }
                }
                ColumnLayout {
                    visible: root.macroTab === 2
                    Layout.fillWidth: true
                    Text {
                        text: qsTranslate("OpenICUE", "Auslöser")
                        color: Theme.text
                    }
                    ComboBox {
                        model: [qsTranslate("OpenICUE", "Beim Drücken"), qsTranslate("OpenICUE",
                                                                                     "Beim Loslassen"),
                            qsTranslate("OpenICUE", "Während gedrückt"), qsTranslate("OpenICUE",
                                                                                     "Umschalten")]
                        onActivated: root.param("trigger", currentIndex)
                    }
                    CheckBox {
                        text: qsTranslate("OpenICUE", "Wiederholen")
                        checked: root.assignment.params.isRepeating || false
                        onToggled: root.param("isRepeating", checked)
                    }
                    CheckBox {
                        text: qsTranslate("OpenICUE", "Unterbrechen")
                        checked: root.assignment.params.isInterruptible || false
                        onToggled: root.param("isInterruptible", checked)
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }
}
