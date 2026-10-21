import QtQuick
import QtQuick.Shapes
import Orbital

// Small rotating arc used for running tool calls.
Item {
    id: spinner
    implicitWidth: 18
    implicitHeight: 18

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        RotationAnimation on rotation { from: 0; to: 360; duration: 900; loops: Animation.Infinite; running: spinner.visible }
        ShapePath {
            strokeColor: Theme.accent
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: spinner.width / 2
                centerY: spinner.height / 2
                radiusX: spinner.width / 2 - 2
                radiusY: spinner.height / 2 - 2
                startAngle: 0
                sweepAngle: 270
            }
        }
    }
}
