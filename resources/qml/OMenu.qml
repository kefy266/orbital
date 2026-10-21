import QtQuick
import QtQuick.Controls.Basic
import Orbital

Menu {
    id: menu
    padding: 6
    implicitWidth: 220
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fast } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.fast } }
    background: Rectangle {
        radius: Theme.radius
        color: Theme.elevated
        border.width: 1
        border.color: Theme.border
    }
}
