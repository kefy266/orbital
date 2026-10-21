import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orbital

// Shown whenever the model / engine asks interactive clarification questions.
Popup {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 40 : 540, 560)
    modal: true
    padding: 24
    closePolicy: Popup.NoAutoClose
    visible: App.question.questions !== undefined && App.question.questions.length > 0

    Overlay.modal: Rectangle { color: Theme.overlay }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.elevated
        border.width: 1
        border.color: Theme.border
    }

    property var userSelections: ({})
    property var customInputs: ({})

    function resetState() {
        var sel = {};
        var inp = {};
        var qs = App.question.questions || [];
        for (var i = 0; i < qs.length; ++i) {
            sel[i] = [];
            inp[i] = "";
        }
        userSelections = sel;
        customInputs = inp;
    }

    Connections {
        target: App
        function onQuestionChanged() {
            dialog.resetState();
        }
    }

    function isSelected(qIdx, label) {
        var list = userSelections[qIdx];
        return list ? list.indexOf(label) >= 0 : false;
    }

    function toggleOption(qIdx, label, isMultiple) {
        var copy = Object.assign({}, userSelections);
        var list = copy[qIdx] ? copy[qIdx].slice() : [];
        if (isMultiple) {
            var pos = list.indexOf(label);
            if (pos >= 0)
                list.splice(pos, 1);
            else
                list.push(label);
        } else {
            list = [label];
            // Clear custom input if user chose a predefined option
            setCustomText(qIdx, "");
        }
        copy[qIdx] = list;
        userSelections = copy;
    }

    function setCustomText(qIdx, text) {
        var copy = Object.assign({}, customInputs);
        copy[qIdx] = text;
        customInputs = copy;
    }

    function canSubmit() {
        var qs = App.question.questions || [];
        if (qs.length === 0)
            return false;
        for (var i = 0; i < qs.length; ++i) {
            var sel = userSelections[i] || [];
            var custom = (customInputs[i] || "").trim();
            if (sel.length === 0 && custom.length === 0)
                return false;
        }
        return true;
    }

    function submit() {
        var qs = App.question.questions || [];
        var result = [];
        for (var i = 0; i < qs.length; ++i) {
            var qAnswers = (userSelections[i] || []).slice();
            var custom = (customInputs[i] || "").trim();
            if (custom.length > 0) {
                if (qs[i].multiple) {
                    if (qAnswers.indexOf(custom) < 0)
                        qAnswers.push(custom);
                } else {
                    qAnswers = [custom];
                }
            }
            result.push(qAnswers);
        }
        App.replyQuestion(result);
    }

    contentItem: ColumnLayout {
        spacing: 16

        // Header
        RowLayout {
            spacing: 14
            Layout.fillWidth: true

            Rectangle {
                width: 40
                height: 40
                radius: 10
                color: Theme.accentSoft
                Icon {
                    anchors.centerIn: parent
                    name: "sparkle"
                    size: 20
                    color: Theme.accent
                }
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text {
                    text: "Bilgi gerekiyor"
                    font.family: Theme.font
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    text: "Asistan devam edebilmek için tercihinizi öğrenmek istiyor."
                    font.family: Theme.font
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textMuted
                }
            }
        }

        // Question List in ScrollView
        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            implicitHeight: Math.min(qColumn.implicitHeight, (dialog.parent ? dialog.parent.height - 220 : 440))
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: qColumn
                width: scrollView.availableWidth
                spacing: 20

                Repeater {
                    model: App.question.questions || []

                    delegate: ColumnLayout {
                        id: questionCard
                        required property var modelData
                        required property int index

                        readonly property int qIndex: index
                        readonly property var qData: modelData
                        readonly property bool isMultiple: qData.multiple === true

                        Layout.fillWidth: true
                        spacing: 8

                        // Question Header / Category
                        RowLayout {
                            spacing: 8
                            Layout.fillWidth: true

                            Rectangle {
                                visible: (qData.header || "").length > 0
                                implicitWidth: headerLabel.implicitWidth + 12
                                implicitHeight: 22
                                radius: 6
                                color: Theme.accentSoft
                                Text {
                                    id: headerLabel
                                    anchors.centerIn: parent
                                    text: qData.header || ""
                                    font.family: Theme.font
                                    font.pixelSize: Theme.fontCaption
                                    font.weight: Font.DemiBold
                                    color: Theme.accent
                                }
                            }

                            Text {
                                text: isMultiple ? "Birden fazla seçilebilir" : "Tek seçenek"
                                font.family: Theme.font
                                font.pixelSize: Theme.fontCaption
                                color: Theme.textFaint
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // Question Prompt
                        Text {
                            Layout.fillWidth: true
                            text: qData.question || ""
                            font.family: Theme.font
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            color: Theme.text
                            wrapMode: Text.WordWrap
                        }

                        // Options list
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: qData.options || []

                                delegate: Rectangle {
                                    id: optionCard
                                    required property var modelData
                                    required property int index

                                    readonly property string optLabel: modelData.label || ""
                                    readonly property string optDesc: modelData.description || ""
                                    readonly property bool picked: dialog.isSelected(questionCard.qIndex, optLabel)

                                    Layout.fillWidth: true
                                    implicitHeight: contentRow.implicitHeight + 16
                                    radius: Theme.radiusSm + 2
                                    color: picked ? Theme.accentSoft : (hoverArea.containsMouse ? Theme.hover : Theme.surface)
                                    border.width: picked ? 2 : 1
                                    border.color: picked ? Theme.accent : Theme.border

                                    Behavior on color { ColorAnimation { duration: Theme.fast } }
                                    Behavior on border.color { ColorAnimation { duration: Theme.fast } }

                                    MouseArea {
                                        id: hoverArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: dialog.toggleOption(questionCard.qIndex, optionCard.optLabel, questionCard.isMultiple)
                                    }

                                    RowLayout {
                                        id: contentRow
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 12

                                        // Indicator: Radio or Checkbox
                                        Rectangle {
                                            width: 18
                                            height: 18
                                            radius: questionCard.isMultiple ? 4 : 9
                                            color: optionCard.picked ? Theme.accent : "transparent"
                                            border.width: optionCard.picked ? 0 : 1.5
                                            border.color: optionCard.picked ? Theme.accent : Theme.borderStrong

                                            // Check icon for multi-select
                                            Icon {
                                                visible: questionCard.isMultiple && optionCard.picked
                                                name: "check"
                                                size: 12
                                                color: Theme.textOnAccent
                                                anchors.centerIn: parent
                                            }

                                            // Dot for single radio
                                            Rectangle {
                                                visible: !questionCard.isMultiple && optionCard.picked
                                                width: 6
                                                height: 6
                                                radius: 3
                                                color: Theme.textOnAccent
                                                anchors.centerIn: parent
                                            }
                                        }

                                        // Text Content
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                Layout.fillWidth: true
                                                text: optionCard.optLabel
                                                font.family: Theme.font
                                                font.pixelSize: Theme.fontUi
                                                font.weight: optionCard.picked ? Font.DemiBold : Font.Normal
                                                color: Theme.text
                                                wrapMode: Text.WordWrap
                                            }

                                            Text {
                                                visible: optionCard.optDesc.length > 0
                                                Layout.fillWidth: true
                                                text: optionCard.optDesc
                                                font.family: Theme.font
                                                font.pixelSize: Theme.fontSmall
                                                color: Theme.textMuted
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Write-in custom option text field
                        OTextField {
                            id: customInput
                            Layout.fillWidth: true
                            iconName: "pencil"
                            placeholderText: "Farklı bir yanıt yazın (isteğe bağlı)..."
                            text: dialog.customInputs[questionCard.qIndex] || ""
                            onTextChanged: {
                                dialog.setCustomText(questionCard.qIndex, text);
                                if (text.trim().length > 0 && !questionCard.isMultiple) {
                                    // Clear radio selection if user chooses custom
                                    var copy = Object.assign({}, dialog.userSelections);
                                    copy[questionCard.qIndex] = [];
                                    dialog.userSelections = copy;
                                }
                            }
                            onAccepted: {
                                if (dialog.canSubmit())
                                    dialog.submit();
                            }
                        }
                    }
                }
            }
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            OButton {
                text: "Reddet"
                variant: "danger"
                onClicked: App.rejectQuestion()
            }

            Item { Layout.fillWidth: true }

            OButton {
                text: "Yanıtla"
                variant: "primary"
                enabled: dialog.canSubmit()
                focus: true
                onClicked: dialog.submit()
            }
        }
    }
}
