import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    property var sourceOptions: []
    model: sourceOptions
    displayText: currentIndex >= 0 ? qsTranslate("OpenICUE", sourceOptions[currentIndex]) : ""
    delegate: ItemDelegate {
        required property string modelData
        required property int index
        width: root.width
        text: qsTranslate("OpenICUE", modelData)
        highlighted: root.highlightedIndex === index
    }
}
