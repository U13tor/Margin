# Margin.Ui.Charts — lightweight in-house chart components (docs/06 §6.4).
# Zero third-party deps: bars are plain Rectangles, the donut is a QML Canvas.
# Built as a STATIC qml module; the Margin executable links the generated
# plugin so the URI resolves for qrc-loaded plugin QML (e.g. ScreenTimeTab).

qt_add_qml_module(margin_ui_charts
    URI Margin.Ui.Charts
    VERSION 1.0
    STATIC
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Margin/Ui/Charts
    QML_FILES
        MBarChart.qml
        MDonutChart.qml
        MMiniBar.qml
)
