import QtQuick
import QtQuick.Controls.Basic
import Orbital

Switch {
    id: control
    implicitWidth: 40
    implicitHeight: 24
    padding: 0

    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 24
        radius: 12
        color: control.checked ? Theme.accent : Theme.selected
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accentHover
        Behavior on color { ColorAnimation { duration: Theme.fast } }
        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: "#ffffff"
            Behavior on x { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
        }
    }
    contentItem: Item {}
}
