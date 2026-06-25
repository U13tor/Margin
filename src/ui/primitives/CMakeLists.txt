# Margin.Ui.Primitives — atomic QML component library (docs/06 §6.2).
# M0-C10a seeded the module with the Theme design-token singleton. M4-C3/C4 add
# MIcon (icon atom) + MButton (3-variant button atom). M4-C5/C6 add MSwitch +
# MSlider + MCard + MListItem. MTextField / MTextArea / ... land in later
# milestones as they are first needed.
# Built as a STATIC qml module; the Margin executable links the generated
# plugin so the URI resolves for qrc-loaded shell QML.

# Singleton property must be set before qt_add_qml_module reads QML_FILES so the
# generated qmldir marks Theme as `singleton`. MIcon + MButton are per-instance
# atoms — NO QT_QML_SINGLETON_TYPE for them.
set_source_files_properties(Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(margin_ui_primitives
    URI Margin.Ui.Primitives
    VERSION 1.0
    STATIC
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Margin/Ui/Primitives
    QML_FILES
        Theme.qml
        MIcon.qml
        MButton.qml
        MSwitch.qml
        MSlider.qml
        MCard.qml
        MListItem.qml
        ProgressDots.qml
)
