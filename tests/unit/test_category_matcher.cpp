// tests/unit/test_category_matcher.cpp
//
// Validates CategoryMatcher rule loading + matching + user-override
// precedence + bad-regex resilience. Uses a real SettingsImpl backed
// by a QTemporaryDir so the read path is exercised end-to-end.

#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QVariant>

#include "host/services/Settings.h"
#include "plugins/screen_time/CategoryMatcher.h"

using Margin::Plugins::ScreenTime::CategoryMatcher;

namespace {
// Build a temp SettingsImpl + write `key` → `valueJson` (a raw JSON
// array string) so we can drive loadUserOverrides without mocking.
struct OverrideSettings {
    std::unique_ptr<QTemporaryDir> tmpDir;
    std::unique_ptr<Margin::SettingsImpl> impl;
    OverrideSettings() {
        tmpDir = std::make_unique<QTemporaryDir>();
        Q_ASSERT(tmpDir->isValid());
        impl = std::make_unique<Margin::SettingsImpl>(tmpDir->path());
    }
};
} // namespace

class TestCategoryMatcher : public QObject {
    Q_OBJECT

private slots:
    void testLoadDefaultsFromQrc();
    void testMatchKnownApps();
    void testNoMatchReturnsUncategorized();
    void testEmptyProcessNameReturnsUncategorized();
    void testUserOverridesTakePrecedence();
    void testBadRegexInUserOverridesIsSkipped();
    void testEmptyUserOverridesIsNoOp();
};

void TestCategoryMatcher::testLoadDefaultsFromQrc() {
    CategoryMatcher m;
    const int n = m.loadDefaults(QStringLiteral(":/screen_time/default_categories.json"));
    QVERIFY2(n >= 5, "Default JSON should ship at least 5 categories");
    QVERIFY(m.rules().size() == n);
}

void TestCategoryMatcher::testMatchKnownApps() {
    CategoryMatcher m;
    m.loadDefaults(QStringLiteral(":/screen_time/default_categories.json"));

    QCOMPARE(m.match(QStringLiteral("Code.exe")),     QStringLiteral("Development"));
    QCOMPARE(m.match(QStringLiteral("idea64.exe")),   QStringLiteral("Development"));
    QCOMPARE(m.match(QStringLiteral("Trae.exe")),     QStringLiteral("Development"));
    QCOMPARE(m.match(QStringLiteral("Qoder.exe")),    QStringLiteral("Development"));
    QCOMPARE(m.match(QStringLiteral("studio64.exe")), QStringLiteral("Development"));
    QCOMPARE(m.match(QStringLiteral("notepad++.exe")),QStringLiteral("Development"));
    QCOMPARE(m.match(QStringLiteral("chrome.exe")),   QStringLiteral("Browser"));
    QCOMPARE(m.match(QStringLiteral("msedge.exe")),   QStringLiteral("Browser"));
    QCOMPARE(m.match(QStringLiteral("Slack.exe")),    QStringLiteral("Communication"));
    QCOMPARE(m.match(QStringLiteral("WXWork.exe")),   QStringLiteral("Communication"));
    QCOMPARE(m.match(QStringLiteral("Explorer.exe")), QStringLiteral("System"));
    QCOMPARE(m.match(QStringLiteral("Notion.exe")),   QStringLiteral("Notes"));
    QCOMPARE(m.match(QStringLiteral("Obsidian.exe")), QStringLiteral("Notes"));
}

void TestCategoryMatcher::testNoMatchReturnsUncategorized() {
    CategoryMatcher m;
    m.loadDefaults(QStringLiteral(":/screen_time/default_categories.json"));
    QCOMPARE(m.match(QStringLiteral("random-unknown-app.exe")),
             QStringLiteral("Uncategorized"));
}

void TestCategoryMatcher::testEmptyProcessNameReturnsUncategorized() {
    CategoryMatcher m;
    m.loadDefaults(QStringLiteral(":/screen_time/default_categories.json"));
    QCOMPARE(m.match(QString()), QStringLiteral("Uncategorized"));
}

void TestCategoryMatcher::testUserOverridesTakePrecedence() {
    OverrideSettings os;
    // Override Code.exe → "Editor" (vs default "Development"). Also
    // add a brand-new rule for someunknown.exe → "Custom".
    const QString json = QStringLiteral(
        "["
        "  {\"match\": \"Code.exe\", \"category\": \"Editor\"},"
        "  {\"match\": \"someunknown.exe\", \"category\": \"Custom\"}"
        "]"
    );
    os.impl->set(QStringLiteral("plugins.screen_time.category_overrides"),
                 QVariant(json));

    CategoryMatcher m;
    m.loadDefaults(QStringLiteral(":/screen_time/default_categories.json"));
    const int added = m.loadUserOverrides(*os.impl);
    QCOMPARE(added, 2);

    // Override wins over default for Code.exe.
    QCOMPARE(m.match(QStringLiteral("Code.exe")),     QStringLiteral("Editor"));
    // New rule works.
    QCOMPARE(m.match(QStringLiteral("someunknown.exe")), QStringLiteral("Custom"));
    // Default rules still resolve for non-overridden apps.
    QCOMPARE(m.match(QStringLiteral("chrome.exe")),   QStringLiteral("Browser"));
}

void TestCategoryMatcher::testBadRegexInUserOverridesIsSkipped() {
    OverrideSettings os;
    // [unterminated is invalid PCRE — must be skipped, not crash.
    const QString json = QStringLiteral(
        "["
        "  {\"match\": \"[unterminated\", \"category\": \"Bad\"},"
        "  {\"match\": \"GoodApp.exe\", \"category\": \"Good\"}"
        "]"
    );
    os.impl->set(QStringLiteral("plugins.screen_time.category_overrides"),
                 QVariant(json));

    CategoryMatcher m;
    m.loadDefaults(QStringLiteral(":/screen_time/default_categories.json"));
    const int added = m.loadUserOverrides(*os.impl);
    QCOMPARE(added, 1);  // bad regex skipped, good one added

    QCOMPARE(m.match(QStringLiteral("GoodApp.exe")), QStringLiteral("Good"));
    // BadApp.exe doesn't match the bad rule (skipped), but the bad
    // category string isn't reachable either.
    QCOMPARE(m.match(QStringLiteral("BadApp.exe")), QStringLiteral("Uncategorized"));
}

void TestCategoryMatcher::testEmptyUserOverridesIsNoOp() {
    OverrideSettings os;
    // Don't write the key — empty default.

    CategoryMatcher m;
    m.loadDefaults(QStringLiteral(":/screen_time/default_categories.json"));
    const int added = m.loadUserOverrides(*os.impl);
    QCOMPARE(added, 0);
    // Defaults still work.
    QCOMPARE(m.match(QStringLiteral("Code.exe")), QStringLiteral("Development"));
}

QTEST_MAIN(TestCategoryMatcher)
#include "test_category_matcher.moc"
