pragma Singleton
import QtQuick

QtObject {
    property bool dark: false
    property bool reducedMotion: Application.styleHints.accessibility.motionPreference === Qt.ReducedMotion
    readonly property int enterDuration: reducedMotion ? 0 : 160
    readonly property int exitDuration: reducedMotion ? 0 : 120
    property color lightAccent: "#2868b8"
    property color darkAccent: "#8dbbff"
    property color lightBackground: "#edf2f7"
    property color darkBackground: "#11161e"
    property Item backdrop: null
    function luminance(value: color): real {
        const linear = channel => channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4)
        return 0.2126 * linear(value.r) + 0.7152 * linear(value.g) + 0.0722 * linear(value.b)
    }
    function mix(a: color, b: color, amount: real): color {
        return Qt.rgba(a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount, a.b + (b.b - a.b) * amount, 1)
    }
    function contrastingText(surface: color): color {
        // Account for translucent accents over the normal control surface.
        const opaque = mix(popupSolid, surface, surface.a)
        const value = luminance(opaque)
        const whiteContrast = 1.05 / (value + 0.05)
        const blackContrast = (value + 0.05) / 0.05
        return whiteContrast >= blackContrast ? "white" : "black"
    }
    function readable(preferred: color): color {
        const contrast = (luminance(preferred) + 0.05) / (luminance(window) + 0.05)
        return Math.max(contrast, 1 / contrast) >= 4.5 ? preferred : darkSurface ? "white" : "black"
    }
    readonly property color window: dark ? darkBackground : lightBackground
    // Keep content readable even if a light color is chosen for Dark, or vice versa.
    readonly property bool darkSurface: luminance(window) < 0.179
    readonly property color text: readable(darkSurface ? "#f0f3f8" : "#192330")
    readonly property color muted: readable(darkSurface ? "#b9c2d0" : "#4e5b6c")
    readonly property color border: darkSurface ? "#0affffff" : "#240e243d"
    readonly property color popupSolid: Qt.rgba(window.r, window.g, window.b, 1)
    readonly property color surface: popupSolid
    readonly property color glass: Qt.rgba(window.r, window.g, window.b, darkSurface ? 0.68 : 0.78)
    readonly property color glassDense: Qt.rgba(window.r, window.g, window.b, darkSurface ? 0.80 : 0.88)
    readonly property real panelRadius: 24
    readonly property real menuRadius: 16
    readonly property real tooltipRadius: 12
    readonly property color panelShadow: darkSurface ? "#4d000000" : "#26091320"
    readonly property color panelContactShadow: darkSurface ? "#24000000" : "#12091320"
    readonly property color panelSheen: darkSurface ? "#0bffffff" : "#18ffffff"
    readonly property color field: darkSurface ? "#12ffffff" : "#100c2847"
    readonly property color hover: darkSurface ? "#20ffffff" : "#180c2847"
    readonly property color pressed: darkSurface ? "#30ffffff" : "#240c2847"
    readonly property color selection: Qt.rgba(accent.r, accent.g, accent.b, darkSurface ? 0.26 : 0.2)
    readonly property color accent: dark ? darkAccent : lightAccent
    readonly property color accentText: contrastingText(accent)
    readonly property color scrim: darkSurface ? "#33000000" : "#160e2038"
}
