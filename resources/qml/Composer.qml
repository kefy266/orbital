import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import Orbital

// Prompt card: growing text area, "/" completion, attached skills, +menu,
// Build/Plan toggle and the send/stop button.
Item {
    id: root
    property bool large: false            // welcome layout: taller, more inviting
    readonly property bool canSend: input.text.trim().length > 0 && !App.busy
    implicitHeight: card.height

    property int historyIndex: -1
    property string currentDraft: ""

    function focusInput() { input.forceActiveFocus() }
    function setText(t) { input.text = t; input.cursorPosition = t.length; input.forceActiveFocus() }

    function send() {
        if (slash.visible && slash.count > 0) { slash.accept(); return }
        const t = input.text.trim()
        if (!t.length || App.busy)
            return
        historyIndex = -1
        currentDraft = ""
        App.submit(t)
        input.text = ""
    }

    RectangularShadow {
        anchors.fill: card
        radius: card.radius
        blur: 24
        spread: -4
        offset.y: 6
        color: Qt.alpha("#000000", Theme.dark ? 0.35 : 0.08)
    }

    Rectangle {
        id: card
        width: parent.width
        height: column.implicitHeight + 20
        radius: 24
        color: Theme.surface
        border.width: dropArea.containsDrag ? 2 : 1
        border.color: dropArea.containsDrag ? Theme.accent : (input.activeFocus ? Theme.borderStrong : Theme.border)
        Behavior on border.color { ColorAnimation { duration: Theme.fast } }

        DropArea {
            id: dropArea
            anchors.fill: parent
            onDropped: drop => {
                if (drop.hasUrls && drop.urls.length > 0) {
                    var formatted = []
                    for (var i = 0; i < drop.urls.length; ++i) {
                        formatted.push(App.formatDroppedUrl(drop.urls[i]))
                    }
                    if (formatted.length > 0) {
                        if (input.text.length > 0 && !input.text.endsWith(" "))
                            input.text += " "
                        input.text += formatted.join(" ")
                        input.cursorPosition = input.text.length
                        input.forceActiveFocus()
                    }
                    drop.acceptProposedAction()
                }
            }
        }

        ColumnLayout {
            id: column
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 6

            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.topMargin: 4
                spacing: 6
                visible: App.attachedSkills.length > 0
                Repeater {
                    model: App.attachedSkills
                    delegate: SkillChip {
                        required property string modelData
                        name: modelData
                        removable: true
                        onRemoved: App.detachSkill(modelData)
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(Math.max(input.implicitHeight, root.large ? 64 : 28), 240)
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                TextArea {
                    id: input
                    placeholderText: App.engineReady ? "Orbital'e bir şey sor…" : "Orbital'e bir şey sor…"
                    placeholderTextColor: Theme.textFaint
                    wrapMode: TextArea.Wrap
                    font.family: Theme.font
                    font.pixelSize: Theme.fontBody
                    color: Theme.text
                    selectionColor: Theme.accentSoft
                    selectedTextColor: Theme.text
                    leftPadding: 6
                    rightPadding: 6
                    topPadding: 6
                    bottomPadding: 4
                    background: null
                    focus: true

                    Keys.onPressed: event => {
                        if (slash.visible) {
                            if (event.key === Qt.Key_Up) { slash.move(-1); event.accepted = true; return }
                            if (event.key === Qt.Key_Down) { slash.move(1); event.accepted = true; return }
                            if (event.key === Qt.Key_Tab) { slash.accept(); event.accepted = true; return }
                            if (event.key === Qt.Key_Escape) { slash.close(); event.accepted = true; return }
                        } else {
                            if (event.key === Qt.Key_Up && (input.cursorPosition === 0 || input.text.length === 0)) {
                                if (App.promptHistory && App.promptHistory.length > 0) {
                                    if (root.historyIndex === -1) {
                                        root.currentDraft = input.text
                                        root.historyIndex = App.promptHistory.length - 1
                                    } else if (root.historyIndex > 0) {
                                        root.historyIndex--
                                    }
                                    input.text = App.promptHistory[root.historyIndex]
                                    input.cursorPosition = input.text.length
                                    event.accepted = true
                                    return
                                }
                            } else if (event.key === Qt.Key_Down && root.historyIndex !== -1) {
                                if (root.historyIndex < App.promptHistory.length - 1) {
                                    root.historyIndex++
                                    input.text = App.promptHistory[root.historyIndex]
                                } else {
                                    root.historyIndex = -1
                                    input.text = root.currentDraft
                                }
                                input.cursorPosition = input.text.length
                                event.accepted = true
                                return
                            }
                        }
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                && !(event.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                            root.send()
                            event.accepted = true
                        }
                    }
                    onTextChanged: slash.update(text)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                IconButton {
                    id: plus
                    iconName: "plus"
                    tip: "Skill iliştir, eylem çalıştır"
                    implicitWidth: 34
                    implicitHeight: 34
                    background: Rectangle {
                        radius: 17
                        color: plus.hovered || plusMenu.visible ? Theme.hover : "transparent"
                        border.width: 1
                        border.color: Theme.border
                    }
                    onClicked: plusMenu.popup(plus, 0, -plusMenu.implicitHeight - 6)
                    OMenu {
                        id: plusMenu
                        implicitWidth: 300
                        Instantiator {
                            model: App.commands.filter(c => c.source === "skill")
                            delegate: OMenuItem {
                                required property var modelData
                                text: modelData.name
                                subtitle: modelData.description
                                iconName: "sparkle"
                                selectedMark: App.attachedSkills.indexOf(modelData.name) >= 0
                                onTriggered: selectedMark ? App.detachSkill(modelData.name) : App.attachSkill(modelData.name)
                            }
                            onObjectAdded: (index, object) => plusMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => plusMenu.removeItem(object)
                        }
                        MenuSeparator {
                            visible: App.actions.length > 0
                            contentItem: Rectangle { implicitHeight: 1; color: Theme.border }
                        }
                        Instantiator {
                            model: App.actions
                            delegate: OMenuItem {
                                required property var modelData
                                required property int index
                                text: modelData.label
                                subtitle: modelData.run
                                iconName: "terminal"
                                onTriggered: App.runAction(index)
                            }
                            onObjectAdded: (index, object) => plusMenu.addItem(object)
                            onObjectRemoved: (index, object) => plusMenu.removeItem(object)
                        }
                    }
                }

                Segmented {
                    implicitHeight: 30
                    visible: App.agents.length > 1
                    model: App.agents.map(a => ({ value: a.id,
                                                  label: a.id === "build" ? "Build" : a.id === "plan" ? "Plan" : a.id }))
                    value: App.agent
                    onPicked: v => App.agent = v
                    ToolTip.visible: hovered
                    property bool hovered: false
                    HoverHandler { onHoveredChanged: parent.hovered = hovered }
                    ToolTip.delay: 700
                    ToolTip.text: "Build: dosya düzenler, komut çalıştırır · Plan: yalnızca okur ve plan çıkarır"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: App.usage
                    visible: text.length > 0 && !root.large
                    font.family: Theme.font
                    font.pixelSize: Theme.fontCaption
                    color: Theme.textFaint
                }

                AbstractButton {
                    id: sendButton
                    implicitWidth: 34
                    implicitHeight: 34
                    enabled: App.busy || root.canSend
                    hoverEnabled: true
                    onClicked: App.busy ? App.stop() : root.send()
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: App.busy ? "Durdur (Esc)" : "Gönder (Enter)"
                    background: Rectangle {
                        radius: 17
                        color: App.busy ? Theme.text
                              : root.canSend ? (sendButton.hovered ? Theme.accentHover : Theme.accent) : Theme.selected
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                    }
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: App.busy ? "stop" : "arrow-up"
                            size: App.busy ? 16 : 18
                            color: App.busy ? Theme.bg : (root.canSend ? Theme.textOnAccent : Theme.textFaint)
                        }
                    }
                }
            }
        }
    }

    // "/" completion for commands and skills
    Popup {
        id: slash
        property var items: []
        property int current: 0
        readonly property int count: items.length

        function update(text) {
            if (!text.startsWith("/") || text.indexOf(" ") >= 0 || text.indexOf("\n") >= 0) { close(); return }
            const needle = text.substring(1).toLowerCase()
            items = App.commands.filter(c => c.name.toLowerCase().indexOf(needle) >= 0)
            current = 0
            if (items.length) open(); else close()
        }
        function move(d) { current = Math.max(0, Math.min(items.length - 1, current + d)); list.positionViewAtIndex(current, ListView.Contain) }
        function accept() {
            const c = items[current]
            close()
            if (!c) return
            if (c.source === "skill") { App.attachSkill(c.name); input.text = "" }
            else { input.text = "/" + c.name + " "; input.cursorPosition = input.text.length }
            input.forceActiveFocus()
        }

        x: 0
        y: -height - 8
        width: Math.min(root.width, 520)
        height: Math.min(list.contentHeight + 12, 300)
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        focus: false
        background: Rectangle { radius: Theme.radius; color: Theme.elevated; border.width: 1; border.color: Theme.border }

        contentItem: ListView {
            id: list
            clip: true
            model: slash.items
            boundsBehavior: Flickable.StopAtBounds
            delegate: AbstractButton {
                id: item
                required property var modelData
                required property int index
                width: ListView.view.width
                implicitHeight: 46
                hoverEnabled: true
                onClicked: { slash.current = index; slash.accept() }
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: item.index === slash.current ? Theme.selected : (item.hovered ? Theme.hover : "transparent")
                }
                contentItem: RowLayout {
                    spacing: 10
                    Icon {
                        Layout.leftMargin: 8
                        name: item.modelData.source === "skill" ? "sparkle" : item.modelData.source === "plugin" ? "blocks" : "terminal"
                        size: 16
                        color: item.modelData.source === "skill" ? Theme.accent : Theme.textMuted
                    }
                    ColumnLayout {
                        spacing: 1
                        Layout.fillWidth: true
                        Text {
                            text: "/" + item.modelData.name
                            font.family: Theme.font
                            font.pixelSize: Theme.fontUi
                            font.weight: Font.Medium
                            color: Theme.text
                        }
                        Text {
                            text: item.modelData.description
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            font.family: Theme.font
                            font.pixelSize: Theme.fontCaption
                            color: Theme.textFaint
                        }
                    }
                    Text {
                        Layout.rightMargin: 10
                        text: item.modelData.source === "skill" ? "skill" : item.modelData.source === "plugin" ? "eklenti" : "komut"
                        font.family: Theme.font
                        font.pixelSize: 11
                        color: Theme.textFaint
                    }
                }
            }
        }
    }
}
