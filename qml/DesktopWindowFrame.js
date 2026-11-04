.pragma library

function flags(decorated, platform) {
    if (decorated) return Qt.Window
    if (platform === "linux") return Qt.Window | Qt.FramelessWindowHint
    return Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
}
