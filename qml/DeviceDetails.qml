import QtQuick
import QtQuick.Layouts
import "."

RowLayout {
    property string panelId: ""
    spacing: 8
    CuePanel {
        Layout.preferredWidth: panelId === "SurfaceCalibration" ? 520 : 688
        Layout.fillHeight: true
        title: panelId === "SurfaceCalibration" ? qsTranslate("OpenICUE",
                                                              "Oberflächenkalibrierung") : panelId
                                                  === "Connections" ? qsTranslate("OpenICUE",
                                                                                  "Verbindungen") :
                                                                      qsTranslate("OpenICUE",
                                                                                  panelId)
        Column {
            width: parent.width
            spacing: 18
            Text {
                width: parent.width
                text: panelId === "SurfaceCalibration" ? qsTranslate("OpenICUE",
                                                                     "Die Oberflächenkalibrierung ist noch nicht verfügbar.") :
                                                         panelId === "Connections" ? qsTranslate(
                                                                                         "OpenICUE",
                                                                                         "USB-Empfänger und Funkverbindung werden getrennt geprüft.") :
                                                                                     qsTranslate(
                                                                                         "OpenICUE",
                                                                                         "Diese Gerätekonfiguration ist noch nicht verfügbar.")
                color: Theme.text
                wrapMode: Text.WordWrap
                font.pixelSize: 12
            }
            CueButton {
                text: qsTranslate("OpenICUE", "Start")
                enabled: false
            }
            Text {
                width: parent.width
                text: qsTranslate("OpenICUE", "Hardware-Kommandos sind noch nicht verifiziert.")
                color: Theme.muted
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }
        }
    }
    Item {
        Layout.fillWidth: true
    }
}
