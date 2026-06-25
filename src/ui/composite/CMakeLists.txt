# Margin.Ui.Composite — composite QML component library (docs/06 §6.3).
# M0-C10a registered the module URI only (skeleton). M4-C7 lands the first
# two composites: StatusCard (Tab1 summary card, 3-slot + default slot +
# clicked) and TimerDisplay (Tab3 big-mono countdown, mm:ss / HH:mm:ss).
# DashboardTabView / Toast / Modal / DevicePairingList / ... land in later
# milestones as they are first needed, per docs/06 §6.3 table.
# Built as a STATIC qml module; the Margin executable + composite-atom tests
# link the generated plugin so the URI resolves at load time.

qt_add_qml_module(margin_ui_composite
    URI Margin.Ui.Composite
    VERSION 1.0
    STATIC
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Margin/Ui/Composite
    QML_FILES
        StatusCard.qml
        TimerDisplay.qml
        CircularTimer.qml
)
