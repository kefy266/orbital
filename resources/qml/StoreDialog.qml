import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Skill & plugin store: browse the catalog hosted in the Orbital repo and
// install or remove items with one click.
Popup {
    id: store
    property string filter: "all"
    function openStore() {
        filter = "all"
        search.text = ""
        open()
        App.loadStore()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(980, parent.width - 48)
    height: Math.min(700, parent.height - 48)
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

    Connections {
        target: App
        function onStoreItemDone(id, ok, message) {
            const item = App.storeItems.find(i => i.id === id)
            notice.show((item ? item.name : id) + " · " + message, !ok)
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 28
            Layout.bottomMargin: 16
            spacing: 14
            RowLayout {
                spacing: 12
                Rectangle {
                    width: 40; height: 40; radius: 11
                    color: Theme.accentSoft
                    Icon { anchors.centerIn: parent; name: "store"; size: 20; color: Theme.accent }
                }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    Text { text: "Mağaza"; font.family: Theme.font; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text }
                    Text {
                        text: "Orbital'e yeni skill'ler ve eklentiler ekle. Yüklediklerin hemen kullanılabilir."
                        font.family: Theme.font; font.pixelSize: Theme.fontUi; color: Theme.textMuted
                    }
                }
                IconButton { Layout.alignment: Qt.AlignTop; iconName: "x"; tip: "Kapat (Esc)"; onClicked: store.close() }
            }
            RowLayout {
                spacing: 10
                OTextField {
                    id: search
                    Layout.fillWidth: true
                    iconName: "search"
                    placeholderText: "Skill ya da eklenti ara"
                }
                Segmented {
                    implicitHeight: 38
                    model: [{ value: "all", label: "Tümü" }, { value: "skill", label: "Skill'ler", icon: "sparkle" }, { value: "plugin", label: "Eklentiler", icon: "blocks" }]
                    value: store.filter
                    onPicked: v => store.filter = v
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        // Catalog
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                visible: App.storeLoading || App.storeError.length > 0
                spacing: 12
                Spinner { Layout.alignment: Qt.AlignHCenter; visible: App.storeLoading; width: 24; height: 24 }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: App.storeLoading ? "Mağaza yükleniyor…" : App.storeError
                    font.family: Theme.font; font.pixelSize: Theme.fontUi
                    color: App.storeError.length ? Theme.danger : Theme.textMuted
                }
                OButton {
                    Layout.alignment: Qt.AlignHCenter
                    visible: !App.storeLoading && App.storeError.length > 0
                    text: "Tekrar dene"
                    iconName: "refresh"
                    onClicked: App.loadStore()
                }
            }

            GridView {
                id: grid
                anchors.fill: parent
                anchors.margins: 20
                anchors.rightMargin: 12
                visible: !App.storeLoading && App.storeError.length === 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                readonly property int columns: Math.max(1, Math.floor(width / 300))
                cellWidth: Math.floor(width / columns)
                cellHeight: 196
                model: App.storeItems.filter(i => (store.filter === "all" || i.type === store.filter)
                                             && (search.text.length === 0
                                                 || (i.name + " " + i.description + " " + i.tags.join(" ")).toLowerCase()
                                                       .indexOf(search.text.toLowerCase()) >= 0))

                delegate: Item {
                    id: cell
                    required property var modelData
                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 6
                        radius: Theme.radius
                        color: hover.hovered ? Theme.surfaceAlt : Theme.surface
                        border.width: 1
                        border.color: Theme.border
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                        HoverHandler { id: hover }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 8

                            RowLayout {
                                spacing: 10
                                Rectangle {
                                    width: 34; height: 34; radius: 9
                                    color: Theme.accentSoft
                                    Icon {
                                        anchors.centerIn: parent
                                        name: cell.modelData.type === "plugin" ? "blocks" : "sparkle"
                                        size: 17
                                        color: Theme.accent
                                    }
                                }
                                ColumnLayout {
                                    spacing: 0
                                    Layout.fillWidth: true
                                    Text {
                                        text: cell.modelData.name
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        font.family: Theme.font; font.pixelSize: Theme.fontUi + 1; font.weight: Font.DemiBold; color: Theme.text
                                    }
                                    Text {
                                        text: (cell.modelData.type === "plugin" ? "Eklenti" : "Skill") + " · " + cell.modelData.author + " · v" + cell.modelData.version
                                        font.family: Theme.font; font.pixelSize: Theme.fontCaption; color: Theme.textFaint
                                    }
                                }
                            }
                            Text {
                                text: cell.modelData.description
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                                font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.textMuted
                                lineHeight: 1.15
                            }
                            Item { Layout.fillHeight: true }
                            RowLayout {
                                visible: !!cell.modelData.runsCode
                                spacing: 5
                                Icon { name: "alert"; size: 13; color: Theme.textFaint }
                                Text {
                                    text: "Bilgisayarında komut çalıştırabilir"
                                    font.family: Theme.font; font.pixelSize: Theme.fontCaption; color: Theme.textFaint
                                }
                            }
                            RowLayout {
                                spacing: 6
                                Repeater {
                                    model: cell.modelData.tags.slice(0, 2)
                                    delegate: Rectangle {
                                        required property string modelData
                                        implicitWidth: tagText.implicitWidth + 14
                                        implicitHeight: 22
                                        radius: 11
                                        color: Theme.surfaceAlt
                                        border.width: 1
                                        border.color: Theme.border
                                        Text { id: tagText; anchors.centerIn: parent; text: modelData; font.family: Theme.font; font.pixelSize: 11; color: Theme.textMuted }
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                Spinner { visible: cell.modelData.busy; width: 18; height: 18 }
                                OButton {
                                    visible: cell.modelData.installed && !cell.modelData.busy
                                    implicitHeight: 30
                                    text: "Kaldır"
                                    variant: "ghost"
                                    onClicked: App.removeStoreItem(cell.modelData.id)
                                }
                                Rectangle {
                                    visible: cell.modelData.installed && !cell.modelData.busy
                                    implicitWidth: installedRow.implicitWidth + 16
                                    implicitHeight: 30
                                    radius: Theme.radiusSm + 2
                                    color: Qt.alpha(Theme.success, 0.14)
                                    Row {
                                        id: installedRow
                                        anchors.centerIn: parent
                                        spacing: 4
                                        Icon { name: "check"; size: 14; color: Theme.success; anchors.verticalCenter: parent.verticalCenter }
                                        Text { text: "Yüklü"; font.family: Theme.font; font.pixelSize: Theme.fontSmall; font.weight: Font.Medium; color: Theme.success; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                }
                                OButton {
                                    visible: !cell.modelData.installed && !cell.modelData.busy
                                    implicitHeight: 30
                                    text: "Yükle"
                                    iconName: "download"
                                    variant: "primary"
                                    onClicked: App.installStoreItem(cell.modelData.id)
                                }
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: grid.count === 0
                    text: "Sonuç bulunamadı"
                    font.family: Theme.font; font.pixelSize: Theme.fontUi; color: Theme.textFaint
                }
            }

            Rectangle {
                id: notice
                property bool error: false
                function show(msg, isError) { label.text = msg; error = isError; opacity = 1; noticeTimer.restart() }
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 18
                width: label.implicitWidth + 32
                height: 36
                radius: 18
                color: error ? Theme.danger : Theme.text
                opacity: 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.normal } }
                Text { id: label; anchors.centerIn: parent; font.family: Theme.font; font.pixelSize: Theme.fontSmall; color: Theme.bg }
                Timer { id: noticeTimer; interval: 3000; onTriggered: notice.opacity = 0 }
            }
        }
    }
}
