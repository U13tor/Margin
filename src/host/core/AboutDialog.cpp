// AboutDialog impl — non-modal, launched from SystemTray "About" entry.

#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSysInfo>
#include <QVBoxLayout>
#include <QtGlobal>

namespace Margin {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("About Margin"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(6);

    // App icon (reuse MSG-001 tray SVG).
    auto* iconLabel = new QLabel(this);
    const QPixmap pix(QStringLiteral(":/icons/tray.svg"));
    if (!pix.isNull()) {
        iconLabel->setPixmap(pix.scaled(48, 48,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(iconLabel);
    }

    auto* title = new QLabel(QStringLiteral("<b>Margin</b>"), this);
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* version = new QLabel(
        QStringLiteral("v%1").arg(QLatin1String(MARGIN_VERSION)), this);
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);

    layout->addSpacing(8);

    auto* desc = new QLabel(
        QStringLiteral("Desktop rhythm conductor."), this);
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    auto* qt = new QLabel(
        QStringLiteral("Built with Qt %1.")
            .arg(QLatin1String(QT_VERSION_STR)), this);
    qt->setAlignment(Qt::AlignCenter);
    layout->addWidget(qt);

    auto* license = new QLabel(
        QStringLiteral("Released under LGPL-3.0-or-later."), this);
    license->setAlignment(Qt::AlignCenter);
    license->setWordWrap(true);
    layout->addWidget(license);

    layout->addSpacing(8);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::hide);
    connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked,
            this, &QDialog::hide);
    layout->addWidget(buttons);

    setFixedWidth(320);
}

} // namespace Margin
