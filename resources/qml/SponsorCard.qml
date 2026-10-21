import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Non-intrusive, native sponsor card for developer services.
// Cycles smoothly between built-in services and optionally fetches dynamic campaigns.
Rectangle {
    id: root
    implicitHeight: 70
    radius: Theme.radius
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
            "desc": "YouTube ve medyalar için hızlı, reklamsız indirme aracı.",
            "url": "https://blufy.oedge.xyz",
            "icon": "download"
        },
        {
            "id": "convert",
            "name": "EdgeConvert",
            "tag": "Dosya Dönüştürücü",
            "desc": "Video, ses ve belgeleri anında farklı formatlara çevirin.",
            "url": "https://convert.oedge.xyz",
            "icon": "refresh"
        },
        {
            "id": "hizmet",
            "name": "Origin Edge Hizmet",
            "tag": "Web & Tasarım Çözümleri",
            "desc": "Güvenilir web barındırma, sunucu ve modern web tasarım hizmeti.",
            "url": "https://hizmet.oedge.xyz",
            "icon": "globe"
        }
    ]

    readonly property var currentItem: campaigns[currentIndex] || campaigns[0]

    Timer {
        id: rotateTimer
        interval: 18000
        repeat: true
        running: !mouseArea.containsMouse && root.campaigns.length > 1
        onTriggered: {
            fadeAnim.restart()
            root.currentIndex = (root.currentIndex + 1) % root.campaigns.length
        }
    }

    SequentialAnimation {
        id: fadeAnim
        NumberAnimation { target: contentBox; property: "opacity"; to: 0.2; duration: 120; easing.type: Easing.OutQuad }
        NumberAnimation { target: contentBox; property: "opacity"; to: 1.0; duration: 180; easing.type: Easing.InQuad }
    }

    Component.onCompleted: {
        // Optional remote sponsor update (falls back cleanly if offline/unavailable)
        try {
            var xhr = new XMLHttpRequest()
            xhr.open("GET", "https://oedge.xyz/api/orbital_ads.json")
            xhr.onreadystatechange = function() {
                if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                    try {
                        var data = JSON.parse(xhr.responseText)
                        if (Array.isArray(data) && data.length > 0) {
                            root.campaigns = data
                            root.currentIndex = 0
                        }
                    } catch (e) {}
                }
            }
            xhr.send()
        } catch (err) {}
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

    ColumnLayout {
        id: contentBox
        anchors.fill: parent
        anchors.margins: 8
        spacing: 3

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                width: 18
                height: 18
                radius: 4
                color: Theme.accentSoft
                Icon {
                    anchors.centerIn: parent
                    name: root.currentItem.icon || "globe"
                    size: 11
                    color: Theme.accent
                }
            }

            Text {
                text: root.currentItem.name
                font.family: Theme.font
                font.pixelSize: Theme.fontSmall
                font.weight: Font.DemiBold
                color: Theme.text
                elide: Text.ElideRight
            }

            Rectangle {
                implicitHeight: 15
                implicitWidth: tagText.implicitWidth + 8
                radius: 3
                color: Theme.hover
                border.width: 1
                border.color: Theme.border
                Text {
                    id: tagText
                    anchors.centerIn: parent
                    text: root.currentItem.tag
                    font.family: Theme.font
                    font.pixelSize: 9
                    font.weight: Font.Medium
                    color: Theme.textMuted
                }
            }

            Item { Layout.fillWidth: true }

            Icon {
                name: "external"
                size: 12
                color: mouseArea.containsMouse ? Theme.accent : Theme.textFaint
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

        Row {
            Layout.alignment: Qt.AlignRight
            spacing: 4
            Repeater {
                model: root.campaigns.length
                delegate: Rectangle {
                    required property int index
                    width: index === root.currentIndex ? 12 : 4
                    height: 4
                    radius: 2
                    color: index === root.currentIndex ? Theme.accent : Theme.border
                    Behavior on width { NumberAnimation { duration: 150 } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
            }
        }
    }
}
