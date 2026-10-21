import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import org.kde.syntaxhighlighting
import Orbital

// Fenced code: header with language + copy, highlighted body (KSyntaxHighlighting).
Rectangle {
    id: block
    property string code
    property string lang

    implicitHeight: column.implicitHeight
    radius: Theme.radius
    color: Theme.codeBg
    border.width: 1
    border.color: Theme.border
    clip: true

    readonly property var aliases: ({
        "js": "JavaScript", "javascript": "JavaScript", "jsx": "JavaScript React (JSX)", "ts": "TypeScript",
        "typescript": "TypeScript", "tsx": "TypeScript React (TSX)", "py": "Python", "python": "Python",
        "sh": "Bash", "bash": "Bash", "shell": "Bash", "zsh": "Zsh", "console": "Bash", "fish": "Fish",
        "c": "C", "cpp": "C++", "c++": "C++", "cc": "C++", "h": "C++", "hpp": "C++", "rs": "Rust", "rust": "Rust",
        "go": "Go", "json": "JSON", "yaml": "YAML", "yml": "YAML", "toml": "TOML", "html": "HTML", "xml": "XML",
        "css": "CSS", "scss": "SCSS", "md": "Markdown", "markdown": "Markdown", "java": "Java", "kt": "Kotlin",
        "kotlin": "Kotlin", "cs": "C#", "csharp": "C#", "rb": "Ruby", "ruby": "Ruby", "php": "PHP", "sql": "SQL",
        "lua": "Lua", "diff": "Diff", "patch": "Diff", "cmake": "CMake", "qml": "QML", "dockerfile": "Dockerfile",
        "ini": "INI Files", "swift": "Swift", "dart": "Dart", "make": "Makefile", "makefile": "Makefile",
        "ps1": "PowerShell", "powershell": "PowerShell", "vue": "Vue", "svelte": "Svelte"
    })

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.leftMargin: 14
            Layout.rightMargin: 6
            Text {
                text: block.lang.length ? block.lang : "kod"
                Layout.fillWidth: true
                font.family: Theme.mono
                font.pixelSize: Theme.fontCaption
                color: Theme.textFaint
            }
            AbstractButton {
                id: copy
                property bool done: false
                implicitHeight: 28
                implicitWidth: copyRow.implicitWidth + 16
                hoverEnabled: true
                onClicked: {
                    App.copyText(block.code)
                    done = true
                    resetTimer.restart()
                }
                Timer { id: resetTimer; interval: 1600; onTriggered: copy.done = false }
                background: Rectangle { radius: Theme.radiusSm; color: copy.hovered ? Theme.hover : "transparent" }
                contentItem: Row {
                    id: copyRow
                    spacing: 6
                    leftPadding: 8
                    Icon { name: copy.done ? "check" : "copy"; size: 14; color: copy.done ? Theme.success : Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: copy.done ? "Kopyalandı" : "Kopyala"
                        font.family: Theme.font
                        font.pixelSize: Theme.fontCaption
                        color: Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.preferredHeight: edit.implicitHeight
            contentWidth: Math.max(width, edit.implicitWidth)
            contentHeight: edit.implicitHeight
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

            TextEdit {
                id: edit
                text: block.code
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                font.family: Theme.mono
                font.pixelSize: 14
                color: Theme.text
                selectionColor: Theme.accentSoft
                selectedTextColor: Theme.text
                padding: 14
                bottomPadding: 16
                width: Math.max(flick.width, implicitWidth)
            }
        }
    }

    SyntaxHighlighter {
        textEdit: edit
        definition: block.aliases[block.lang.toLowerCase()] || block.lang || "None"
        theme: Repository.defaultTheme(Theme.dark ? Repository.DarkTheme : Repository.LightTheme)
    }
}
