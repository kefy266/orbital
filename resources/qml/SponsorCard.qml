import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Non-intrusive, native sponsor card for developer services.
// Strictly bounded to prevent any horizontal or vertical overflow.
Rectangle {
    id: root
    implicitHeight: 52
    Layout.fillWidth: true
    radius: Theme.radiusSm + 2
    color: Theme.surfaceAlt
    border.width: 1
    border.color: mouseArea.containsMouse ? Theme.accent : Theme.border
    clip: true

    Behavior on border.color { ColorAnimation { duration: Theme.fast } }

    property int currentIndex: 0
    property var campaigns: [
        {
            "id": "blufy",
            "name": "Blufy",
            "tag": "Ücretsiz MP3/4 İndirici",
            "desc": "blufy.oedge.xyz • Hızlı medya indirme",
            "url": "https://blufy.oedge.xyz",
            "icon": "download"
        },
        {
            "id": "convert",
            "name": "EdgeConvert",
            "tag": "Dosya Dönüştürücü",
            "desc": "convert.oedge.xyz • Medya dönüştürme",
            "url": "https://convert.oedge.xyz",
            "icon": "refresh"
        },
        {
            "id": "hizmet",
            "name": "Origin Edge Hizmet",
            "tag": "Web & Tasarım",
            "desc": "hizmet.oedge.xyz • Hosting & Web Çözümleri",
            "url": "https://hizmet.oedge.xyz",
            "icon": "globe"
        }
    ]

    readonly property var currentItem: campaigns[currentIndex] || campaigns[0]

    Timer {
        id: rotateTimer
        interval: 16000
        repeat: true
        running: !mouseArea.containsMouse && root.campaigns.length > 1
        onTriggered: {
            fadeAnim.restart()
            root.currentIndex = (root.currentIndex + 1) % root.campaigns.length
        }
    }

    SequentialAnimation {
        id: fadeAnim
        NumberAnimation { target: contentBox; property: "opacity"; to: 0.2; duration: 100 }
        NumberAnimation { target: contentBox; property: "opacity"; to: 1.0; duration: 150 }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (root.currentItem && root.currentItem.url)
                Qt.openUrlExternally(root.currentItem.url)
        }
    }

    RowLayout {
        id: contentBox
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8

        Rectangle {
            width: 24
            height: 24
            radius: 6
            color: Theme.accentSoft
            Icon {
                anchors.centerIn: parent
                name: root.currentItem.icon || "globe"
                size: 13
                color: Theme.accent
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            clip: true

            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                Text {
                    text: root.currentItem.name
                    font.family: Theme.font
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.DemiBold
                    color: Theme.text
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    implicitHeight: 14
                    implicitWidth: tagText.implicitWidth + 6
                    radius: 3
                    color: Theme.hover
                    Text {
                        id: tagText
                        anchors.centerIn: parent
                        text: "SPONSOR"
                        font.family: Theme.font
                        font.pixelSize: 8
                        font.weight: Font.Bold
                        color: Theme.textMuted
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.currentItem.desc
                font.family: Theme.font
                font.pixelSize: Theme.fontCaption - 1
                color: Theme.textMuted
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        Icon {
            name: "external"
            size: 13
            color: mouseArea.containsMouse ? Theme.accent : Theme.textFaint
        }
    }
}
