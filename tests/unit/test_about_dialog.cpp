// tests/unit/test_about_dialog.cpp
//
// M4-C16: verifies the About dialog surfaces the version from the
// MARGIN_VERSION macro and the LGPL-3.0-or-later license note. Runs
// headless — QDialog::show() is never called, only widget children are
// inspected.

#include <QLabel>
#include <QObject>
#include <QList>
#include <QString>
#include <QTest>

#include "host/core/AboutDialog.h"

namespace {

bool anyLabelContains(QWidget* parent, const QString& needle) {
    const auto labels = parent->findChildren<QLabel*>();
    for (QLabel* lbl : labels) {
        if (lbl->text().contains(needle)) return true;
    }
    return false;
}

} // namespace

class TestAboutDialog : public QObject {
    Q_OBJECT

private slots:
    void showsVersionFromMacro() {
        Margin::AboutDialog dlg;
        // MARGIN_VERSION is propagated as a string macro; we just confirm
        // the "v" prefix and the literal version string landed somewhere.
        const QString needle =
            QStringLiteral("v") + QLatin1String(MARGIN_VERSION);
        QVERIFY2(anyLabelContains(&dlg, needle),
                 qPrintable(QStringLiteral("No label contains '%1'").arg(needle)));
    }

    void showsLicenseNote() {
        Margin::AboutDialog dlg;
        QVERIFY(anyLabelContains(&dlg, QStringLiteral("LGPL-3.0-or-later")));
    }

    void showsAppName() {
        Margin::AboutDialog dlg;
        QVERIFY(anyLabelContains(&dlg, QStringLiteral("Margin")));
    }

    void showsQtVersionNote() {
        Margin::AboutDialog dlg;
        QVERIFY(anyLabelContains(&dlg, QStringLiteral("Qt ")));
    }
};

QTEST_MAIN(TestAboutDialog)
#include "test_about_dialog.moc"
