import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

Popup {
    id: dialog
    property string targetId
    property string initialTitle
    signal renamed(string newTitle)

    function ask(id, title) {
        targetId = id
        initialTitle = title
        input.text = title
        open()
        input.forceActiveFocus()
        input.selectAll()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 420
    modal: true
    padding: 24
    Overlay.modal: Rectangle { color: Theme.overlay }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.fast } }

    background: Rectangle { radius: Theme.radiusLg; color: Theme.elevated; border.width: 1; border.color: Theme.border }

    contentItem: ColumnLayout {
        spacing: 12
        Text {
            text: "Sohbet Başlığını Düzenle"
            font.family: Theme.font
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            text: "Bu sohbet için yeni bir başlık belirleyin:"
            font.family: Theme.font
            font.pixelSize: Theme.fontUi
            color: Theme.textMuted
        }
        OTextField {
            id: input
            Layout.fillWidth: true
            placeholderText: "Sohbet başlığı"
            onAccepted: save()
        }
        RowLayout {
            Layout.topMargin: 10
            Layout.alignment: Qt.AlignRight
            spacing: 8
            OButton { text: "Vazgeç"; variant: "ghost"; onClicked: dialog.close() }
            OButton {
                text: "Kaydet"
                variant: "primary"
                enabled: input.text.trim().length > 0
                onClicked: save()
            }
        }
    }

    function save() {
        var t = input.text.trim()
        if (t.length > 0) {
            dialog.close()
            dialog.renamed(t)
        }
    }
}
