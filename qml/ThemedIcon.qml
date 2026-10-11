import QtQuick
import "."

Image {
    id: root
    property string iconSource: ""
    property bool isSelected: false
    source: iconSource.startsWith("image://icons/") ? iconSource + "?base=" + encodeURIComponent((
                                                                                                     isSelected
                                                                                                     ? Theme.iconSelectedBase :
                                                                                                       Theme.iconBase).toString(
                                                                                                     )) + "&accent="
                                                      + encodeURIComponent(Theme.iconAccent.toString(
                                                                               )) + "&secondary="
                                                      + encodeURIComponent(
                                                          Theme.iconSecondary.toString())
                                                      + "&highlight=" + encodeURIComponent(
                                                          Theme.iconHighlight.toString()) :
                                                      iconSource
    fillMode: Image.PreserveAspectFit
    sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
}
