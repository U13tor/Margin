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

void TestPathsMigrate::migratesDirectoryRecursive() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString srcRoot = tmp.path() + QStringLiteral("/legacy/keyring");
    const QString dstRoot = tmp.path() + QStringLiteral("/fresh/keyring");
    // Build a 2-level tree mirroring Keyring's <service>/<key>.bin layout.
    QVERIFY(QDir().mkpath(srcRoot + QStringLiteral("/Margin")));
    QVERIFY(writeTextFile(srcRoot + QStringLiteral("/Margin/master.bin"),
                          QStringLiteral("MASTER-KEY-BYTES")));
    QVERIFY(writeTextFile(srcRoot + QStringLiteral("/Margin/aux.bin"),
                          QStringLiteral("AUX")));

    QVERIFY(Margin::Paths::migrateItem(srcRoot, dstRoot));
    QVERIFY(QFileInfo::exists(dstRoot + QStringLiteral("/Margin/master.bin")));
    QVERIFY(QFileInfo::exists(dstRoot + QStringLiteral("/Margin/aux.bin")));
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
