import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Orbital

// Settings modal: left navigation, pages on the right. Engine-affecting
// changes are applied (engine restart) when the dialog closes.
Popup {
    id: dialog
    property string page: "general"
    property int revision: 0          // bump to re-read lists from App
    function openAt(p) { page = p; revision++; open() }
    function refresh() { revision++ }
    function devConnect(id, key) {
        const p = App.providerCatalog().find(x => x.id === id)
        if (!p) return
        connectDialog.openFor(p)
        keyField.text = key
        connectButton.clicked()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(920, parent.width - 48)
    height: Math.min(660, parent.height - 48)
    modal: true
    padding: 0
    Overlay.modal: Rectangle { color: Theme.overlay }
    onClosed: App.applySettings()
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.fast } }
    background: Rectangle { radius: Theme.radiusLg; color: Theme.bg; border.width: 1; border.color: Theme.border }

    FolderDialog { id: projectDialog; onAccepted: App.setProject(selectedFolder) }
    FolderDialog { id: importSkillDialog; title: "SKILL.md içeren klasörü seç"
        onAccepted: { const e = App.importSkill(selectedFolder); if (e) errorText.show(e); dialog.refresh() } }
    FolderDialog { id: installPluginDialog; title: "plugin.json içeren klasörü seç"
        onAccepted: { const e = App.installPlugin(selectedFolder); if (e) errorText.show(e); dialog.refresh() } }

    // ---------------------------------------------------------- building blocks
    component PageTitle: ColumnLayout {
        property string title
        property string subtitle
        spacing: 4
        Layout.fillWidth: true
        Layout.bottomMargin: 8
        Text { text: parent.title; font.family: Theme.font; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text }
        Text {
            visible: parent.subtitle.length > 0
            text: parent.subtitle
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.family: Theme.font; font.pixelSize: Theme.fontUi; color: Theme.textMuted
        }
    }
    component Card: Rectangle {
        default property alias content: inner.data
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight
        radius: Theme.radius
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        ColumnLayout { id: inner; width: parent.width; spacing: 0 }
    }
    component SettingRow: Item {
        id: srow
        property string title
        property string subtitle
        property bool divider: true
        default property alias trailing: trail.data
        Layout.fillWidth: true
        implicitHeight: Math.max(56, texts.implicitHeight + 24)
        ColumnLayout {
            id: texts
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: trail.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text { text: srow.title; font.family: Theme.font; font.pixelSize: Theme.fontUi; font.weight: Font.Medium; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
            Text {
                visible: srow.subtitle.length > 0
                text: srow.subtitle
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                font.family: Theme.font; font.pixelSize: Theme.fontCaption + 1; color: Theme.textFaint
            }
        }
        Row { id: trail; anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; spacing: 6 }
        Rectangle { visible: srow.divider; anchors.bottom: parent.bottom; x: 16; width: parent.width - 16; height: 1; color: Theme.border }
    }
    component SectionLabel: Text {
        Layout.topMargin: 14
        Layout.bottomMargin: 2
        font.family: Theme.font; font.pixelSize: Theme.fontCaption; font.weight: Font.DemiBold; color: Theme.textFaint
    }

    contentItem: RowLayout {
        spacing: 0

        // Navigation
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 220
            color: Theme.sidebar
            radius: Theme.radiusLg
            Rectangle { anchors.right: parent.right; width: Theme.radiusLg; height: parent.height; color: Theme.sidebar }
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                anchors.topMargin: 18
                spacing: 2
                Text { text: "Ayarlar"; Layout.leftMargin: 10; Layout.bottomMargin: 12; font.family: Theme.font; font.pixelSize: Theme.fontTitle; font.weight: Font.DemiBold; color: Theme.text }
                Repeater {
                    model: [
                        { id: "general", label: "Genel", icon: "settings" },
                        { id: "providers", label: "Sağlayıcılar", icon: "server" },
                        { id: "permissions", label: "İzinler", icon: "shield" },
                        { id: "skills", label: "Skill'ler", icon: "sparkle" },
                        { id: "plugins", label: "Eklentiler", icon: "blocks" }
                    ]
                    delegate: AbstractButton {
                        id: navItem
                        required property var modelData
                        readonly property bool on: dialog.page === modelData.id
                        Layout.fillWidth: true
                        implicitHeight: 36
                        hoverEnabled: true
                        onClicked: { dialog.page = modelData.id; skillEditor.visible = false }
                        background: Rectangle { radius: Theme.radiusSm; color: navItem.on ? Theme.selected : (navItem.hovered ? Theme.hover : "transparent") }
                        contentItem: Row {
                            spacing: 10
                            leftPadding: 10
                            Icon { name: navItem.modelData.icon; size: 16; color: navItem.on ? Theme.text : Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: navItem.modelData.label; font.family: Theme.font; font.pixelSize: Theme.fontUi; font.weight: navItem.on ? Font.Medium : Font.Normal; color: navItem.on ? Theme.text : Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Text { text: "Orbital 0.3"; Layout.leftMargin: 10; font.family: Theme.font; font.pixelSize: Theme.fontCaption; color: Theme.textFaint }
            }
        }

        // Pages
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            IconButton { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12; z: 2; iconName: "x"; tip: "Kapat (Esc)"; onClicked: dialog.close() }

            Text {
                id: errorText
                function show(msg) { text = msg; opacity = 1; errorTimer.restart() }
                z: 3
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 16
                opacity: 0
                color: Theme.danger
                font.family: Theme.font
                font.pixelSize: Theme.fontSmall
                Behavior on opacity { NumberAnimation { duration: Theme.normal } }
                Timer { id: errorTimer; interval: 4000; onTriggered: errorText.opacity = 0 }
            }

            Rectangle {
                id: successText
                function show(msg) { successLabel.text = msg; opacity = 1; successTimer.restart() }
                z: 3
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 16
                width: successLabel.implicitWidth + 32
                height: 36
                radius: 18
                color: Theme.text
                opacity: 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.normal } }
                Text { id: successLabel; anchors.centerIn: parent; font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.bg }
                Timer { id: successTimer; interval: 3500; onTriggered: successText.opacity = 0 }
            }

            ScrollView {
                id: scroller
                anchors.fill: parent
                anchors.topMargin: 8
                contentWidth: availableWidth
                visible: !skillEditor.visible
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: scroller.availableWidth - 56
                    x: 28
                    y: 22
                    spacing: 10

                    // ----------------------------------------------------- General
                    ColumnLayout {
                        visible: dialog.page === "general"
                        Layout.fillWidth: true
                        spacing: 10
                        PageTitle { title: "Genel" }
                        SectionLabel { text: "GÖRÜNÜM" }
                        Card {
                            SettingRow {
                                title: "Tema"
                                subtitle: "Sistem ayarını izle ya da açık/koyu temayı sabitle."
                                divider: false
                                Segmented {
                                    model: [{ value: "system", label: "Sistem", icon: "monitor" }, { value: "light", label: "Açık", icon: "sun" }, { value: "dark", label: "Koyu", icon: "moon" }]
                                    value: App.themeMode
                                    onPicked: v => App.themeMode = v
                                }
                            }
                        }
                        SectionLabel { text: "PROJE VE DOSYALAR" }
                        Card {
                            SettingRow {
                                title: App.projectName
                                subtitle: App.projectPath
                                OButton { text: "Değiştir"; iconName: "folder"; onClicked: projectDialog.open() }
                            }
                            Repeater {
                                model: [
                                    { key: "config", label: "Yapılandırma dosyası" },
                                    { key: "data", label: "Orbital veri klasörü" },
                                    { key: "engine", label: "Orbital Cloud kurulum klasörü" }
                                ]
                                delegate: SettingRow {
                                    required property var modelData
                                    required property int index
                                    title: modelData.label
                                    subtitle: App.paths()[modelData.key]
                                    divider: index < 2
                                    IconButton { iconName: "external"; tip: "Aç"; onClicked: App.openPath(App.paths()[modelData.key]) }
                                }
                            }
                        }
                    }

                    // ----------------------------------------------------- Providers
                    ColumnLayout {
                        visible: dialog.page === "providers"
                        Layout.fillWidth: true
                        spacing: 10
                        PageTitle {
                            title: "Sağlayıcılar"
                            subtitle: "API anahtarını ekleyerek kendi hesabındaki modelleri bağla. Bağlanan modeller model menüsünde görünür."
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: width > 560 ? 2 : 1
                            columnSpacing: 10
                            rowSpacing: 10
                            Repeater {
                                model: dialog.revision >= 0 ? App.providerCatalog() : []
                                delegate: AbstractButton {
                                    id: tile
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 72
                                    hoverEnabled: true
                                    onClicked: connectDialog.openFor(modelData)
                                    background: Rectangle {
                                        radius: Theme.radius
                                        color: tile.hovered ? Theme.hover : Theme.surface
                                        border.width: 1
                                        border.color: tile.modelData.connected ? Qt.alpha(Theme.success, 0.45) : Theme.border
                                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                                    }
                                    contentItem: RowLayout {
                                        spacing: 12
                                        Rectangle {
                                            Layout.leftMargin: 14
                                            width: 36; height: 36; radius: 10
                                            color: tile.modelData.color
                                            Text {
                                                anchors.centerIn: parent
                                                text: tile.modelData.name.charAt(0)
                                                font.family: Theme.font
                                                font.pixelSize: 17
                                                font.weight: Font.Bold
                                                color: "#ffffff"
                                            }
                                        }
                                        ColumnLayout {
                                            spacing: 2
                                            Layout.fillWidth: true
                                            Text {
                                                text: tile.modelData.name
                                                font.family: Theme.font; font.pixelSize: Theme.fontUi; font.weight: Font.Medium; color: Theme.text
                                            }
                                            Text {
                                                text: tile.modelData.description
                                                Layout.fillWidth: true; elide: Text.ElideRight
                                                font.family: Theme.font; font.pixelSize: Theme.fontCaption + 1; color: Theme.textFaint
                                            }
                                        }
                                        Rectangle {
                                            Layout.rightMargin: 14
                                            implicitWidth: statusRow.implicitWidth + 18
                                            implicitHeight: 26
                                            radius: 13
                                            color: tile.modelData.connected ? Qt.alpha(Theme.success, 0.14) : Theme.surfaceAlt
                                            border.width: tile.modelData.connected ? 0 : 1
                                            border.color: Theme.border
                                            Row {
                                                id: statusRow
                                                anchors.centerIn: parent
                                                spacing: 4
                                                Icon { visible: tile.modelData.connected; name: "check"; size: 13; color: Theme.success; anchors.verticalCenter: parent.verticalCenter }
                                                Text {
                                                    text: tile.modelData.connected ? "Bağlı" : "Bağla"
                                                    font.family: Theme.font; font.pixelSize: Theme.fontCaption + 1; font.weight: Font.Medium
                                                    color: tile.modelData.connected ? Theme.success : Theme.text
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        OButton {
                            Layout.topMargin: 6
                            text: "Özel sağlayıcı ekle (OpenAI uyumlu)"
                            iconName: "plus"
                            variant: "ghost"
                            onClicked: customDialog.open()
                        }
                    }

                    // ----------------------------------------------------- Permissions
                    ColumnLayout {
                        visible: dialog.page === "permissions"
                        Layout.fillWidth: true
                        spacing: 10
                        PageTitle { title: "İzinler"; subtitle: "Orbital'in her araç için ne yapacağını seç. \"Sor\" seçiliyse çalıştırmadan önce onayın istenir." }
                        Card {
                            Repeater {
                                model: [
                                    { tool: "read", label: "Dosya okuma", icon: "file" },
                                    { tool: "list", label: "Klasör listeleme", icon: "folder" },
                                    { tool: "glob", label: "Dosya arama", icon: "search" },
                                    { tool: "grep", label: "İçerik arama", icon: "search" },
                                    { tool: "skill", label: "Skill yükleme", icon: "sparkle" },
                                    { tool: "write", label: "Dosya yazma", icon: "pencil" },
                                    { tool: "edit", label: "Dosya düzenleme", icon: "pencil" },
                                    { tool: "bash", label: "Komut çalıştırma", icon: "terminal" },
                                    { tool: "*", label: "Diğer araçlar (eklenti ve MCP)", icon: "blocks" }
                                ]
                                delegate: SettingRow {
                                    required property var modelData
                                    required property int index
                                    title: modelData.label
                                    subtitle: modelData.tool === "*" ? "" : modelData.tool
                                    divider: index < 8
                                    Segmented {
                                        model: [{ value: "allow", label: "İzin ver" }, { value: "ask", label: "Sor" }, { value: "deny", label: "Reddet" }]
                                        value: dialog.revision >= 0 ? (App.permissionMap()[modelData.tool] || "ask") : "ask"
                                        onPicked: v => { App.setPermission(modelData.tool, v); dialog.refresh() }
                                    }
                                }
                            }
                        }
                    }

                    // ----------------------------------------------------- Skills
                    ColumnLayout {
                        visible: dialog.page === "skills"
                        Layout.fillWidth: true
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            PageTitle { title: "Skill'ler"; subtitle: "Modelin belirli işleri nasıl yapacağını anlatan talimat paketleri. Etkin skill'leri model gerektiğinde kendisi yükler; sohbette /skill-adı ya da + menüsüyle mesaja iliştirebilirsin." }
                            Row {
                                Layout.alignment: Qt.AlignTop
                                spacing: 6
                                OButton { text: "İçe aktar"; variant: "ghost"; onClicked: importSkillDialog.open() }
                                OButton { text: "Yeni skill"; iconName: "plus"; variant: "primary"; onClicked: skillEditor.edit(null) }
                            }
                        }
                        Card {
                            Repeater {
                                id: skillRepeater
                                model: dialog.revision >= 0 ? App.skillList() : []
                                delegate: SettingRow {
                                    required property var modelData
                                    required property int index
                                    title: modelData.name + (modelData.editable ? "" : "  ·  " + modelData.origin)
                                    subtitle: modelData.description
                                    divider: index < skillRepeater.count - 1
                                    IconButton { visible: modelData.editable; iconName: "pencil"; tip: "Düzenle"; onClicked: skillEditor.edit(modelData) }
                                    OSwitch { checked: modelData.enabled; onToggled: App.setSkillEnabled(modelData.name, checked) }
                                }
                            }
                        }
                        OButton { text: "Skill klasörünü aç"; iconName: "external"; variant: "ghost"; onClicked: App.openPath(App.paths().skills) }
                    }

                    // ----------------------------------------------------- Plugins
                    ColumnLayout {
                        visible: dialog.page === "plugins"
                        Layout.fillWidth: true
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            PageTitle { title: "Eklentiler"; subtitle: "Slash komutları, MCP araç sunucuları, betik araçları, paketlenmiş skill'ler, talimatlar, eylemler ve prompt hook'ları ekler. Her eklenti plugin.json içeren bir klasördür." }
                            Row {
                                Layout.alignment: Qt.AlignTop
                                spacing: 6
                                OButton { text: "Klasörden kur"; variant: "ghost"; onClicked: installPluginDialog.open() }
                                OButton { text: "Yeni eklenti"; iconName: "plus"; variant: "primary"; onClicked: newPlugin.open() }
                            }
                        }
                        Repeater {
                            model: dialog.revision >= 0 ? App.pluginList() : []
                            delegate: Card {
                                id: plcard
                                required property var modelData
                                property bool open: false
                                AbstractButton {
                                    Layout.fillWidth: true
                                    implicitHeight: 68
                                    hoverEnabled: true
                                    onClicked: plcard.open = !plcard.open
                                    background: Item {}
                                    contentItem: RowLayout {
                                        spacing: 12
                                        Rectangle {
                                            Layout.leftMargin: 14
                                            width: 36; height: 36; radius: 10
                                            color: plcard.modelData.error ? Theme.dangerSoft : Theme.accentSoft
                                            Icon { anchors.centerIn: parent; name: plcard.modelData.error ? "alert" : "blocks"; size: 18; color: plcard.modelData.error ? Theme.danger : Theme.accent }
                                        }
                                        ColumnLayout {
                                            spacing: 2
                                            Layout.fillWidth: true
                                            Text {
                                                text: plcard.modelData.name + (plcard.modelData.version ? "  <font color='" + Theme.textFaint + "'>v" + plcard.modelData.version + "</font>" : "")
                                                textFormat: Text.StyledText
                                                font.family: Theme.font; font.pixelSize: Theme.fontUi; font.weight: Font.Medium; color: Theme.text
                                            }
                                            Text {
                                                text: plcard.modelData.error || plcard.modelData.description
                                                Layout.fillWidth: true; elide: Text.ElideRight
                                                font.family: Theme.font; font.pixelSize: Theme.fontCaption + 1
                                                color: plcard.modelData.error ? Theme.danger : Theme.textFaint
                                            }
                                        }
                                        OSwitch {
                                            Layout.rightMargin: 16
                                            checked: plcard.modelData.enabled
                                            onToggled: App.setPluginEnabled(plcard.modelData.id, checked)
                                        }
                                    }
                                }
                                ColumnLayout {
                                    visible: plcard.open
                                    Layout.fillWidth: true
                                    Layout.margins: 16
                                    Layout.topMargin: 0
                                    spacing: 10
                                    Text {
                                        visible: plcard.modelData.summary.length > 0
                                        text: "Katkılar: " + plcard.modelData.summary
                                        font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.textMuted
                                    }
                                    Text {
                                        text: plcard.modelData.dir
                                        Layout.fillWidth: true; elide: Text.ElideMiddle
                                        font.family: Theme.mono; font.pixelSize: Theme.fontCaption; color: Theme.textFaint
                                    }
                                    RowLayout {
                                        spacing: 6
                                        OButton { text: "Klasörü aç"; iconName: "folder"; variant: "ghost"; onClicked: App.openPath(plcard.modelData.dir) }
                                        OButton { text: "plugin.json"; iconName: "pencil"; variant: "ghost"; onClicked: App.openPath(plcard.modelData.dir + "/plugin.json") }
                                        Item { Layout.fillWidth: true }
                                        OButton {
                                            text: "Kaldır"; iconName: "trash"; variant: "danger"
                                            onClicked: { removePluginConfirm.targetId = plcard.modelData.id; removePluginConfirm.message = "“" + plcard.modelData.name + "” ve klasörü silinecek."; removePluginConfirm.open() }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Item { height: 28; width: 1 }
                }
            }

            // ----------------------------------------------------- Skill editor
            ColumnLayout {
                id: skillEditor
                property var skill: null
                function edit(s) {
                    skill = s
                    nameField.text = s ? s.name : ""
                    descField.text = s ? s.description : ""
                    bodyArea.text = s ? s.body : "# Talimatlar\n\n1. "
                    visible = true
                    nameField.forceActiveFocus()
                }
                visible: false
                anchors.fill: parent
                anchors.margins: 28
                anchors.topMargin: 22
                spacing: 12

                RowLayout {
                    spacing: 8
                    IconButton { iconName: "chevron-left"; tip: "Geri"; onClicked: skillEditor.visible = false }
                    Text {
                        text: skillEditor.skill ? "Skill'i düzenle" : "Yeni skill"
                        Layout.fillWidth: true
                        font.family: Theme.font; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text
                    }
                }
                OTextField { id: nameField; Layout.fillWidth: true; placeholderText: "Ad (ör. kod-inceleme)" }
                OTextField { id: descField; Layout.fillWidth: true; placeholderText: "Açıklama — model bu skill'i ne zaman kullanmalı?" }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: bodyArea.activeFocus ? 2 : 1
                    border.color: bodyArea.activeFocus ? Theme.accent : Theme.border
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 2
                        TextArea {
                            id: bodyArea
                            wrapMode: TextArea.Wrap
                            font.family: Theme.mono
                            font.pixelSize: 14
                            color: Theme.text
                            selectionColor: Theme.accentSoft
                            padding: 14
                            background: null
                            placeholderText: "Markdown talimatlar"
                            placeholderTextColor: Theme.textFaint
                        }
                    }
                }
                RowLayout {
                    spacing: 8
                    OButton {
                        visible: skillEditor.skill !== null
                        text: "Sil"; iconName: "trash"; variant: "danger"
                        onClicked: {
                            const e = App.deleteSkill(skillEditor.skill.name)
                            if (e) { errorText.show(e); return }
                            skillEditor.visible = false
                            dialog.refresh()
                        }
                    }
                    Item { Layout.fillWidth: true }
                    OButton { text: "Vazgeç"; variant: "ghost"; onClicked: skillEditor.visible = false }
                    OButton {
                        text: "Kaydet"; variant: "primary"
                        onClicked: {
                            const e = App.saveSkill(skillEditor.skill ? skillEditor.skill.name : "", nameField.text, descField.text, bodyArea.text)
                            if (e) { errorText.show(e); return }
                            skillEditor.visible = false
                            dialog.refresh()
                        }
                    }
                }
            }
        }
    }

    // Small prompts
    component NamePrompt: Popup {
        id: prompt
        property string heading
        property string placeholder
        signal submitted(string value)
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 380
        modal: true
        padding: 22
        Overlay.modal: Rectangle { color: "transparent" }
        onOpened: { field.text = ""; field.forceActiveFocus() }
        background: Rectangle { radius: Theme.radiusLg; color: Theme.elevated; border.width: 1; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: prompt.heading; font.family: Theme.font; font.pixelSize: Theme.fontTitle; font.weight: Font.DemiBold; color: Theme.text }
            OTextField { id: field; Layout.fillWidth: true; placeholderText: prompt.placeholder; onAccepted: ok.clicked() }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                OButton { text: "Vazgeç"; variant: "ghost"; onClicked: prompt.close() }
                OButton { id: ok; text: "Oluştur"; variant: "primary"; enabled: field.text.trim().length > 0
                    onClicked: { prompt.close(); prompt.submitted(field.text.trim()) } }
            }
        }
    }
    NamePrompt {
        id: newProvider
        heading: "Yeni sağlayıcı"
        placeholder: "ör. LM Studio"
        onSubmitted: v => { App.addProvider(v); dialog.refresh() }
    }
    NamePrompt {
        id: newPlugin
        heading: "Yeni eklenti"
        placeholder: "kimlik, ör. benim-araclarim"
        onSubmitted: v => { const e = App.createPlugin(v); if (e) errorText.show(e); dialog.refresh() }
    }
    ConfirmDialog {
        id: removePluginConfirm
        property string targetId
        heading: "Eklenti kaldırılsın mı?"
        confirmText: "Kaldır"
        onConfirmed: { const e = App.removePlugin(targetId); if (e) errorText.show(e); dialog.refresh() }
    }

    // Connect a provider with an API key; the key is tested before it is saved.
    Popup {
        id: connectDialog
        property var provider: ({})
        property bool busy: false
        property string error: ""
        function openFor(p) { provider = p; error = ""; busy = false; keyField.text = ""; open() }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        modal: true
        padding: 24
        Overlay.modal: Rectangle { color: "transparent" }
        onOpened: keyField.forceActiveFocus()
        background: Rectangle { radius: Theme.radiusLg; color: Theme.elevated; border.width: 1; border.color: Theme.border }
        Connections {
            target: App
            function onProviderResult(id, ok, message) {
                if (id !== connectDialog.provider.id || !connectDialog.visible)
                    return
                connectDialog.busy = false
                if (ok) { connectDialog.close(); dialog.refresh(); successText.show(connectDialog.provider.name + " bağlandı · " + message) }
                else connectDialog.error = message
            }
        }
        contentItem: ColumnLayout {
            spacing: 12
            RowLayout {
                spacing: 12
                Rectangle {
                    width: 40; height: 40; radius: 11
                    color: connectDialog.provider.color || Theme.accent
                    Text { anchors.centerIn: parent; text: (connectDialog.provider.name || "?").charAt(0); font.family: Theme.font; font.pixelSize: 19; font.weight: Font.Bold; color: "#ffffff" }
                }
                ColumnLayout {
                    spacing: 1
                    Text { text: (connectDialog.provider.name || "") + (connectDialog.provider.connected ? " bağlı" : " bağla"); font.family: Theme.font; font.pixelSize: Theme.fontTitle; font.weight: Font.DemiBold; color: Theme.text }
                    Text { text: connectDialog.provider.description || ""; font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.textMuted }
                }
            }
            Text {
                visible: (connectDialog.provider.keyUrl || "").length > 0
                text: "<a href=\"" + connectDialog.provider.keyUrl + "\">API anahtarı al →</a>"
                textFormat: Text.StyledText
                linkColor: Theme.accent
                font.family: Theme.font
                font.pixelSize: Theme.fontSmall
                onLinkActivated: link => Qt.openUrlExternally(link)
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
            OTextField {
                id: keyField
                Layout.fillWidth: true
                placeholderText: connectDialog.provider.connected ? "Yeni anahtar yapıştır (değiştirmek için)" : "API anahtarını yapıştır"
                echoMode: showKey.checked ? TextInput.Normal : TextInput.Password
                rightPadding: 40
                enabled: !connectDialog.busy
                onAccepted: connectButton.clicked()
                IconButton {
                    id: showKey
                    checkable: true
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 30; implicitHeight: 30
                    iconName: "info"
                    tip: checked ? "Gizle" : "Göster"
                    active: checked
                }
            }
            Text {
                visible: connectDialog.error.length > 0
                text: connectDialog.error
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.danger
            }
            Text {
                text: "Anahtar yalnızca bu bilgisayarda, Orbital'in ayar dosyasında saklanır."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.family: Theme.font; font.pixelSize: Theme.fontCaption; color: Theme.textFaint
            }
            RowLayout {
                Layout.topMargin: 6
                Layout.fillWidth: true
                spacing: 8
                OButton {
                    visible: !!connectDialog.provider.connected
                    text: "Bağlantıyı kaldır"
                    variant: "danger"
                    onClicked: { App.disconnectProvider(connectDialog.provider.id); connectDialog.close(); dialog.refresh() }
                }
                Item { Layout.fillWidth: true }
                OButton { text: "Vazgeç"; variant: "ghost"; onClicked: connectDialog.close() }
                OButton {
                    id: connectButton
                    text: connectDialog.busy ? "Kontrol ediliyor…" : "Bağla"
                    variant: "primary"
                    enabled: keyField.text.trim().length > 0 && !connectDialog.busy
                    onClicked: { connectDialog.error = ""; connectDialog.busy = true; App.connectProvider(connectDialog.provider.id, keyField.text) }
                }
            }
        }
    }

    // Any other OpenAI-compatible endpoint.
    Popup {
        id: customDialog
        property bool busy: false
        property string error: ""
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 460
        modal: true
        padding: 24
        Overlay.modal: Rectangle { color: "transparent" }
        onOpened: { error = ""; busy = false; cName.text = ""; cUrl.text = ""; cKey.text = ""; cName.forceActiveFocus() }
        background: Rectangle { radius: Theme.radiusLg; color: Theme.elevated; border.width: 1; border.color: Theme.border }
        Connections {
            target: App
            function onProviderResult(id, ok, message) {
                if (id !== "custom" || !customDialog.visible)
                    return
                customDialog.busy = false
                if (ok) { customDialog.close(); dialog.refresh(); successText.show(cName.text + " bağlandı · " + message) }
                else customDialog.error = message
            }
        }
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "Özel sağlayıcı"; font.family: Theme.font; font.pixelSize: Theme.fontTitle; font.weight: Font.DemiBold; color: Theme.text }
            Text {
                text: "OpenAI uyumlu herhangi bir API (Together, Fireworks, Azure OpenAI, kendi sunucun…)."
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.textMuted
            }
            OTextField { id: cName; Layout.fillWidth: true; placeholderText: "Ad (ör. Together)" }
            OTextField { id: cUrl; Layout.fillWidth: true; placeholderText: "Adres (ör. https://api.together.xyz/v1)" }
            OTextField { id: cKey; Layout.fillWidth: true; placeholderText: "API anahtarı"; echoMode: TextInput.Password }
            Text {
                visible: customDialog.error.length > 0
                text: customDialog.error
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.danger
            }
            RowLayout {
                Layout.topMargin: 6
                Layout.alignment: Qt.AlignRight
                spacing: 8
                OButton { text: "Vazgeç"; variant: "ghost"; onClicked: customDialog.close() }
                OButton {
                    text: customDialog.busy ? "Kontrol ediliyor…" : "Ekle"
                    variant: "primary"
                    enabled: cName.text.trim().length > 0 && cUrl.text.trim().length > 0 && !customDialog.busy
                    onClicked: { customDialog.error = ""; customDialog.busy = true; App.addCustomProvider(cName.text, cUrl.text, cKey.text) }
                }
            }
        }
    }
}
