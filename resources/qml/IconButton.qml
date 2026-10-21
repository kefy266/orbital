import QtQuick
import QtQuick.Controls.Basic
import Orbital

// Square, borderless icon button with a soft hover background and tooltip.
AbstractButton {
    id: control
    property string iconName: ""
    property color iconColor: Theme.textMuted
    property int iconSize: 18
    property string tip: ""
    property bool active: false

    implicitWidth: 32
    implicitHeight: 32
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.delay: 600
    ToolTip.text: tip

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.enabled ? (control.hovered || control.active ? Theme.text : control.iconColor) : Theme.textFaint
        }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.pressed ? Theme.selected : (control.hovered || control.active ? Theme.hover : "transparent")
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
}
