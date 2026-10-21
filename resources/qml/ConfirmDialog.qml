import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Small modal for destructive confirmations.
Popup {
    id: dialog
    property string heading
    property string message
    property string confirmText: "Onayla"
    property bool destructive: true
    signal confirmed()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 400
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
        spacing: 10
        Text {
            text: dialog.heading
            font.family: Theme.font
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            text: dialog.message
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.family: Theme.font
            font.pixelSize: Theme.fontUi
            color: Theme.textMuted
        }
        RowLayout {
            Layout.topMargin: 14
            Layout.alignment: Qt.AlignRight
            spacing: 8
            OButton { text: "Vazgeç"; variant: "ghost"; onClicked: dialog.close() }
            OButton {
                text: dialog.confirmText
                variant: dialog.destructive ? "danger" : "primary"
                onClicked: { dialog.close(); dialog.confirmed() }
            }
        }
    }
}
