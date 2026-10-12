import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Software-profile list: add below the list, operations on the selected row.
Popup {
    id: sidebar
    objectName: "profileSidebar"
    width: 320
    modal: false
    padding: 8
    property string contextProfileId: ""
    property string contextProfileName: ""
    signal createRequested()
    signal importRequested()
    signal renameRequested(string profileId, string profileName)
    background: Rectangle {
        color: Theme.panel
        border.color: "#333333"
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 4
        RowLayout {
            Layout.preferredHeight: 48
            Text {
                text: qsTranslate("OpenICUE", "PROFILE")
                color: Theme.text
                Layout.fillWidth: true
                Layout.leftMargin: 8
            }
            CueButton {
                objectName: "profile-import"
                text: "↓"
                Accessible.name: qsTranslate("OpenICUE", "Importieren")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: sidebar.importRequested()
            }
            CueButton {
                objectName: "profile-collapse"
                text: "‹"
                onClicked: sidebar.close()
            }
        }
        ListView {
            id: profileList
            objectName: "profile-list"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, sidebar.availableHeight - 116)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: icue.profiles
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                objectName: "profile-row-" + modelData.id
                width: ListView.view.width
                height: 56
                radius: 4
                color: modelData.id === icue.activeProfileId ? Theme.selected : profileMouse.containsMouse ? Theme.raised : "transparent"
                Text {
                    anchors.fill: parent
                    anchors.margins: 12
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: modelData.name
                    color: Theme.text
                }
                MouseArea {
                    id: profileMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        sidebar.contextProfileId = modelData.id;
                        sidebar.contextProfileName = modelData.name;
                        icue.selectProfile(modelData.id);
                        if (mouse.button === Qt.RightButton)
                            profileMenu.popup(profileMouse, mouse.x, mouse.y);
                    }
                }
            }
        }
        CueButton {
            objectName: "profile-add"
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            text: "+   " + qsTranslate("OpenICUE", "Neues Profil")
            isAccent: true
            onClicked: sidebar.createRequested()
        }
        Item { Layout.fillHeight: true }
    }
    Menu {
        id: profileMenu
        objectName: "profile-context-menu"
        MenuItem {
            objectName: "profile-rename"
            text: qsTranslate("OpenICUE", "Umbenennen")
            onTriggered: sidebar.renameRequested(sidebar.contextProfileId, sidebar.contextProfileName)
        }
        MenuItem {
            objectName: "profile-duplicate"
            text: qsTranslate("OpenICUE", "Duplizieren")
            onTriggered: icue.duplicateProfile(sidebar.contextProfileId, qsTranslate("OpenICUE", "%1 — Kopie").arg(sidebar.contextProfileName))
        }
        MenuSeparator {}
        MenuItem {
            objectName: "profile-delete"
            text: qsTranslate("OpenICUE", "Löschen")
            enabled: icue.profiles.length > 1
            onTriggered: icue.removeProfile(sidebar.contextProfileId)
        }
    }
}
