import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

MenuItem {
    id: item
    property string iconName: ""
    property string subtitle: ""
    property bool danger: false
    property bool selectedMark: false

    implicitHeight: subtitle ? 48 : 34
    hoverEnabled: true
    leftPadding: 10
    rightPadding: 10

    contentItem: RowLayout {
        spacing: 10
        Icon {
            visible: item.iconName.length > 0
            name: item.iconName
            size: 16
            color: item.danger ? Theme.danger : Theme.textMuted
        }
        ColumnLayout {
            spacing: 1
            Layout.fillWidth: true
            Text {
                text: item.text
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.family: Theme.font
                font.pixelSize: Theme.fontUi
                color: item.danger ? Theme.danger : Theme.text
            }
            Text {
                visible: item.subtitle.length > 0
                text: item.subtitle
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.family: Theme.font
                font.pixelSize: Theme.fontCaption
                color: Theme.textFaint
            }
        }
        Icon { visible: item.selectedMark; name: "check"; size: 16; color: Theme.accent }
    }
    background: Rectangle {
        radius: Theme.radiusSm
        color: item.highlighted || item.hovered ? (item.danger ? Theme.dangerSoft : Theme.hover) : "transparent"
    }
}
