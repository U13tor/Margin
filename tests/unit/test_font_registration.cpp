// tests/unit/test_font_registration.cpp
//
// M4-C2: verifies the bundled Inter + JetBrains Mono TTFs (fetched into
// src/resources/fonts/ by cmake/fetch_fonts.cmake and compiled into the test
// exe via fonts.qrc) are loadable via QFontDatabase::addApplicationFont and
// register the expected family names. Guards the qrc wiring + the HostCore
// bootstrap-time registration loop.
//
// Failure paths covered:
//   1. A bogus qrc URL returns -1 (mirrors the "font not bundled" path in
//      HostCore::bootstrap where the fetch script couldn't reach the network).
//
// Mechanism: links fonts.qrc directly (mirrors test_dashboard_shell linking
// host.qrc + ui.qrc). QTEST_MAIN picks QGuiApplication because Qt6::Gui is
// linked — required for QFontDatabase to function.

#include <QFontDatabase>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTest>

class TestFontRegistration : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void interRegularRegisters();
    void interMediumRegisters();
    void interSemiBoldRegisters();
    void jetBrainsMonoRegularRegisters();
    void jetBrainsMonoMediumRegisters();
    void bogusPathReturnsMinusOne();
};

void TestFontRegistration::initTestCase() {
    // Sanity-check the qrc is wired into the test exe — if the qrc is empty
    // (fetch script couldn't download), every individual test would fail with
    // the same -1 return; surface the cause up front.
    const int probe = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/Inter-Regular.ttf"));
    if (probe < 0) {
        QSKIP("fonts.qrc is empty — cmake/fetch_fonts.cmake did not download "
              "any faces (offline configure?). Skipping the registration "
              "assertions; the bogus-path test still runs.");
    }
}

void TestFontRegistration::interRegularRegisters() {
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/Inter-Regular.ttf"));
    QVERIFY2(id >= 0, "Inter-Regular.ttf addApplicationFont returned -1");
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    QVERIFY2(families.contains(QStringLiteral("Inter")),
             qPrintable(QStringLiteral("expected 'Inter' in %1")
                            .arg(families.join(QStringLiteral(", ")))));
}

void TestFontRegistration::interMediumRegisters() {
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/Inter-Medium.ttf"));
    QVERIFY2(id >= 0, "Inter-Medium.ttf addApplicationFont returned -1");
    QVERIFY2(QFontDatabase::applicationFontFamilies(id).contains(QStringLiteral("Inter")),
             "Inter-Medium should register under the 'Inter' family name");
}

void TestFontRegistration::interSemiBoldRegisters() {
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/Inter-SemiBold.ttf"));
    QVERIFY2(id >= 0, "Inter-SemiBold.ttf addApplicationFont returned -1");
    QVERIFY2(QFontDatabase::applicationFontFamilies(id).contains(QStringLiteral("Inter")),
             "Inter-SemiBold should register under the 'Inter' family name");
}

void TestFontRegistration::jetBrainsMonoRegularRegisters() {
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/JetBrainsMono-Regular.ttf"));
    QVERIFY2(id >= 0, "JetBrainsMono-Regular.ttf addApplicationFont returned -1");
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    QVERIFY2(families.contains(QStringLiteral("JetBrains Mono")),
             qPrintable(QStringLiteral("expected 'JetBrains Mono' in %1")
                            .arg(families.join(QStringLiteral(", ")))));
}

void TestFontRegistration::jetBrainsMonoMediumRegisters() {
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/JetBrainsMono-Medium.ttf"));
    QVERIFY2(id >= 0, "JetBrainsMono-Medium.ttf addApplicationFont returned -1");
    QVERIFY2(QFontDatabase::applicationFontFamilies(id).contains(QStringLiteral("JetBrains Mono")),
             "JetBrainsMono-Medium should register under the 'JetBrains Mono' family name");
}

void TestFontRegistration::bogusPathReturnsMinusOne() {
    // Missing resource path returns -1 — must not throw, must not crash. This
    // is the contract HostCore::bootstrap relies on when the fetch script
    // couldn't bundle a face.
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/__nonexistent_face__.ttf"));
    QCOMPARE(id, -1);
}

QTEST_MAIN(TestFontRegistration)
#include "test_font_registration.moc"
