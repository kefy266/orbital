import QtQuick
import Orbital

// Pill showing an attached skill; removable when `removable` is set.
Rectangle {
    id: chip
    property string name
    property bool removable: false
    signal removed()

    implicitHeight: 26
    implicitWidth: content.implicitWidth + 18
    radius: 13
    color: Theme.accentSoft

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 5
        Icon { name: "sparkle"; size: 13; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: chip.name
            font.family: Theme.font
            font.pixelSize: Theme.fontCaption + 1
            font.weight: Font.Medium
            color: Theme.accent
            anchors.verticalCenter: parent.verticalCenter
        }
        Icon {
            visible: chip.removable
            name: "x"
            size: 12
            color: Theme.accent
            anchors.verticalCenter: parent.verticalCenter
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                cursorShape: Qt.PointingHandCursor
                onClicked: chip.removed()
            }
        }
    }
}
