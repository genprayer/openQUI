import QtQuick.Controls

DialogButtonBox {
    Button {
        text: qsTranslate("OpenICUE", "OK")
        DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
    }
    Button {
        text: qsTranslate("OpenICUE", "Abbrechen")
        DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
    }
}
