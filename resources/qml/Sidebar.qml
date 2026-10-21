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

        // Project selector
        AbstractButton {
            id: project
            Layout.fillWidth: true
            Layout.topMargin: 4
            implicitHeight: 48
            hoverEnabled: true
            onClicked: projectPopup.opened ? projectPopup.close() : projectPopup.open()
            ToolTip.visible: hovered && !projectPopup.opened
            ToolTip.delay: 700
            ToolTip.text: App.projectPath
            background: Rectangle {
                radius: Theme.radius
                color: project.hovered || projectPopup.opened ? Theme.hover : "transparent"
                border.width: 1
                border.color: projectPopup.opened ? Theme.accent : Theme.border
            }
            contentItem: RowLayout {
                spacing: 10
                Rectangle {
                    Layout.leftMargin: 8
                    width: 30
                    height: 30
                    radius: 8
                    color: App.isNoFolder ? Theme.accentSoft : Theme.hover
                    Icon {
                        anchors.centerIn: parent
                        name: App.isNoFolder ? "sparkle" : "folder"
                        size: 16
                        color: App.isNoFolder ? Theme.accent : Theme.text
                    }
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
                        text: App.isNoFolder ? "Klasörsüz mod" : "Proje klasörü"
                        font.family: Theme.font
                        font.pixelSize: Theme.fontCaption
                        color: Theme.textFaint
                    }
                }
                Icon {
                    Layout.rightMargin: 10
                    name: "chevron-down"
                    size: 14
                    color: Theme.textFaint
                    rotation: projectPopup.opened ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: Theme.fast } }
                }
            }

            Popup {
                id: projectPopup
                y: project.height + 4
                x: 0
                width: project.width
                implicitHeight: Math.min(380, projectLayout.implicitHeight + 16)
                padding: 6
                modal: true
                focus: true
                enter: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fast }
                    NumberAnimation { property: "y"; from: project.height - 4; to: project.height + 4; duration: Theme.fast }
                }
                exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.fast } }
                background: Rectangle {
                    radius: Theme.radius
                    color: Theme.elevated
                    border.width: 1
                    border.color: Theme.border
                }

                contentItem: ColumnLayout {
                    id: projectLayout
                    spacing: 3

                    // No-folder quick action
                    AbstractButton {
                        id: noFolderBtn
                        Layout.fillWidth: true
                        implicitHeight: 38
                        hoverEnabled: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: noFolderBtn.hovered ? Theme.hover : "transparent"
                        }
                        onClicked: {
                            projectPopup.close()
                            App.openNoFolderProject()
                        }
                        contentItem: RowLayout {
                            spacing: 8
                            Rectangle {
                                Layout.leftMargin: 6
                                width: 24; height: 24; radius: 6
                                color: Theme.accentSoft
                                Icon { anchors.centerIn: parent; name: "sparkle"; size: 13; color: Theme.accent }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text {
                                    text: "Klasörsüz Oturum"
                                    font.family: Theme.font
                                    font.pixelSize: Theme.fontSmall
                                    font.weight: Font.DemiBold
                                    color: Theme.text
                                }
                                Text {
                                    text: "/orbital/no-folder"
                                    font.family: Theme.mono
                                    font.pixelSize: Theme.fontCaption - 1
                                    color: Theme.textFaint
                                }
                            }
                            Icon {
                                visible: App.isNoFolder
                                name: "check"
                                size: 14
                                color: Theme.accent
                                Layout.rightMargin: 6
                            }
                        }
                    }

                    // Open other folder
                    AbstractButton {
                        id: openFolderBtn
                        Layout.fillWidth: true
                        implicitHeight: 34
                        hoverEnabled: true
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: openFolderBtn.hovered ? Theme.hover : "transparent"
                        }
                        onClicked: {
                            projectPopup.close()
                            folderDialog.open()
                        }
                        contentItem: RowLayout {
                            spacing: 8
                            Rectangle {
                                Layout.leftMargin: 6
                                width: 24; height: 24; radius: 6
                                color: Theme.hover
                                Icon { anchors.centerIn: parent; name: "plus"; size: 13; color: Theme.textMuted }
                            }
                            Text {
                                text: "Yeni Klasör Aç…"
                                Layout.fillWidth: true
                                font.family: Theme.font
                                font.pixelSize: Theme.fontSmall
                                color: Theme.text
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 3
                        Layout.bottomMargin: 3
                        height: 1
                        color: Theme.border
                    }

                    Text {
                        text: "PROJELER"
                        font.family: Theme.font
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 0.5
                        color: Theme.textFaint
                        Layout.leftMargin: 8
                    }

                    ListView {
                        id: projList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(200, contentHeight)
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        model: App.projects
                        delegate: AbstractButton {
                            id: pRow
                            required property var modelData
                            width: projList.width
                            implicitHeight: 38
                            hoverEnabled: true
                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: pRow.modelData.isCurrent ? Theme.selected : (pRow.hovered ? Theme.hover : "transparent")
                            }
                            onClicked: {
                                projectPopup.close()
                                App.openProjectPath(pRow.modelData.path)
                            }
                            contentItem: RowLayout {
                                spacing: 8
                                Icon {
                                    Layout.leftMargin: 6
                                    name: pRow.modelData.isNoFolder ? "sparkle" : "folder"
                                    size: 14
                                    color: pRow.modelData.isCurrent ? Theme.accent : Theme.textMuted
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Text {
                                        text: pRow.modelData.name
                                        font.family: Theme.font
                                        font.pixelSize: Theme.fontSmall
                                        font.weight: pRow.modelData.isCurrent ? Font.DemiBold : Font.Normal
                                        color: Theme.text
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: pRow.modelData.path
                                        font.family: Theme.font
                                        font.pixelSize: Theme.fontCaption - 1
                                        color: Theme.textFaint
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }
                                Rectangle {
                                    visible: pRow.modelData.sessionCount > 0
                                    implicitHeight: 18
                                    implicitWidth: scText.implicitWidth + 8
                                    radius: 9
                                    color: Theme.hover
                                    Text {
                                        id: scText
                                        anchors.centerIn: parent
                                        text: pRow.modelData.sessionCount
                                        font.family: Theme.font
                                        font.pixelSize: 10
                                        color: Theme.textMuted
                                    }
                                }
                                IconButton {
                                    visible: !pRow.modelData.isCurrent && !pRow.modelData.isNoFolder && pRow.hovered
                                    iconName: "x"
                                    iconSize: 12
                                    tip: "Listeden kaldır"
                                    Layout.rightMargin: 4
                                    onClicked: App.removeRecentProject(pRow.modelData.path)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Project filter tabs (Bu Proje vs Tüm Sohbetler)
        Segmented {
            Layout.fillWidth: true
            Layout.topMargin: 2
            value: App.projectFilter
            model: [
                { value: "current", label: "Bu Proje", icon: "folder" },
                { value: "all", label: "Tüm Sohbetler", icon: "list" }
            ]
            onPicked: val => App.setProjectFilter(val)
        }

        OTextField {
            id: search
            Layout.fillWidth: true
            Layout.topMargin: 2
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
                required property string projectName
                required property string directory
                readonly property bool current: sessionId === App.sessionId
                readonly property bool matches: search.text.length === 0
                                                || title.toLowerCase().indexOf(search.text.toLowerCase()) >= 0
                                                || projectName.toLowerCase().indexOf(search.text.toLowerCase()) >= 0
                width: ListView.view.width
                height: matches ? (App.projectFilter === "all" && projectName.length > 0 ? 44 : 36) : 0
                visible: matches
                hoverEnabled: true
                onClicked: App.openSession(sessionId)
                ToolTip.visible: hovered && titleText.truncated
                ToolTip.delay: 800
                ToolTip.text: title + (projectName.length > 0 ? " [" + projectName + "]" : "")

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: row.current ? Theme.selected : (row.hovered ? Theme.hover : "transparent")
                }
                contentItem: RowLayout {
                    spacing: 4
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        spacing: 1
                        Text {
                            id: titleText
                            text: row.title
                            Layout.fillWidth: true
                            font.family: Theme.font
                            font.pixelSize: Theme.fontUi
                            color: row.current ? Theme.text : Theme.textMuted
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: App.projectFilter === "all" && row.projectName.length > 0
                            text: row.projectName
                            Layout.fillWidth: true
                            font.family: Theme.font
                            font.pixelSize: Theme.fontCaption - 1
                            color: Theme.accent
                            elide: Text.ElideRight
                        }
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

        // Native Developer Sponsor Card (Blufy, EdgeConvert, Hizmet)
        SponsorCard {
            Layout.fillWidth: true
            Layout.topMargin: 2
            Layout.bottomMargin: 2
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
