// test_paths_migrate — verifies Paths::migrateItem behavior used by
// migrateFromLegacyLayout(). Drives the public test seam against
// QTemporaryDir-backed paths, covering file/dir copy, idempotency, and
// missing-source no-op. migrateFromLegacyLayout() itself wraps the same
// primitive with QStandardPaths-derived paths and is not exercised here
// (mocking Qt global state would be disproportionate to the logic).

#include "paths/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>

class TestPathsMigrate : public QObject {
    Q_OBJECT

private slots:
    void missingSourceIsNoOp();
    void existingDestIsIdempotent();
    void migratesSingleFile();
    void migratesSettingsJsonFromLegacy();
    void preservesExistingSettingsJsonOnConflict();
    void migratesDirectoryRecursive();
    void secondCallIsIdempotent();
};

namespace {
// Write a small text file with deterministic content for byte-equality checks.
bool writeTextFile(const QString& path, const QString& content) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream out(&f);
    out << content;
    out.flush();
    f.close();
    return true;
}

QString readTextFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll());
}
} // namespace

void TestPathsMigrate::missingSourceIsNoOp() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = tmp.path() + QStringLiteral("/nope.db");
    const QString dst = tmp.path() + QStringLiteral("/elsewhere/nope.db");

    QVERIFY(Margin::Paths::migrateItem(src, dst));
    QVERIFY(!QFileInfo::exists(dst));
}

void TestPathsMigrate::existingDestIsIdempotent() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = tmp.path() + QStringLiteral("/origin.dat");
    const QString dst = tmp.path() + QStringLiteral("/target/origin.dat");
    QVERIFY(writeTextFile(src, QStringLiteral("OLD")));
    QVERIFY(QDir().mkpath(QFileInfo(dst).absolutePath()));
    QVERIFY(writeTextFile(dst, QStringLiteral("ALREADY-HERE")));

    QVERIFY(Margin::Paths::migrateItem(src, dst));
    // dst must be untouched — migration must not overwrite existing data.
    QCOMPARE(readTextFile(dst), QStringLiteral("ALREADY-HERE"));
}

void TestPathsMigrate::migratesSingleFile() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = tmp.path() + QStringLiteral("/legacy/margin.db");
    const QString dst = tmp.path() + QStringLiteral("/fresh/margin.db");
    QVERIFY(QDir().mkpath(QFileInfo(src).absolutePath()));
    QVERIFY(writeTextFile(src, QStringLiteral("SQLITE-HEADER-BYTES")));

    QVERIFY(Margin::Paths::migrateItem(src, dst));
    QVERIFY(QFileInfo::exists(dst));
    QCOMPARE(readTextFile(dst), QStringLiteral("SQLITE-HEADER-BYTES"));
    // Source is preserved — migrateItem copies, never moves, so a downgrade
    // reinstall can still find the legacy data until NSIS uninstall wipes
    // the legacy INSTDIR.
    QVERIFY(QFileInfo::exists(src));
}

void TestPathsMigrate::migratesSettingsJsonFromLegacy() {
    // Reproduces the M6-C5+1 scenario at the unit level: settings.json that
    // used to live under %LOCALAPPDATA%\Margin\ (= legacyRoot, == NSIS
    // InstallDir) must be copied to the Roaming config dir on first launch
    // after the fix, so that uninstall/reinstall no longer wipes Aura's
    // paired-device + encrypted settings. The encrypted ct/iv blobs are
    // opaque to migrateItem — it just copies bytes — so any valid JSON
    // shape stands in for the real payload.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString legacyConfigDir = tmp.path() + QStringLiteral("/legacy");
    const QString roamingConfigDir = tmp.path() + QStringLiteral("/roaming");
    QVERIFY(QDir().mkpath(legacyConfigDir));
    const QString payload = QStringLiteral(
        "{\"plugins\":{\"aura\":{"
        "\"paired_device_identifier\":{\"__encrypted__\":true,"
        "\"ct\":\"0f8DBS2a/QwS9ioy4nICO0FUHdfIH0SlH2Fau46a3QzbkNms6UQdaA==\","
        "\"iv\":\"S5R1ukscsWVRRrS+\"},"
        "\"rssi_threshold\":-90}}}");
    QVERIFY(writeTextFile(legacyConfigDir + QStringLiteral("/settings.json"),
                          payload));

    QVERIFY(Margin::Paths::migrateItem(
        legacyConfigDir + QStringLiteral("/settings.json"),
        roamingConfigDir + QStringLiteral("/settings.json")));

    QVERIFY(QFileInfo::exists(roamingConfigDir
                              + QStringLiteral("/settings.json")));
    QCOMPARE(readTextFile(roamingConfigDir
                          + QStringLiteral("/settings.json")),
             payload);
    // Source preserved — uninstall will clean it later, not migrateItem.
    QVERIFY(QFileInfo::exists(legacyConfigDir
                              + QStringLiteral("/settings.json")));
}

void TestPathsMigrate::preservesExistingSettingsJsonOnConflict() {
    // If the user already launched the fixed build once (settings.json
    // written to Roaming) and then a stale installer drops an old Local
    // build's settings.json back into legacyRoot, migrateItem must NOT
    // clobber the user's current Roaming settings.json. Same idempotency
    // contract as existingDestIsIdempotent, but with realistic JSON content
    // to guard against a future refactor that might switch on file type.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString legacyConfigDir = tmp.path() + QStringLiteral("/legacy");
    const QString roamingConfigDir = tmp.path() + QStringLiteral("/roaming");
    QVERIFY(QDir().mkpath(legacyConfigDir));
    QVERIFY(QDir().mkpath(roamingConfigDir));
    // Payload strings kept out of the QVERIFY() argument list because moc's
    // macro-argument scanner mishandles `\"` sequences inside string literals
    // embedded directly in a QVERIFY expansion (build error "missing ')' in
    // macro usage"). Assigning to a local variable first sidesteps it.
    const QString legacyPayload = QStringLiteral(
        "{\"plugins\":{\"aura\":{\"old\":1}}}");
    const QString freshPayload = QStringLiteral(
        "{\"plugins\":{\"aura\":{\"paired_device_identifier\":"
        "{\"__encrypted__\":true,\"ct\":\"FRESH\",\"iv\":\"FRESHIV==\"}}}}");
    QVERIFY(writeTextFile(legacyConfigDir + QStringLiteral("/settings.json"),
                          legacyPayload));
    QVERIFY(writeTextFile(roamingConfigDir + QStringLiteral("/settings.json"),
                          freshPayload));

    QVERIFY(Margin::Paths::migrateItem(
        legacyConfigDir + QStringLiteral("/settings.json"),
        roamingConfigDir + QStringLiteral("/settings.json")));

    QCOMPARE(readTextFile(roamingConfigDir
                          + QStringLiteral("/settings.json")),
             freshPayload);
}

void TestPathsMigrate::migratesDirectoryRecursive() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString srcRoot = tmp.path() + QStringLiteral("/legacy/keyring");
    const QString dstRoot = tmp.path() + QStringLiteral("/fresh/keyring");
    // Build a 2-level tree mirroring Keyring's <service>/<key>.bin layout.
    // Avoid Windows reserved device names (AUX/CON/PRN/NUL/COM*/LPT*) —
    // Windows Server 2022 (CI runner) rejects CreateFile for "aux.*" while
    // Windows 11 LTSC tolerates it, causing flaky cross-version failures.
    QVERIFY(QDir().mkpath(srcRoot + QStringLiteral("/Margin")));
    QVERIFY(writeTextFile(srcRoot + QStringLiteral("/Margin/master.bin"),
                          QStringLiteral("MASTER-KEY-BYTES")));
    QVERIFY(writeTextFile(srcRoot + QStringLiteral("/Margin/secondary.bin"),
                          QStringLiteral("SECONDARY")));

    QVERIFY(Margin::Paths::migrateItem(srcRoot, dstRoot));
    QVERIFY(QFileInfo::exists(dstRoot + QStringLiteral("/Margin/master.bin")));
    QVERIFY(QFileInfo::exists(dstRoot + QStringLiteral("/Margin/secondary.bin")));
    QCOMPARE(readTextFile(dstRoot + QStringLiteral("/Margin/master.bin")),
             QStringLiteral("MASTER-KEY-BYTES"));
}

void TestPathsMigrate::secondCallIsIdempotent() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = tmp.path() + QStringLiteral("/origin.dat");
    const QString dst = tmp.path() + QStringLiteral("/target/origin.dat");
    QVERIFY(writeTextFile(src, QStringLiteral("FIRST")));

    QVERIFY(Margin::Paths::migrateItem(src, dst));
    QCOMPARE(readTextFile(dst), QStringLiteral("FIRST"));

    // Change the source after first migration — second call must NOT
    // re-copy and overwrite, even though src size now differs from dst.
    QVERIFY(writeTextFile(src, QStringLiteral("SECOND-LONGER")));
    QVERIFY(Margin::Paths::migrateItem(src, dst));
    QCOMPARE(readTextFile(dst), QStringLiteral("FIRST"));
}

QTEST_MAIN(TestPathsMigrate)
#include "test_paths_migrate.moc"
