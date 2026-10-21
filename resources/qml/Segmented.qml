import QtQuick
import Orbital

// Segmented control. model: [{value, label, icon?}]
Rectangle {
    id: root
    property var model: []
    property string value: ""
    signal picked(string value)

    implicitHeight: 34
    implicitWidth: row.implicitWidth + 6
    radius: Theme.radiusSm + 2
    color: Theme.surfaceAlt
    border.width: 1
    border.color: Theme.border

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 2
        Repeater {
            model: root.model
            delegate: Rectangle {
                required property var modelData
                readonly property bool on: root.value === modelData.value
                width: content.implicitWidth + 22
                height: root.height - 6
                radius: Theme.radiusSm
                color: on ? Theme.elevated : (area.containsMouse ? Theme.hover : "transparent")
                border.width: on ? 1 : 0
                border.color: Theme.border
                Behavior on color { ColorAnimation { duration: Theme.fast } }
                Row {
                    id: content
                    anchors.centerIn: parent
                    spacing: 6
                    Icon {
                        visible: !!modelData.icon
                        name: modelData.icon || ""
                        size: 15
                        color: on ? Theme.text : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData.label
                        font.family: Theme.font
                        font.pixelSize: Theme.fontSmall
                        font.weight: on ? Font.DemiBold : Font.Normal
                        color: on ? Theme.text : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (!on) root.picked(modelData.value)
                }
            }
        }
    }
}
