import QtQuick

// Theme-tinted SVG icon served by the C++ "icon" image provider.
Image {
    id: icon
    property string name
    property color color: "#000000"
    property int size: 18

    width: size
    height: size
    sourceSize: Qt.size(size * Screen.devicePixelRatio, size * Screen.devicePixelRatio)
    source: name ? "image://icon/" + name + "?" + color : ""
    smooth: true
    asynchronous: false
    cache: true
}
