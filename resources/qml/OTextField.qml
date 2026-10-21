import QtQuick
import QtQuick.Controls.Basic
import Orbital

TextField {
    id: control
    property string iconName: ""

    implicitHeight: 38
    leftPadding: iconName ? 36 : 12
    rightPadding: 12
    font.family: Theme.font
    font.pixelSize: Theme.fontUi
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accentSoft
    selectedTextColor: Theme.text
    selectByMouse: true

    background: Rectangle {
        radius: Theme.radiusSm + 2
        color: Theme.surface
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
        Icon {
            visible: control.iconName.length > 0
            name: control.iconName
            color: Theme.textFaint
            size: 16
            x: 12
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
