import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Installs skills straight from a GitHub repository or folder URL.
Popup {
    id: dialog
    property bool busy: false
    property string message
    property bool failed: false
    signal installed()

    function openDialog() {
        input.text = ""
        message = ""
        busy = false
        open()
        input.forceActiveFocus()
    }
    function install() {
        if (busy || input.text.trim().length === 0)
            return
        busy = true
        message = ""
        App.installSkillFromGitHub(input.text)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(500, parent.width - 48)
    modal: true
    padding: 24
    closePolicy: busy ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
    Overlay.modal: Rectangle { color: Theme.overlay }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.fast } }
    background: Rectangle { radius: Theme.radiusLg; color: Theme.elevated; border.width: 1; border.color: Theme.border }

    Connections {
        target: App
        enabled: dialog.visible
        function onGithubSkillDone(ok, message) {
            dialog.busy = false
            dialog.failed = !ok
            dialog.message = message
            if (ok)
                dialog.installed()
        }
    }

    contentItem: ColumnLayout {
        spacing: 12
        Text {
            text: "GitHub'dan skill yükle"
            font.family: Theme.font
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "Bir deponun ya da klasörün adresini yapıştır. İçindeki SKILL.md bulunan her klasör skill olarak yüklenir; aynı adlı skill varsa güncellenir."
            font.family: Theme.font
            font.pixelSize: Theme.fontUi
            color: Theme.textMuted
        }
        OTextField {
            id: input
            Layout.fillWidth: true
            enabled: !dialog.busy
            placeholderText: "github.com/kullanıcı/depo/tree/main/skills/ornek"
            onAccepted: dialog.install()
        }
        RowLayout {
            visible: dialog.busy || dialog.message.length > 0
            Layout.fillWidth: true
            spacing: 8
            Spinner { visible: dialog.busy; implicitWidth: 16; implicitHeight: 16 }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: dialog.busy ? "İndiriliyor…" : dialog.message
                font.family: Theme.font
                font.pixelSize: Theme.fontUi
                color: dialog.busy ? Theme.textMuted : dialog.failed ? Theme.danger : Theme.success
            }
        }
        RowLayout {
            Layout.topMargin: 6
            Layout.alignment: Qt.AlignRight
            spacing: 8
            OButton { text: dialog.message.length > 0 && !dialog.failed ? "Kapat" : "Vazgeç"; variant: "ghost"; enabled: !dialog.busy; onClicked: dialog.close() }
            OButton {
                text: "Yükle"
                iconName: "download"
                variant: "primary"
                enabled: !dialog.busy && input.text.trim().length > 0
                onClicked: dialog.install()
            }
        }
    }
}
