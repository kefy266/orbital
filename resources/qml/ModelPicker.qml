import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Header button showing the current model; opens a searchable list grouped
// by provider, with the engine switch on top.
AbstractButton {
    id: picker
    hoverEnabled: true
    implicitHeight: 34
    implicitWidth: label.implicitWidth + 40
    onClicked: popup.opened ? popup.close() : popup.open()

    background: Rectangle {
        radius: Theme.radiusSm + 2
        color: picker.hovered || popup.visible ? Theme.hover : "transparent"
    }
    contentItem: Row {
        spacing: 6
        leftPadding: 12
        Text {
            id: label
            text: App.modelLabel
            font.family: Theme.font
            font.pixelSize: Theme.fontUi + 1
            font.weight: Font.Medium
            color: Theme.text
            anchors.verticalCenter: parent.verticalCenter
        }
        Icon {
            name: "lock"
            size: 13
            color: Theme.danger
            visible: App.isCurrentModelLocked
            anchors.verticalCenter: parent.verticalCenter
        }
        Icon { name: "chevron-down"; size: 14; color: Theme.textFaint; anchors.verticalCenter: parent.verticalCenter }
    }

    Popup {
        id: popup
        y: picker.height + 6
        x: Math.min(0, parent.width - width)
        width: 380
        height: Math.min(520, layout.implicitHeight + 24)
        padding: 12
        onOpened: { search.text = ""; search.forceActiveFocus() }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fast }
            NumberAnimation { property: "y"; from: picker.height; to: picker.height + 6; duration: Theme.normal; easing.type: Easing.OutCubic }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.fast } }
        background: Rectangle { radius: Theme.radius + 2; color: Theme.elevated; border.width: 1; border.color: Theme.border }

        contentItem: ColumnLayout {
            id: layout
            spacing: 10

            OTextField {
                id: search
                Layout.fillWidth: true
                implicitHeight: 36
                iconName: "search"
                placeholderText: "Model ara"
            }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 340)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                model: App.models.filter(m => search.text.length === 0
                                         || (m.name + " " + m.model + " " + m.provider).toLowerCase().indexOf(search.text.toLowerCase()) >= 0)
                section.property: "provider"
                section.delegate: Text {
                    required property string section
                    width: ListView.view.width
                    text: section
                    topPadding: 10
                    bottomPadding: 4
                    leftPadding: 10
                    font.family: Theme.font
                    font.pixelSize: Theme.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.3
                    color: Theme.textFaint
                }
                delegate: AbstractButton {
                    id: item
                    required property var modelData
                    readonly property bool current: modelData.ref === App.modelRef
                    readonly property bool locked: modelData.locked === true
                    width: ListView.view.width
                    implicitHeight: 38
                    hoverEnabled: true
                    onClicked: {
                        App.setModel(modelData.ref)
                        popup.close()
                        if (locked)
                            App.toast("Bu modelin kotası dolmuştu (🔒). Kilidi kaldırmak için /kilit-ac yazabilirsiniz.")
                    }
                    background: Rectangle { radius: Theme.radiusSm; color: item.hovered ? Theme.hover : "transparent" }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            Layout.leftMargin: 10
                            text: item.modelData.name
                            elide: Text.ElideRight
                            font.family: Theme.font
                            font.pixelSize: Theme.fontUi
                            color: item.locked ? Theme.textFaint : Theme.text
                        }
                        Rectangle {
                            visible: item.locked
                            radius: 4
                            color: Qt.alpha(Theme.danger, 0.15)
                            border.width: 1
                            border.color: Qt.alpha(Theme.danger, 0.35)
                            implicitHeight: 18
                            implicitWidth: lockRow.implicitWidth + 8
                            Row {
                                id: lockRow
                                anchors.centerIn: parent
                                spacing: 3
                                Icon { name: "lock"; size: 11; color: Theme.danger; anchors.verticalCenter: parent.verticalCenter }
                                Text {
                                    text: "Limit Doldu"
                                    font.family: Theme.font
                                    font.pixelSize: Theme.fontSmall - 1
                                    font.weight: Font.Medium
                                    color: Theme.danger
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Icon { Layout.rightMargin: 10; visible: item.current; name: "check"; size: 16; color: Theme.accent }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: App.engineReady ? "Model bulunamadı" : "Hazırlanıyor…"
                    font.family: Theme.font
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textFaint
                }
            }
        }
    }
}
