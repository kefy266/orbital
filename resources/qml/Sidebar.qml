import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Orbital

// Left column: Sohbet/Kod switch, navigation, the chat list (Sohbet) or the
// project tree with the open project's chats (Kod), and the account row.
Rectangle {
    id: root
    color: Theme.sidebar
    signal openSettings(string page)
    signal openStore()
    signal collapse()

    readonly property bool codeMode: !App.isNoFolder
    readonly property var codeProjects: App.projects.filter(p => !p.isNoFolder)
    property bool searching: false

    function matches(title, project) {
        const q = search.text.trim().toLowerCase()
        return q.length === 0 || title.toLowerCase().indexOf(q) >= 0 || project.toLowerCase().indexOf(q) >= 0
    }
    function openCode() {
        if (root.codeProjects.length > 0)
            App.openProjectPath(root.codeProjects[0].path)
        else
            folderDialog.open()
    }

    Component.onCompleted: App.projectFilter = "current"

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    FolderDialog {
        id: folderDialog
        title: "Proje klasörü seç"
        // Without an explicit window the non-native fallback dialog never shows.
        parentWindow: root.Window.window
        onAccepted: App.setProject(selectedFolder)
    }

    component NavRow: AbstractButton {
        id: nav
        property string iconName
        property bool active: false
        Layout.fillWidth: true
        implicitHeight: 34
        hoverEnabled: true
        background: Rectangle {
            radius: Theme.radiusSm
            color: nav.active ? Theme.selected : nav.hovered ? Theme.hover : "transparent"
        }
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
        }
    }

    component SessionRow: AbstractButton {
        id: row
        required property string sessionId
        required property string title
        required property string projectName
        property int indent: 0
        readonly property bool current: sessionId === App.sessionId
        width: parent ? parent.width : 0
        implicitHeight: 34
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
                Layout.leftMargin: 10 + row.indent
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
                        text: "Yeniden adlandır"
                        iconName: "pencil"
                        onTriggered: renameDialog.ask(row.sessionId, row.title)
                    }
                    OMenuItem {
                        text: "Markdown olarak dışa aktar"
                        iconName: "file"
                        onTriggered: App.exportChatAsMarkdown()
                    }
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        anchors.rightMargin: 11
        spacing: 2

        // Brand, collapse, Sohbet/Kod
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.topMargin: 4
            Layout.bottomMargin: 8
            spacing: 8
            Icon { name: "logo"; size: 22 }
            Text {
                text: "Orbital"
                font.family: Theme.font
                font.pixelSize: Theme.fontTitle - 1
                font.weight: Font.DemiBold
                color: Theme.text
                Layout.fillWidth: true
            }
            Segmented {
                implicitHeight: 30
                model: [{ value: "chat", label: "", icon: "chat" }, { value: "code", label: "", icon: "code" }]
                value: root.codeMode ? "code" : "chat"
                onPicked: v => {
                    if (v === "chat" && root.codeMode)
                        App.openNoFolderProject()
                    else if (v === "code" && !root.codeMode)
                        root.openCode()
                }
                ToolTip.visible: hovered
                property bool hovered: false
                HoverHandler { onHoveredChanged: parent.hovered = hovered }
                ToolTip.delay: 600
                ToolTip.text: "Sohbet: klasörsüz sohbet · Kod: proje klasöründe çalışan ajan"
            }
            IconButton { iconName: "sidebar"; tip: "Kenar çubuğunu kapat (Ctrl+B)"; onClicked: root.collapse() }
        }

        NavRow { text: "Yeni sohbet"; iconName: "plus"; onClicked: App.newChat() }
        NavRow {
            text: "Ara"
            iconName: "search"
            active: root.searching
            onClicked: {
                root.searching = !root.searching
                if (root.searching)
                    search.forceActiveFocus()
                else
                    search.text = ""
            }
        }
        OTextField {
            id: search
            visible: root.searching
            Layout.fillWidth: true
            Layout.topMargin: 2
            Layout.bottomMargin: 2
            implicitHeight: 34
            placeholderText: "Sohbetlerde ara"
            font.pixelSize: Theme.fontSmall
            Keys.onEscapePressed: { text = ""; root.searching = false }
        }
        NavRow { text: "Mağaza"; iconName: "store"; onClicked: root.openStore() }
        NavRow { text: "Skill'ler"; iconName: "sparkle"; onClicked: root.openSettings("skills") }
        NavRow { text: "Eklentiler"; iconName: "blocks"; onClicked: root.openSettings("plugins") }

        // Chats (Sohbet) or projects with their chats (Kod)
        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 10
            clip: true
            contentHeight: listColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: listColumn
                width: flick.width
                spacing: 1

                Text {
                    text: root.codeMode ? "Projeler" : "Sohbetler"
                    leftPadding: 10
                    bottomPadding: 6
                    font.family: Theme.font
                    font.pixelSize: Theme.fontCaption
                    font.weight: Font.DemiBold
                    color: Theme.textFaint
                }

                // Sohbet: the no-folder chats
                Repeater {
                    id: chatRepeater
                    model: root.codeMode ? null : App.sessions
                    delegate: SessionRow {
                        visible: root.matches(title, projectName)
                        height: visible ? implicitHeight : 0
                        width: listColumn.width
                    }
                }

                // Kod: every project; the open one lists its chats underneath
                Repeater {
                    model: root.codeMode ? root.codeProjects : []
                    delegate: Column {
                        id: proj
                        required property var modelData
                        readonly property bool open: modelData.isCurrent
                        width: listColumn.width
                        spacing: 1

                        AbstractButton {
                            id: projRow
                            width: parent.width
                            implicitHeight: 34
                            hoverEnabled: true
                            onClicked: if (!proj.open) App.openProjectPath(proj.modelData.path)
                            ToolTip.visible: hovered
                            ToolTip.delay: 800
                            ToolTip.text: proj.modelData.path
                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: projRow.hovered ? Theme.hover : "transparent"
                            }
                            contentItem: RowLayout {
                                spacing: 8
                                Icon {
                                    Layout.leftMargin: 10
                                    name: proj.open ? "chevron-down" : "chevron-right"
                                    size: 13
                                    color: Theme.textFaint
                                }
                                Icon { name: "folder"; size: 15; color: proj.open ? Theme.accent : Theme.textMuted }
                                Text {
                                    text: proj.modelData.name
                                    Layout.fillWidth: true
                                    font.family: Theme.font
                                    font.pixelSize: Theme.fontUi
                                    font.weight: proj.open ? Font.DemiBold : Font.Normal
                                    color: Theme.text
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: !projRow.hovered && proj.modelData.sessionCount > 0
                                    text: proj.modelData.sessionCount
                                    Layout.rightMargin: 10
                                    font.family: Theme.font
                                    font.pixelSize: Theme.fontCaption
                                    color: Theme.textFaint
                                }
                                IconButton {
                                    visible: projRow.hovered && proj.open
                                    implicitWidth: 24
                                    implicitHeight: 24
                                    iconName: "plus"
                                    iconSize: 13
                                    tip: "Bu projede yeni sohbet"
                                    onClicked: App.newChat()
                                }
                                IconButton {
                                    visible: projRow.hovered && !proj.open
                                    implicitWidth: 24
                                    implicitHeight: 24
                                    Layout.rightMargin: 4
                                    iconName: "x"
                                    iconSize: 12
                                    tip: "Listeden kaldır"
                                    onClicked: App.removeRecentProject(proj.modelData.path)
                                }
                            }
                        }

                        Repeater {
                            model: proj.open ? App.sessions : null
                            delegate: SessionRow {
                                indent: 22
                                visible: root.matches(title, projectName)
                                height: visible ? implicitHeight : 0
                                width: proj.width
                            }
                        }
                    }
                }

                NavRow {
                    visible: root.codeMode
                    width: listColumn.width
                    text: "Klasör aç…"
                    iconName: "folder"
                    onClicked: folderDialog.open()
                }

                Text {
                    visible: !root.codeMode && chatRepeater.count === 0
                    width: listColumn.width
                    topPadding: 24
                    horizontalAlignment: Text.AlignHCenter
                    text: App.engineReady ? "Henüz sohbet yok" : "Hazırlanıyor…"
                    font.family: Theme.font
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textFaint
                }
            }
        }

        // Native Developer Sponsor Card (Blufy, EdgeConvert, Hizmet)
        SponsorCard {
            Layout.fillWidth: true
            Layout.topMargin: 6
            Layout.bottomMargin: 4
        }

        // Account + settings
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 4
            AbstractButton {
                id: account
                Layout.fillWidth: true
                implicitHeight: 42
                hoverEnabled: true
                enabled: !App.accountBusy
                onClicked: App.accountSignedIn ? root.openSettings("general") : App.accountSignIn()
                background: Rectangle { radius: Theme.radiusSm; color: account.hovered ? Theme.hover : "transparent" }
                contentItem: RowLayout {
                    spacing: 10
                    Rectangle {
                        Layout.leftMargin: 6
                        width: 28
                        height: 28
                        radius: 14
                        color: App.accountSignedIn ? Theme.accent : Theme.hover
                        Text {
                            anchors.centerIn: parent
                            visible: App.accountSignedIn
                            text: (App.accountName || App.accountEmail || "?").charAt(0).toUpperCase()
                            font.family: Theme.font
                            font.pixelSize: Theme.fontSmall
                            font.weight: Font.DemiBold
                            color: "white"
                        }
                        Icon { anchors.centerIn: parent; visible: !App.accountSignedIn; name: "user"; size: 15; color: Theme.textMuted }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            text: App.accountSignedIn ? (App.accountName || App.accountEmail)
                                                      : App.accountBusy ? "Tarayıcıda girişi tamamla…" : "EdgeAccount ile giriş yap"
                            elide: Text.ElideRight
                            font.family: Theme.font
                            font.pixelSize: Theme.fontUi
                            color: Theme.text
                        }
                        Text {
                            Layout.fillWidth: true
                            text: App.accountSignedIn ? App.accountEmail : "Origin Edge hesabı"
                            elide: Text.ElideRight
                            font.family: Theme.font
                            font.pixelSize: Theme.fontCaption
                            color: Theme.textFaint
                        }
                    }
                }
            }
            IconButton {
                iconName: "settings"
                tip: "Ayarlar (Ctrl+,)"
                onClicked: root.openSettings("general")
                Rectangle {
                    visible: App.updateAvailable
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 5
                    width: 7; height: 7; radius: 3.5
                    color: Theme.accent
                }
            }
        }
    }

    RenameDialog {
        id: renameDialog
        onRenamed: newTitle => App.renameSession(targetId, newTitle)
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
