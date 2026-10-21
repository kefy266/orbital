import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Non-intrusive sponsor banner rendered inline inside the chat transcript.
// 100% isolated to UI; does not interact with LLM prompt context.
Rectangle {
    id: root
    property int adIndex: 0
    implicitHeight: 46
    radius: Theme.radiusSm + 2
    color: Theme.surfaceAlt
    border.width: 1
    border.color: mouseArea.containsMouse ? Theme.accent : Theme.border
    clip: true

    Behavior on border.color { ColorAnimation { duration: Theme.fast } }

    readonly property var campaigns: [
        {
            "id": "blufy",
            "name": "Blufy",
            "tag": "Ücretsiz MP3/4 İndirici",
            "desc": "YouTube ve medyalar için hızlı, reklamsız indirme aracı",
            "domain": "blufy.oedge.xyz",
            "url": "https://blufy.oedge.xyz",
            "icon": "download"
        },
        {
            "id": "convert",
            "name": "EdgeConvert",
            "tag": "Dosya Dönüştürücü",
            "desc": "Video, ses ve belgeleri anında farklı formatlara çevirin",
            "domain": "convert.oedge.xyz",
            "url": "https://convert.oedge.xyz",
            "icon": "refresh"
        },
        {
            "id": "hizmet",
            "name": "Origin Edge Hizmet",
            "tag": "Web & Tasarım Çözümleri",
            "desc": "Güvenilir web barındırma, sunucu ve modern web tasarım hizmeti",
            "domain": "hizmet.oedge.xyz",
            "url": "https://hizmet.oedge.xyz",
            "icon": "globe"
        }
    ]

    readonly property var ad: campaigns[Math.abs(adIndex) % campaigns.length]

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (root.ad && root.ad.url)
                Qt.openUrlExternally(root.ad.url)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Rectangle {
            width: 26
            height: 26
            radius: 6
            color: Theme.accentSoft
            Icon {
                anchors.centerIn: parent
                name: root.ad.icon || "globe"
                size: 13
                color: Theme.accent
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Rectangle {
                    implicitHeight: 15
                    implicitWidth: sponsorTag.implicitWidth + 8
                    radius: 3
                    color: Theme.accentSoft
                    Text {
                        id: sponsorTag
                        anchors.centerIn: parent
                        text: "SPONSOR"
                        font.family: Theme.font
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: Theme.accent
                    }
                }

                Text {
                    text: root.ad.name
                    font.family: Theme.font
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.DemiBold
                    color: Theme.text
                }

                Text {
                    text: "•  " + root.ad.tag
                    font.family: Theme.font
                    font.pixelSize: Theme.fontCaption
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Text {
                text: root.ad.desc + " — " + root.ad.domain
                font.family: Theme.font
                font.pixelSize: Theme.fontCaption - 1
                color: Theme.textFaint
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Row {
            spacing: 4
            Text {
                text: "Keşfet"
                font.family: Theme.font
                font.pixelSize: Theme.fontSmall
                font.weight: Font.Medium
                color: mouseArea.containsMouse ? Theme.accent : Theme.textMuted
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                name: "external"
                size: 13
                color: mouseArea.containsMouse ? Theme.accent : Theme.textMuted
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
