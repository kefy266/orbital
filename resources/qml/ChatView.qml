import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Transcript. Items come from App.chat (ChatModel); each row picks a component by kind.
ListView {
    id: view
    property bool stick: true
    readonly property real column: Math.min(Theme.chatWidth, width - 48)

    model: App.chat
    spacing: 18
    topMargin: 28
    bottomMargin: 28
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    reuseItems: false
    cacheBuffer: 2000
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    // Follow new output while the user is at the bottom.
    onMovementEnded: stick = atYEnd
    onContentHeightChanged: if (stick) Qt.callLater(positionViewAtEnd)
    onCountChanged: if (stick) Qt.callLater(positionViewAtEnd)
    Connections {
        target: App
        function onScrollToEnd() { view.stick = true; Qt.callLater(view.positionViewAtEnd) }
    }

    delegate: Item {
        id: row
        required property int index
        required property string kind
        required property string text
        required property var segments
        required property var skills
        required property string toolTitle
        required property string toolSubtitle
        required property string toolStatus
        required property string toolDetails
        required property string toolIcon
        required property bool lastInTurn

        width: view.width
        height: loader.item ? loader.item.implicitHeight : 0

        Loader {
            id: loader
            width: view.column
            x: (view.width - width) / 2
            sourceComponent: row.kind === "user" ? userMsg
                           : row.kind === "text" ? textMsg
                           : row.kind === "reasoning" ? reasoningMsg
                           : row.kind === "tool" ? toolMsg
                           : errorMsg
        }

        Component {
            id: userMsg
            Column {
                spacing: 6
                Flow {
                    width: parent.width
                    layoutDirection: Qt.RightToLeft
                    spacing: 6
                    visible: row.skills.length > 0
                    Repeater {
                        model: row.skills
                        delegate: SkillChip { required property string modelData; name: modelData }
                    }
                }
                Item {
                    width: parent.width
                    height: bubble.height
                    Rectangle {
                        id: bubble
                        anchors.right: parent.right
                        width: Math.min(parent.width * 0.8, userText.implicitWidth + 32)
                        height: userText.implicitHeight + 22
                        radius: Theme.radiusLg
                        color: Theme.userBubble
                        TextEdit {
                            id: userText
                            anchors.fill: parent
                            anchors.margins: 11
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            text: row.text
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            font.family: Theme.font
                            font.pixelSize: Theme.fontBody
                            color: Theme.text
                            selectionColor: Theme.accentSoft
                            selectedTextColor: Theme.text
                        }
                    }
                }
            }
        }

        Component {
            id: textMsg
            Column {
                spacing: 14
                Repeater {
                    model: row.segments
                    delegate: Loader {
                        required property var modelData
                        width: parent.width
                        sourceComponent: modelData.type === "code" ? codeSeg : proseSeg
                        Component {
                            id: proseSeg
                            TextEdit {
                                text: modelData.text
                                textFormat: TextEdit.MarkdownText
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.Wrap
                                font.family: Theme.font
                                font.pixelSize: Theme.fontBody
                                color: Theme.text
                                selectionColor: Theme.accentSoft
                                selectedTextColor: Theme.text
                                onLinkActivated: link => Qt.openUrlExternally(link)
                                HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor }
                            }
                        }
                        Component {
                            id: codeSeg
                            CodeBlock { code: modelData.text; lang: modelData.lang }
                        }
                    }
                }
                // Turn actions
                Row {
                    visible: row.lastInTurn && !(App.busy && row.index === view.count - 1)
                    spacing: 2
                    IconButton {
                        implicitWidth: 28; implicitHeight: 28; iconSize: 15
                        iconName: "copy"; tip: "Yanıtı kopyala"
                        onClicked: App.copyText(App.chat.turnText(row.index))
                    }
                }
            }
        }

        Component {
            id: reasoningMsg
            Column {
                id: rcol
                property bool open: false
                spacing: 8
                AbstractButton {
                    id: rhead
                    hoverEnabled: true
                    implicitHeight: 28
                    implicitWidth: rrow.implicitWidth + 8
                    onClicked: rcol.open = !rcol.open
                    background: Item {}
                    contentItem: Row {
                        id: rrow
                        spacing: 6
                        Icon { name: "bulb"; size: 15; color: Theme.textFaint; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: "Düşünce süreci"
                            font.family: Theme.font
                            font.pixelSize: Theme.fontSmall
                            color: rhead.hovered ? Theme.textMuted : Theme.textFaint
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Icon {
                            name: "chevron-right"; size: 14; color: Theme.textFaint
                            rotation: rcol.open ? 90 : 0
                            Behavior on rotation { NumberAnimation { duration: Theme.fast } }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
                Rectangle {
                    visible: rcol.open
                    width: parent.width
                    height: rtext.implicitHeight
                    color: "transparent"
                    Rectangle { width: 2; height: parent.height; radius: 1; color: Theme.border }
                    Text {
                        id: rtext
                        x: 14
                        width: parent.width - 14
                        text: row.text.trim()
                        wrapMode: Text.Wrap
                        font.family: Theme.font
                        font.pixelSize: Theme.fontSmall + 1
                        font.italic: true
                        lineHeight: 1.35
                        color: Theme.textMuted
                    }
                }
            }
        }

        Component {
            id: toolMsg
            Rectangle {
                id: tcard
                property bool open: false
                implicitHeight: tcol.implicitHeight
                radius: Theme.radius
                color: thead.hovered && !open ? Theme.hover : "transparent"
                border.width: 1
                border.color: row.toolStatus === "error" ? Theme.danger : Theme.border
                Behavior on color { ColorAnimation { duration: Theme.fast } }

                Column {
                    id: tcol
                    width: parent.width
                    AbstractButton {
                        id: thead
                        width: parent.width
                        implicitHeight: 44
                        hoverEnabled: true
                        onClicked: tcard.open = !tcard.open
                        background: Item {}
                        contentItem: RowLayout {
                            spacing: 10
                            Rectangle {
                                Layout.leftMargin: 10
                                width: 26; height: 26; radius: 7
                                color: Theme.accentSoft
                                Icon { anchors.centerIn: parent; name: row.toolIcon; size: 15; color: Theme.accent }
                            }
                            Text {
                                text: row.toolTitle
                                font.family: Theme.font
                                font.pixelSize: Theme.fontUi
                                font.weight: Font.Medium
                                color: Theme.text
                            }
                            Text {
                                text: row.toolSubtitle
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                                font.family: Theme.mono
                                font.pixelSize: Theme.fontCaption + 1
                                color: Theme.textFaint
                            }
                            Item {
                                width: 18; height: 18
                                Spinner { anchors.fill: parent; visible: row.toolStatus !== "completed" && row.toolStatus !== "error" }
                                Icon {
                                    anchors.centerIn: parent
                                    visible: row.toolStatus === "completed" || row.toolStatus === "error"
                                    name: row.toolStatus === "error" ? "alert" : "check"
                                    size: 16
                                    color: row.toolStatus === "error" ? Theme.danger : Theme.success
                                }
                            }
                            Icon {
                                Layout.rightMargin: 12
                                name: "chevron-down"; size: 14; color: Theme.textFaint
                                rotation: tcard.open ? 180 : 0
                                Behavior on rotation { NumberAnimation { duration: Theme.fast } }
                            }
                        }
                    }
                    Rectangle {
                        visible: tcard.open
                        width: parent.width - 16
                        x: 8
                        height: Math.min(details.implicitHeight, 320)
                        radius: Theme.radiusSm
                        color: Theme.codeBg
                        clip: true
                        Flickable {
                            anchors.fill: parent
                            contentHeight: details.implicitHeight
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            TextEdit {
                                id: details
                                width: parent.width
                                text: row.toolDetails
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.WrapAnywhere
                                font.family: Theme.mono
                                font.pixelSize: 13
                                color: Theme.textMuted
                                padding: 12
                            }
                        }
                    }
                    Item { visible: tcard.open; width: 1; height: 8 }
                }
            }
        }

        Component {
            id: errorMsg
            Rectangle {
                implicitHeight: erow.implicitHeight + 24
                radius: Theme.radius
                color: Theme.dangerSoft
                border.width: 1
                border.color: Qt.alpha(Theme.danger, 0.4)
                RowLayout {
                    id: erow
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    Icon { Layout.alignment: Qt.AlignTop; name: "alert"; size: 17; color: Theme.danger }
                    TextEdit {
                        Layout.fillWidth: true
                        text: row.text
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        font.family: Theme.font
                        font.pixelSize: Theme.fontUi
                        color: Theme.danger
                    }
                }
            }
        }
    }

    footer: Item {
        width: view.width
        height: App.thinking ? 44 : 0
        Row {
            visible: App.thinking
            x: (view.width - view.column) / 2
            y: 14
            spacing: 6
            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    width: 8; height: 8; radius: 4
                    color: Theme.accent
                    SequentialAnimation on opacity {
                        running: App.thinking
                        loops: Animation.Infinite
                        PauseAnimation { duration: index * 160 }
                        NumberAnimation { from: 0.25; to: 1; duration: 380; easing.type: Easing.OutQuad }
                        NumberAnimation { from: 1; to: 0.25; duration: 380; easing.type: Easing.InQuad }
                        PauseAnimation { duration: (2 - index) * 160 }
                    }
                }
            }
        }
    }
}
