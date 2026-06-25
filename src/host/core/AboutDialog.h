// AboutDialog — small non-modal QDialog shown from the tray menu's "About"
// entry (M4-C16). Reads version from MARGIN_VERSION macro (project(VERSION)
// propagated via margin_plugin_api compile defs). Styling is Qt-native:
// the tray menu fidelity work explicitly excludes pixel theming.

#pragma once

#include <QDialog>

namespace Margin {

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

} // namespace Margin
