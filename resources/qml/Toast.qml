import QtQuick
import Orbital

// Bottom-centred transient message.
Rectangle {
    id: toast
    function show(message) {
        label.text = message
        opacity = 1
        hideTimer.restart()
    }

    width: Math.min(label.implicitWidth + 36, parent.width - 48)
    height: 40
    radius: 20
    color: Theme.text
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.normal } }

    Text {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, toast.parent.width - 84)
        elide: Text.ElideRight
        font.family: Theme.font
        font.pixelSize: Theme.fontSmall
        color: Theme.bg
    }
    Timer { id: hideTimer; interval: 3200; onTriggered: toast.opacity = 0 }
}
