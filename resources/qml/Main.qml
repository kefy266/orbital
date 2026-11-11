import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

ApplicationWindow {
    id: window
    width: 1320
    height: 860
    minimumWidth: 560
    minimumHeight: 480
    visible: true
    title: App.sessionTitle.length ? App.sessionTitle + " — Orbital" : "Orbital"
    color: Theme.bg
    font.family: Theme.font

    property bool sidebarOpen: width >= 900
    property bool panelOpen: false
    readonly property bool chatting: App.chat.count > 0 || App.sessionId.length > 0

    function openSettings(page) { settings.openAt(page || "general") }

    Shortcut { sequence: "Ctrl+N"; onActivated: { App.newChat(); composer.focusInput() } }
    Shortcut { sequence: "Ctrl+B"; onActivated: window.sidebarOpen = !window.sidebarOpen }
    Shortcut { sequence: "Ctrl+Shift+B"; onActivated: window.panelOpen = !window.panelOpen }
    Shortcut { sequence: "Ctrl+,"; onActivated: window.openSettings("general") }
    Shortcut { sequence: "Ctrl+L"; onActivated: composer.focusInput() }
    Shortcut { sequence: "Esc"; enabled: App.busy; onActivated: App.stop() }

    Connections {
        target: App
        function onToast(message) { toast.show(message) }
        function onSessionChanged() { composer.focusInput() }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: window.sidebarOpen ? Theme.sidebarWidth : 0
            visible: Layout.preferredWidth > 0
            clip: true
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
            onOpenSettings: page => window.openSettings(page)
            onOpenStore: storeDialog.openStore()
            onCollapse: window.sidebarOpen = false
        }

        // Main column
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Header
            Item {
                id: header
                width: parent.width
                height: 56
                z: 2
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 4
                    IconButton {
                        visible: !window.sidebarOpen
                        iconName: "sidebar"
                        tip: "Kenar çubuğunu aç (Ctrl+B)"
                        onClicked: window.sidebarOpen = true
                    }
                    IconButton {
                        visible: !window.sidebarOpen
                        iconName: "edit"
                        tip: "Yeni sohbet (Ctrl+N)"
                        onClicked: App.newChat()
                    }
                    // Chat title with its menu
                    AbstractButton {
                        id: titleButton
                        Layout.leftMargin: 4
                        Layout.maximumWidth: header.width - 140
                        implicitHeight: 34
                        implicitWidth: titleRow.implicitWidth + 24
                        hoverEnabled: App.sessionId.length > 0
                        enabled: App.sessionId.length > 0
                        onClicked: titleMenu.popup(titleButton, 0, titleButton.height + 4)
                        background: Rectangle {
                            radius: Theme.radiusSm + 2
                            color: titleButton.hovered || titleMenu.visible ? Theme.hover : "transparent"
                        }
                        contentItem: Row {
                            id: titleRow
                            spacing: 6
                            leftPadding: 10
                            Text {
                                text: App.sessionTitle.length ? App.sessionTitle : (App.isNoFolder ? "Yeni sohbet" : App.projectName)
                                width: Math.min(implicitWidth, header.width - 200)
                                elide: Text.ElideRight
                                font.family: Theme.font
                                font.pixelSize: Theme.fontUi + 1
                                font.weight: Font.Medium
                                color: Theme.text
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Icon {
                                visible: titleButton.enabled
                                name: "chevron-down"
                                size: 14
                                color: Theme.textFaint
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        OMenu {
                            id: titleMenu
                            OMenuItem { text: "Yeniden adlandır"; iconName: "pencil"; onTriggered: titleRename.ask(App.sessionId, App.sessionTitle) }
                            OMenuItem { text: "Markdown olarak dışa aktar"; iconName: "file"; onTriggered: App.exportChatAsMarkdown() }
                            OMenuItem { text: "Sohbeti sil"; iconName: "trash"; danger: true; onTriggered: titleDelete.open() }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    IconButton {
                        iconName: "panel-right"
                        tip: window.panelOpen ? "Paneli kapat (Ctrl+Shift+B)" : "Oturum paneli (Ctrl+Shift+B)"
                        onClicked: window.panelOpen = !window.panelOpen
                    }
                }
            }

            // Engine status banner
            Rectangle {
                id: banner
                anchors.top: header.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                z: 3
                width: Math.min(parent.width - 32, bannerRow.implicitWidth + 28)
                height: 38
                radius: 19
                visible: App.status.length > 0
                color: App.statusIsError ? Theme.dangerSoft : Theme.surface
                border.width: 1
                border.color: App.statusIsError ? Qt.alpha(Theme.danger, 0.4) : Theme.border
                RowLayout {
                    id: bannerRow
                    anchors.centerIn: parent
                    spacing: 10
                    Spinner { visible: !App.statusIsError; width: 16; height: 16 }
                    Icon { visible: App.statusIsError; name: "alert"; size: 16; color: Theme.danger }
                    Text {
                        text: App.status
                        Layout.maximumWidth: banner.parent.width - 200
                        elide: Text.ElideRight
                        font.family: Theme.font
                        font.pixelSize: Theme.fontSmall
                        color: App.statusIsError ? Theme.danger : Theme.textMuted
                    }
                    OButton {
                        visible: App.statusIsError
                        implicitHeight: 28
                        text: "Tekrar dene"
                        variant: "ghost"
                        onClicked: App.retryEngine()
                    }
                }
            }

            ChatView {
                id: chatView
                anchors.top: header.bottom
                anchors.bottom: composerArea.top
                width: parent.width
                visible: window.chatting
                onEditPromptRequested: prompt => composer.setText(prompt)
            }

            // Welcome
            ColumnLayout {
                id: welcome
                visible: !window.chatting
                opacity: visible ? 1 : 0
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: composerArea.top
                anchors.bottomMargin: 28
                width: Math.min(Theme.chatWidth, parent.width - 48)
                spacing: 14

                Icon { Layout.alignment: Qt.AlignHCenter; name: "logo"; size: 56 }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        const h = new Date().getHours()
                        return h < 6 ? "İyi geceler" : h < 12 ? "Günaydın" : h < 18 ? "İyi günler" : "İyi akşamlar"
                    }
                    font.family: Theme.font
                    font.pixelSize: Theme.fontDisplay
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: App.isNoFolder ? "Bugün ne konuşalım?" : "<b>" + App.projectName + "</b> projesinde bugün ne yapalım?"
                    textFormat: Text.StyledText
                    font.family: Theme.font
                    font.pixelSize: Theme.fontBody
                    color: Theme.textMuted
                }
            }

            Item {
                id: composerArea
                width: parent.width
                height: composer.implicitHeight + (window.chatting ? 34 : 0)
                y: window.chatting ? parent.height - height
                                   : Math.max(header.height + 260, (parent.height - height) / 2 + 40)
                Behavior on y { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }

                Composer {
                    id: composer
                    width: Math.min(Theme.chatWidth, parent.width - 48)
                    anchors.horizontalCenter: parent.horizontalCenter
                    large: !window.chatting
                }
                Text {
                    visible: window.chatting
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 9
                    text: "Orbital hata yapabilir. Önemli değişiklikleri gözden geçir."
                    font.family: Theme.font
                    font.pixelSize: 12
                    color: Theme.textFaint
                }
            }

            // Suggestions under the welcome composer
            Flow {
                visible: !window.chatting
                anchors.top: composerArea.bottom
                anchors.topMargin: 16
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(Theme.chatWidth, parent.width - 48)
                spacing: 8
                Repeater {
                    model: App.isNoFolder ? [
                        { icon: "bulb", label: "Fikir üret", prompt: "Bana bugün üzerinde çalışabileceğim 5 yaratıcı proje fikri öner, her biri için kısa bir açıklama yaz." },
                        { icon: "file", label: "Metni düzelt", prompt: "Göndereceğim metnin yazım ve anlatım hatalarını düzelt, daha akıcı hale getir. Metin:" },
                        { icon: "globe", label: "Bir konuyu anlat", prompt: "Bana yapay zekanın nasıl çalıştığını sade bir dille anlat." },
                        { icon: "terminal", label: "Kod örneği", prompt: "Python'da bir klasördeki dosyaları boyutuna göre sıralayan kısa bir betik yaz ve açıkla." }
                    ] : [
                        { icon: "search", label: "Projeyi açıkla", prompt: "Bu projenin yapısını incele: ne işe yaradığını, ana bileşenlerini ve önemli dosyalarını kısaca açıkla." },
                        { icon: "terminal", label: "Derle ve test et", prompt: "Projeyi derle ve testleri çalıştır. Hata varsa sebebini bul ve düzelt." },
                        { icon: "file", label: "README yaz", prompt: "Bu proje için kurulum, kullanım ve geliştirme adımlarını anlatan bir README.md yaz." },
                        { icon: "check", label: "Değişiklikleri incele", prompt: "/review" }
                    ]
                    delegate: AbstractButton {
                        id: chip
                        required property var modelData
                        hoverEnabled: true
                        implicitHeight: 36
                        implicitWidth: chipRow.implicitWidth + 26
                        // Prompts ending in ':' need the user's text: fill the composer instead of sending.
                        onClicked: modelData.prompt.endsWith(":") ? composer.setText(modelData.prompt + " ") : App.submit(modelData.prompt)
                        background: Rectangle {
                            radius: 18
                            color: chip.hovered ? Theme.hover : "transparent"
                            border.width: 1
                            border.color: Theme.border
                            Behavior on color { ColorAnimation { duration: Theme.fast } }
                        }
                        contentItem: Row {
                            id: chipRow
                            spacing: 8
                            leftPadding: 13
                            Icon { name: chip.modelData.icon; size: 15; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: chip.modelData.label
                                font.family: Theme.font
                                font.pixelSize: Theme.fontSmall + 1
                                color: Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            Toast {
                id: toast
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: composerArea.top
                anchors.bottomMargin: 12
                z: 5
            }
        }

        SidePanel {
            Layout.fillHeight: true
            Layout.preferredWidth: window.panelOpen ? 300 : 0
            visible: Layout.preferredWidth > 0
            clip: true
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
            onClose: window.panelOpen = false
            onOpenSettings: page => window.openSettings(page)
        }
    }

    RenameDialog {
        id: titleRename
        onRenamed: newTitle => App.renameSession(targetId, newTitle)
    }
    ConfirmDialog {
        id: titleDelete
        heading: "Sohbet silinsin mi?"
        message: "“" + App.sessionTitle + "” kalıcı olarak silinecek."
        confirmText: "Sil"
        onConfirmed: App.deleteSession(App.sessionId)
    }

    SettingsDialog { id: settings }
    StoreDialog { id: storeDialog }
    // Dev aid: installs store items named in ORBITAL_DEV_SETTINGS=store:<id>:<id>…
    Timer {
        id: devInstall
        property var ids: []
        interval: 1500
        running: ids.length > 0
        onTriggered: { ids.forEach(id => App.installStoreItem(id)); ids = [] }
    }
    PermissionDialog {}
    QuestionDialog {}

    Component.onCompleted: {
        composer.focusInput()
        const hook = App.devHook()
        if (hook.length) {
            const parts = hook.split(":")
            if (parts[0] === "store") {
                storeDialog.openStore()
                if (parts.length > 1)
                    devInstall.ids = parts.slice(1)
                return
            }
            settings.openAt(parts[0])
            if (parts.length > 2)
                settings.devConnect(parts[1], parts.slice(2).join(":"))
        }
    }
}
