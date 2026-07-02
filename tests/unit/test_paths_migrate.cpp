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
    fprintf(stderr, "[SLOT] missingSourceIsNoOp ENTER\n");
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString src = tmp.path() + QStringLiteral("/nope.db");
    const QString dst = tmp.path() + QStringLiteral("/elsewhere/nope.db");

    QVERIFY(Margin::Paths::migrateItem(src, dst));
    QVERIFY(!QFileInfo::exists(dst));
}

void TestPathsMigrate::existingDestIsIdempotent() {
    fprintf(stderr, "[SLOT] existingDestIsIdempotent ENTER\n");
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
    fprintf(stderr, "[SLOT] migratesSingleFile ENTER\n");
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
    fprintf(stderr, "[SLOT] migratesDirectoryRecursive ENTER\n");
    QTemporaryDir tmp;
    fprintf(stderr, "[STEP1] QTemporaryDir created, isValid=%d path=%s\n",
            tmp.isValid() ? 1 : 0,
            qPrintable(tmp.path()));
    QVERIFY(tmp.isValid());
    const QString srcRoot = tmp.path() + QStringLiteral("/legacy/keyring");
    const QString dstRoot = tmp.path() + QStringLiteral("/fresh/keyring");
    fprintf(stderr, "[STEP2] srcRoot=%s\n", qPrintable(srcRoot));

    const bool mkOk = QDir().mkpath(srcRoot + QStringLiteral("/Margin"));
    fprintf(stderr, "[STEP3] mkpath Margin=%d\n", mkOk ? 1 : 0);
    QVERIFY(mkOk);

    const bool wMaster = writeTextFile(srcRoot + QStringLiteral("/Margin/master.bin"),
                                       QStringLiteral("MASTER-KEY-BYTES"));
    fprintf(stderr, "[STEP4] writeTextFile master.bin=%d\n", wMaster ? 1 : 0);
    QVERIFY(wMaster);

    const bool wAux = writeTextFile(srcRoot + QStringLiteral("/Margin/aux.bin"),
                                    QStringLiteral("AUX"));
    fprintf(stderr, "[STEP5] writeTextFile aux.bin=%d\n", wAux ? 1 : 0);
    QVERIFY(wAux);

    fprintf(stderr, "[STEP6] calling migrateItem\n");
    const bool migOk = Margin::Paths::migrateItem(srcRoot, dstRoot);
    fprintf(stderr, "[STEP7] migrateItem=%d\n", migOk ? 1 : 0);
    QVERIFY(migOk);

    const bool eMaster = QFileInfo::exists(dstRoot + QStringLiteral("/Margin/master.bin"));
    fprintf(stderr, "[STEP8] exists master.bin=%d\n", eMaster ? 1 : 0);
    QVERIFY(eMaster);

    const bool eAux = QFileInfo::exists(dstRoot + QStringLiteral("/Margin/aux.bin"));
    fprintf(stderr, "[STEP9] exists aux.bin=%d\n", eAux ? 1 : 0);
    QVERIFY(eAux);

    const QString got = readTextFile(dstRoot + QStringLiteral("/Margin/master.bin"));
    fprintf(stderr, "[STEP10] readTextFile='%s'\n", qPrintable(got));
    QCOMPARE(got, QStringLiteral("MASTER-KEY-BYTES"));
}

void TestPathsMigrate::secondCallIsIdempotent() {
    fprintf(stderr, "[SLOT] secondCallIsIdempotent ENTER\n");
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
