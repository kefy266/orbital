import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Orbital

Rectangle {
    id: root
    color: Theme.sidebar
    signal openSettings(string page)
    signal openStore()
    signal collapse()

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    FolderDialog {
        id: folderDialog
        title: "Proje klasörü seç"
        onAccepted: App.setProject(selectedFolder)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        anchors.rightMargin: 11
        spacing: 6

        // Brand + actions
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.topMargin: 4
            spacing: 8
            Icon { name: "logo"; size: 24 }
            Text {
                text: "Orbital"
                font.family: Theme.font
                font.pixelSize: Theme.fontTitle - 1
                font.weight: Font.DemiBold
                color: Theme.text
                Layout.fillWidth: true
            }
            IconButton { iconName: "sidebar"; tip: "Kenar çubuğunu kapat"; onClicked: root.collapse() }
            IconButton { iconName: "edit"; tip: "Yeni sohbet (Ctrl+N)"; onClicked: App.newChat() }
        }

        // Project
        AbstractButton {
            id: project
            Layout.fillWidth: true
            Layout.topMargin: 6
            implicitHeight: 48
            hoverEnabled: true
            onClicked: folderDialog.open()
            ToolTip.visible: hovered
            ToolTip.delay: 700
            ToolTip.text: App.projectPath
            background: Rectangle {
                radius: Theme.radius
                color: project.hovered ? Theme.hover : "transparent"
                border.width: 1
                border.color: Theme.border
            }
            contentItem: RowLayout {
                spacing: 10
                Rectangle {
                    Layout.leftMargin: 8
                    width: 30
                    height: 30
                    radius: 8
                    color: Theme.accentSoft
                    Icon { anchors.centerIn: parent; name: "folder"; size: 16; color: Theme.accent }
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Text {
                        text: App.projectName
                        font.family: Theme.font
                        font.pixelSize: Theme.fontUi
                        font.weight: Font.Medium
                        color: Theme.text
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "Proje klasörü"
                        font.family: Theme.font
                        font.pixelSize: Theme.fontCaption
                        color: Theme.textFaint
                    }
                }
                Icon { Layout.rightMargin: 10; name: "chevron-down"; size: 14; color: Theme.textFaint }
            }
        }

        OTextField {
            id: search
            Layout.fillWidth: true
            Layout.topMargin: 4
            implicitHeight: 34
            iconName: "search"
            placeholderText: "Sohbetlerde ara"
            font.pixelSize: Theme.fontSmall
            background: Rectangle {
                radius: Theme.radiusSm + 2
                color: search.activeFocus ? Theme.surface : "transparent"
                border.width: search.activeFocus ? 2 : 0
                border.color: Theme.accent
                Icon { name: "search"; color: Theme.textFaint; size: 15; x: 12; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // Sessions
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 2
            clip: true
            spacing: 1
            model: App.sessions
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            section.property: "section"
            section.delegate: Text {
                required property string section
                text: section
                width: ListView.view.width
                topPadding: 14
                bottomPadding: 6
                leftPadding: 10
                font.family: Theme.font
                font.pixelSize: Theme.fontCaption
                font.weight: Font.DemiBold
                color: Theme.textFaint
            }

            delegate: AbstractButton {
                id: row
                required property string sessionId
                required property string title
                required property string time
                readonly property bool current: sessionId === App.sessionId
                readonly property bool matches: search.text.length === 0
                                                || title.toLowerCase().indexOf(search.text.toLowerCase()) >= 0
                width: ListView.view.width
                height: matches ? 36 : 0
                visible: matches
                hoverEnabled: true
                onClicked: App.openSession(sessionId)
                ToolTip.visible: hovered && titleText.truncated
                ToolTip.delay: 800
                ToolTip.text: title

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: row.current ? Theme.selected : (row.hovered ? Theme.hover : "transparent")
                }
                contentItem: RowLayout {
                    spacing: 4
                    Text {
                        id: titleText
                        text: row.title
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        font.family: Theme.font
                        font.pixelSize: Theme.fontUi
                        color: row.current ? Theme.text : Theme.textMuted
                        elide: Text.ElideRight
                    }
                    IconButton {
                        visible: row.hovered || row.current || menu.visible
                        implicitWidth: 26
                        implicitHeight: 26
                        Layout.rightMargin: 4
                        iconName: "more"
                        iconSize: 16
                        tip: "Seçenekler"
                        onClicked: menu.popup()
                        OMenu {
                            id: menu
                            OMenuItem {
                                text: "Sohbeti sil"
                                iconName: "trash"
                                danger: true
                                onTriggered: confirmDelete.ask(row.sessionId, row.title)
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: App.engineReady ? "Henüz sohbet yok" : "Hazırlanıyor…"
                font.family: Theme.font
                font.pixelSize: Theme.fontSmall
                color: Theme.textFaint
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        component NavRow: AbstractButton {
            id: nav
            property string iconName
            property string badge: ""
            property bool hasDot: false
            Layout.fillWidth: true
            implicitHeight: 36
            hoverEnabled: true
            background: Rectangle { radius: Theme.radiusSm; color: nav.hovered ? Theme.hover : "transparent" }
            contentItem: RowLayout {
                spacing: 10
                Icon { Layout.leftMargin: 10; name: nav.iconName; size: 17; color: Theme.textMuted }
                Text {
                    text: nav.text
                    Layout.fillWidth: true
                    font.family: Theme.font
                    font.pixelSize: Theme.fontUi
                    color: Theme.text
                }
                Rectangle {
                    visible: nav.hasDot
                    Layout.rightMargin: 10
                    width: 7; height: 7; radius: 3.5
                    color: Theme.accent
                }
            }
        }
        NavRow { text: "Mağaza"; iconName: "store"; onClicked: root.openStore() }
        NavRow { text: "Skill'ler"; iconName: "sparkle"; onClicked: root.openSettings("skills") }
        NavRow { text: "Eklentiler"; iconName: "blocks"; onClicked: root.openSettings("plugins") }
        NavRow { text: "Ayarlar"; iconName: "settings"; hasDot: App.updateAvailable; onClicked: root.openSettings("general") }
    }

    ConfirmDialog {
        id: confirmDelete
        property string targetId
        function ask(id, title) {
            targetId = id
            message = "“" + title + "” kalıcı olarak silinecek."
            open()
        }
        heading: "Sohbet silinsin mi?"
        confirmText: "Sil"
        onConfirmed: App.deleteSession(targetId)
    }
}
