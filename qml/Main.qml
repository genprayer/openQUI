import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import OpenICUE 1.0
import "."

ApplicationWindow {
    id: root
    objectName: "icueWindow"
    width: 1368
    height: 1050
    minimumWidth: 1280
    minimumHeight: 840
    visible: true
    title: "OpenQUI"
    color: Theme.background
    flags: Qt.Window | Qt.FramelessWindowHint
    palette.window: Theme.raised
    palette.windowText: Theme.text
    palette.base: "#292929"
    palette.text: Theme.text
    palette.button: Theme.selected
    palette.buttonText: Theme.text
    palette.highlight: Theme.selected
    property string panelId: initialPanel
    property string selectedKey: "Mouse2"
    property string deviceModelId: icue.selectedModelId
    property var pendingLightingSelection: []
    readonly property var lightingSelectionIds: panels.item && panels.item.hasOwnProperty("selectedZoneIds") ? panels.item.selectedZoneIds : []
    function selectDefaultKey() {
        selectedKey = icue.device.kind === "Mouse" ? "Mouse2" : icue.keys.length ? icue.keys[0].id : "";

        pendingLightingSelection = [];
        viewIndex = 0;
    }
    function selectAssignmentKey(key) {
        selectedKey = key;
        for (const entry of icue.keys)
            if (entry.id === key) {
                viewIndex = entry.view;
                break;
            }
    }
    onDeviceModelIdChanged: selectDefaultKey()
    Component.onCompleted: {
        selectDefaultKey();
        icue.setLightingEditorActive(panelId === "Lightings");
    }
    property int viewIndex: 0
    onPanelIdChanged: {
        viewIndex = 0;
        icue.setLightingEditorActive(panelId === "Lightings");
    }
    Connections {
        target: icue
        function onNoticeChanged() {
            noticeTimer.restart();
        }
    }
    Timer {
        id: noticeTimer
        interval: 8000
    }
    FileDialog {
        id: importDialog
        title: qsTranslate("OpenICUE", "iCUE-Profil importieren")
        nameFilters: [qsTranslate("OpenICUE", "iCUE Profile (*.cueprofile)")]
        onAccepted: icue.importProfile(selectedFile.toString())
    }
    Dialog {
        id: profileDialog
        title: qsTranslate("OpenICUE", "Profil erstellen")
        anchors.centerIn: parent
        modal: true
        footer: CueDialogButtons {}
        background: Rectangle {
            color: Theme.raised
            radius: 4
        }
        contentItem: CueField {
            id: profileName
            placeholderText: qsTranslate("OpenICUE", "Profilname")
            text: qsTranslate("OpenICUE", "Neues Profil")
            implicitWidth: 260
        }
        onAccepted: icue.createProfile(profileName.text)
    }
    Popup {
        id: profilesPopup
        x: 80
        y: 112
        width: 304
        height: 410
        modal: false
        background: Rectangle {
            color: Theme.panel
            radius: 4
            border.color: "#333333"
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            RowLayout {
                Text {
                    text: qsTranslate("OpenICUE", "PROFILE")
                    color: Theme.text
                    Layout.fillWidth: true
                }
                CueButton {
                    text: "+"
                    isAccent: true
                    onClicked: profileDialog.open()
                }
                CueButton {
                    text: qsTranslate("OpenICUE", "Importieren")
                    onClicked: importDialog.open()
                }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: icue.profiles
                clip: true
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 40
                    radius: 3
                    color: modelData.id === icue.activeProfileId ? Theme.selected : "transparent"
                    Text {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name
                        color: Theme.text
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: icue.selectProfile(modelData.id)
                    }
                    CueButton {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 26
                        text: "×"
                        enabled: icue.profiles.length > 1
                        onClicked: icue.removeProfile(modelData.id)
                    }
                }
            }
        }
    }
    Rectangle {
        id: titleBar
        height: 40
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#181818"
        MouseArea {
            anchors.fill: parent
            onPressed: root.startSystemMove()
            onDoubleClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
        }
        Image {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            source: "qrc:/app/branding/open-cue-gui.svg"
            fillMode: Image.PreserveAspectFit
        }
        Text {
            objectName: "app-brand-label"
            x: 52
            anchors.verticalCenter: parent.verticalCenter
            text: "OpenQUI"
            color: "white"
            font.pixelSize: 19
            font.bold: true
        }
        Row {
            x: 200
            height: parent.height
            spacing: 28
            Repeater {
                model: [qsTranslate("OpenICUE", "STARTSEITE"), qsTranslate("OpenICUE", "DASHBOARD")]
                Text {
                    required property string modelData
                    text: qsTranslate("OpenICUE", modelData)
                    color: Theme.muted
                    font.pixelSize: 12
                    height: 40
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        Row {
            anchors.right: parent.right
            height: 40
            spacing: 4
            CueButton {
                objectName: "application-settings"
                width: 40
                height: 40
                imageSource: "image://icons/controls/settings"
                isFlat: true
                onClicked: settings.openGeneral()
            }
            CueButton {
                width: 40
                height: 40
                text: "—"
                onClicked: root.showMinimized()
            }
            CueButton {
                width: 40
                height: 40
                text: "□"
                onClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
            }
            CueButton {
                width: 40
                height: 40
                text: "×"
                onClicked: root.close()
            }
        }
    }
    Rectangle {
        id: deviceRibbon
        y: 40
        height: 72
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#161616"
        Row {
            x: 92
            spacing: 8
            height: parent.height
            Repeater {
                model: icue.devices
                delegate: Rectangle {
                    required property var modelData
                    width: 142
                    height: 72
                    color: modelData.id === icue.selectedModelId ? "#303030" : "transparent"
                    Image {
                        x: 8
                        y: 12
                        width: 46
                        height: 44
                        source: modelData.icon
                        fillMode: Image.PreserveAspectFit
                        opacity: modelData.isConnected ? 1 : 0.45
                    }
                    Text {
                        x: 60
                        width: 77
                        height: 72
                        text: modelData.name
                        color: Theme.text
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        verticalAlignment: Text.AlignVCenter
                    }
                    Rectangle {
                        x: 58
                        y: 53
                        width: 5
                        height: 5
                        radius: 3
                        color: modelData.isConnected ? Theme.green : Theme.muted
                    }
                    Text {
                        x: 70
                        y: 51
                        visible: modelData.telemetry && modelData.telemetry.hasBattery || false
                        text: visible ? modelData.telemetry.batteryPercent + "%" : ""
                        color: Theme.muted
                        font.pixelSize: 10
                    }
                    MouseArea {
                        objectName: "device-" + modelData.id
                        anchors.fill: parent
                        onClicked: {
                            icue.selectDevice(modelData.id);
                            root.panelId = modelData.kind === "Headset" ? "Equalizer" : modelData.kind === "Mouse" ? "Dpi" : modelData.isReceiver ? "Connections" : "Lightings";
                            root.viewIndex = 0;
                        }
                    }
                }
            }
        }
    }
    Rectangle {
        x: 0
        y: 112
        width: 80
        height: root.height - 112
        color: "#171717"
        CueButton {
            x: 12
            y: 16
            width: 56
            height: 56
            text: "▤"
            font.pixelSize: 24
            onClicked: profilesPopup.open()
        }
        Text {
            x: 8
            y: 80
            width: 64
            text: qsTranslate("OpenICUE", "PROFILE")
            color: Theme.muted
            font.pixelSize: 9
            horizontalAlignment: Text.AlignHCenter
        }
    }
    Item {
        id: screen
        x: 80
        y: 112
        width: root.width - 80
        height: root.height - 112
        CuePanel {
            x: 16
            y: 16
            width: 256
            height: screen.height * 0.55 - 32
            title: icue.device.name || ""
            titleRightMargin: 48
            CueButton {
                objectName: "device-settings"
                x: parent.width - 26
                y: -38
                width: 26
                imageSource: "image://icons/controls/settings"
                isFlat: true
                onClicked: settings.openDevice()
            }
            Rectangle {
                y: 12
                width: parent.width
                height: 1
                color: "#292929"
            }
            Column {
                x: -16
                y: 32
                width: parent.width + 32
                spacing: 0
                Repeater {
                    model: icue.navigation
                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: 32
                        radius: 3
                        color: root.panelId === modelData.id ? Theme.selected : "transparent"
                        Text {
                            x: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            color: root.panelId === modelData.id ? Theme.text : Theme.muted
                            font.pixelSize: 13
                        }
                        MouseArea {
                            objectName: "navigation-" + modelData.id
                            anchors.fill: parent
                            onClicked: root.panelId = modelData.id
                        }
                    }
                }
                Item {
                    width: 1
                    height: 24
                }
                Text {
                    x: 16
                    y: 0
                    width: parent.width - 32
                    visible: root.panelId === "Lightings"
                    text: icue.isHardwareLightingActive ? qsTranslate("OpenICUE", "Beleuchtung wird live aktualisiert") : icue.device.isConnected ? qsTranslate("OpenICUE", "Lokale Beleuchtungsvorschau") : qsTranslate("OpenICUE", "Profil bearbeiten · Gerät nicht per USB erkannt")
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    font.pixelSize: 10
                }
            }
        }
        Item {
            id: deviceStage
            x: 277
            width: screen.width - x
            height: screen.height * 0.55
            Image {
                id: bodyImage
                objectName: "deviceImage"
                anchors.centerIn: parent
                width: parent.width
                height: width * (icue.device.imageHeight || 1120) / (icue.device.imageWidth || 2956)
                source: icue.device.bodies && icue.device.bodies.length ? "image://devices/" + icue.selectedModelId + "/" + Math.min(root.viewIndex, icue.device.bodies.length - 1) : ""
                fillMode: Image.PreserveAspectFit
                DeviceSelection {
                    id: selection
                    objectName: "deviceSelection"
                    anchors.fill: parent
                    visible: root.panelId === "Assignments" || root.panelId === "Lightings"
                    entries: root.panelId === "Lightings" ? icue.lightingKeys : icue.keys.filter(k => !k.id.startsWith("Gesture_"))
                    canvasSize: Qt.size(icue.device.imageWidth || 2956, icue.device.imageHeight || 1120)

                    viewIndex: root.viewIndex
                    isMultiSelection: root.panelId === "Lightings"
                    selectedIds: isMultiSelection ? root.lightingSelectionIds : [root.selectedKey]
                    previewLayers: root.panelId === "Lightings" ? icue.document.lightingLayers || [] : []
                    frameColors: root.panelId === "Lightings" && icue.lightingState.modelId === icue.selectedModelId ? icue.hardwarePreviewColors : ({})
                    onSelectionRequested: ids => {
                        if (isMultiSelection && panels.item)
                            panels.item.selectZones(ids);
                        else if (ids.length)
                            root.selectedKey = ids[0];
                    }
                }
            }
            Column {
                x: 24
                y: 18
                spacing: 5
                visible: root.panelId === "Lightings"
                Text {
                    text: qsTranslate("OpenICUE", "Lokale Beleuchtungsvorschau")
                    color: Theme.text
                    font.pixelSize: 12
                }
                Text {
                    text: qsTranslate("OpenICUE", "Klicken · Shift: mehrere · Ziehen: Bereich auswählen")
                    color: Theme.muted
                    font.pixelSize: 11
                }
            }
            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                anchors.horizontalCenter: parent.horizontalCenter
                text: selection.hoveredLabel
                color: Theme.text
                font.pixelSize: 12
            }
            Column {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16
                Repeater {
                    model: icue.device.bodies || []
                    Image {
                        required property string modelData
                        required property int index
                        width: 64
                        height: 48
                        visible: root.viewIndex !== index
                        source: "image://devices/" + icue.selectedModelId + "/" + index + "/thumbnail"
                        fillMode: Image.PreserveAspectFit
                        opacity: 0.7
                        MouseArea {
                            objectName: "device-view-" + index
                            anchors.fill: parent
                            onClicked: root.viewIndex = index
                        }
                    }
                }
            }
        }
        Loader {
            id: panels
            objectName: "devicePanels"
            x: 16
            y: screen.height * 0.55
            width: screen.width - 32
            height: screen.height * 0.45 - 16
            source: root.panelId === "Equalizer" ? "EqualizerPanels.qml" : root.panelId === "Dpi" ? "DpiPanels.qml" : root.panelId === "Gestures" ? "GesturePanels.qml" : root.panelId === "Lightings" ? "LightingPanels.qml" : root.panelId === "Assignments" ? "AssignmentPanels.qml" : root.panelId === "Connections" ? "ConnectionPanels.qml" : "DeviceDetails.qml"
            onLoaded: {
                if (item.hasOwnProperty("selectedKey")) {
                    item.selectedKey = Qt.binding(function () {
                        return root.selectedKey;
                    });
                    item.keySelectionRequested.connect(function (key) {
                        root.selectAssignmentKey(key);
                    });
                }
                if (item.hasOwnProperty("pendingSelection")) {
                    item.pendingSelection = Qt.binding(function () {
                        return root.pendingLightingSelection;
                    });
                    item.pendingSelectionRequested.connect(function (ids) {
                        root.pendingLightingSelection = ids;
                    });
                }
                if (item.hasOwnProperty("panelId"))
                    item.panelId = Qt.binding(function () {
                        return root.panelId;
                    });
            }
        }
    }
    DeviceSettings {
        id: settings
        anchors.centerIn: parent
    }
    Rectangle {
        visible: noticeTimer.running && icue.notice.length > 0
        color: "#eb303030"
        radius: 4
        anchors.horizontalCenter: parent.horizontalCenter
        y: 120
        width: Math.min(root.width - 200, 760)
        height: toast.implicitHeight + 24
        z: 10
        Text {
            id: toast
            anchors.fill: parent
            anchors.margins: 12
            text: icue.notice
            color: Theme.text
            wrapMode: Text.WordWrap
            font.pixelSize: 12
        }
    }
}
