pragma Singleton
import QtQuick
import Orbital

// Design tokens. Warm neutral palette (light + dark), one violet accent.
// Follows the system colour scheme unless the user pinned light/dark.
QtObject {
    id: theme

    readonly property bool dark: App.themeMode === "dark"
                                 || (App.themeMode === "system" && Application.styleHints.colorScheme === Qt.ColorScheme.Dark)

    // Surfaces
    readonly property color bg:          dark ? "#262624" : "#faf9f5"
    readonly property color sidebar:     dark ? "#1f1e1d" : "#f3f1ea"
    readonly property color surface:     dark ? "#30302e" : "#ffffff"
    readonly property color surfaceAlt:  dark ? "#2b2b29" : "#f5f4ef"
    readonly property color elevated:    dark ? "#353533" : "#ffffff"
    readonly property color userBubble:  dark ? "#3a3936" : "#efede5"
    readonly property color codeBg:      dark ? "#1d1d1b" : "#f7f6f1"
    readonly property color hover:       dark ? "#2f2f2c" : "#ebe9e1"
    readonly property color selected:    dark ? "#3a3936" : "#e5e2d8"
    readonly property color border:      dark ? "#3e3d3a" : "#e4e1d6"
    readonly property color borderStrong: dark ? "#55534e" : "#cfcbbd"
    readonly property color overlay:     dark ? "#99000000" : "#59000000"

    // Text
    readonly property color text:        dark ? "#f2f1ec" : "#1f1e1d"
    readonly property color textMuted:   dark ? "#bfbdb4" : "#5e5d58"
    readonly property color textFaint:   dark ? "#8e8c84" : "#8d8b84"

    // Accent & status
    readonly property color accent:      dark ? "#9b8cff" : "#6e56cf"
    readonly property color accentHover: dark ? "#ad9fff" : "#5d46bd"
    readonly property color accentSoft:  dark ? "#2f2a4a" : "#ece8fb"
    readonly property color textOnAccent:    "#ffffff"
    readonly property color danger:      dark ? "#f08a7a" : "#c2412d"
    readonly property color dangerSoft:  dark ? "#3a2622" : "#fbeae6"
    readonly property color success:     dark ? "#7fd1a0" : "#2f8a57"

    // Type — Inter for UI, Hack for code; modular scale (major second, base 16)
    // First installed family wins; falls back to the platform UI / fixed font.
    function firstFamily(candidates, fallback) {
        const installed = Qt.fontFamilies()
        for (const f of candidates)
            if (installed.indexOf(f) >= 0)
                return f
        return fallback
    }
    readonly property string font: firstFamily(["Inter", "Segoe UI Variable Text", "Segoe UI", ".AppleSystemUIFont",
                                                "SF Pro Text", "Noto Sans", "Cantarell"], Qt.application.font.family)
    readonly property string mono: firstFamily(["JetBrains Mono", "Cascadia Code", "Cascadia Mono", "SF Mono", "Menlo",
                                                "Hack", "Consolas", "DejaVu Sans Mono", "Noto Sans Mono"], "monospace")
    readonly property int fontCaption: 12
    readonly property int fontSmall:   13
    readonly property int fontUi:      14
    readonly property int fontBody:    16
    readonly property int fontTitle:   18
    readonly property int fontDisplay: 30

    // Geometry
    readonly property int radiusSm: 8
    readonly property int radius:   12
    readonly property int radiusLg: 20
    readonly property int chatWidth: 768
    readonly property int sidebarWidth: 272

    // Motion (ms)
    readonly property int fast: 120
    readonly property int normal: 220
}
