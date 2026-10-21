import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Shown whenever the engine asks before running a tool.
Popup {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 460
    modal: true
    padding: 24
    closePolicy: Popup.NoAutoClose
    visible: App.permission.tool !== undefined
    Overlay.modal: Rectangle { color: Theme.overlay }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
    }
    background: Rectangle { radius: Theme.radiusLg; color: Theme.elevated; border.width: 1; border.color: Theme.border }

    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            spacing: 12
            Rectangle {
                width: 38; height: 38; radius: 10
                color: Theme.accentSoft
                Icon { anchors.centerIn: parent; name: "shield"; size: 20; color: Theme.accent }
            }
            ColumnLayout {
                spacing: 2
                Text {
                    text: "İzin gerekiyor"
                    font.family: Theme.font
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    text: "Orbital <b>" + (App.permission.tool || "") + "</b> aracını çalıştırmak istiyor."
                    textFormat: Text.StyledText
                    font.family: Theme.font
                    font.pixelSize: Theme.fontUi
                    color: Theme.textMuted
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            visible: (App.permission.detail || "").length > 0
            implicitHeight: Math.min(detail.implicitHeight, 200)
            radius: Theme.radiusSm
            color: Theme.codeBg
            border.width: 1
            border.color: Theme.border
            clip: true
            TextEdit {
                id: detail
                width: parent.width
                text: App.permission.detail || ""
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: Theme.mono
                font.pixelSize: 13
                color: Theme.text
                padding: 12
            }
        }
        RowLayout {
            Layout.topMargin: 8
            Layout.fillWidth: true
            spacing: 8
            OButton { text: "Reddet"; variant: "danger"; onClicked: App.replyPermission("reject") }
            Item { Layout.fillWidth: true }
            OButton { text: "Bu oturumda hep"; variant: "secondary"; onClicked: App.replyPermission("always") }
            OButton { text: "İzin ver"; variant: "primary"; onClicked: App.replyPermission("once"); focus: true }
        }
    }
}
