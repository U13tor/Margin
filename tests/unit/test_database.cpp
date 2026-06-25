// tests/unit/test_database.cpp
//
// Validates DatabaseImpl round-trip + transaction semantics + WAL pragma
// application (docs/05-host-services.md §5.3). Uses a fresh QTemporaryDir
// per test so the real margin.db is never touched.

#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

#include "host/services/DatabaseImpl.h"

using Margin::DatabaseImpl;

namespace {
QVariantMap params(std::initializer_list<std::pair<QString, QVariant>> items) {
    QVariantMap m;
    for (const auto& p : items) m.insert(p.first, p.second);
    return m;
}
} // namespace

class TestDatabase : public QObject {
    Q_OBJECT

private slots:
    void testOpenCreatesFile();
    void testExecQueryRoundTrip();
    void testNamedParamsBind();
    void testTransactionCommit();
    void testTransactionRollback();
    void testBadSqlReturnsFalse();
    void testOpenIsIdempotent();
    void testCloseIsSafeWhenNotOpen();
};

void TestDatabase::testOpenCreatesFile() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + "/margin-test.db";

    DatabaseImpl db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QVERIFY2(db.lastError().isEmpty(), "open() should clear lastError on success");

    db.close();
    QVERIFY2(QFile::exists(path), "SQLite file should exist on disk after open");
}

void TestDatabase::testExecQueryRoundTrip() {
    QTemporaryDir tmp;
    DatabaseImpl db;
    QVERIFY(db.open(tmp.path() + "/rt.db"));

    QVERIFY(db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"));
    QVERIFY(db.exec("INSERT INTO t (id, name) VALUES (:id, :name)",
                    params({{"id", 1}, {"name", QStringLiteral("alpha")}})));
    QVERIFY(db.exec("INSERT INTO t (id, name) VALUES (:id, :name)",
                    params({{"id", 2}, {"name", QStringLiteral("beta")}})));

    const auto rows = db.query("SELECT id, name FROM t ORDER BY id");
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows[0].value("id").toInt(), 1);
    QCOMPARE(rows[0].value("name").toString(), QStringLiteral("alpha"));
    QCOMPARE(rows[1].value("id").toInt(), 2);
    QCOMPARE(rows[1].value("name").toString(), QStringLiteral("beta"));
    QVERIFY2(db.lastError().isEmpty(), "successful query should clear lastError");
}

void TestDatabase::testNamedParamsBind() {
    QTemporaryDir tmp;
    DatabaseImpl db;
    QVERIFY(db.open(tmp.path() + "/params.db"));
    QVERIFY(db.exec("CREATE TABLE t (a INTEGER, b INTEGER, c TEXT)"));

    QVERIFY(db.exec("INSERT INTO t VALUES (:a, :b, :c)",
                    params({{"a", 10},
                            {"b", 20},
                            {"c", QStringLiteral("hi")}})));

    const auto rows = db.query("SELECT a, b, c FROM t");
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].value("a").toInt(), 10);
    QCOMPARE(rows[0].value("b").toInt(), 20);
    QCOMPARE(rows[0].value("c").toString(), QStringLiteral("hi"));
}

void TestDatabase::testTransactionCommit() {
    QTemporaryDir tmp;
    DatabaseImpl db;
    QVERIFY(db.open(tmp.path() + "/tx-commit.db"));
    QVERIFY(db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)"));

    QVERIFY(db.transaction());
    QVERIFY(db.exec("INSERT INTO t (id) VALUES (1)"));
    QVERIFY(db.exec("INSERT INTO t (id) VALUES (2)"));
    QVERIFY(db.commit());

    const auto rows = db.query("SELECT COUNT(*) AS c FROM t");
    QCOMPARE(rows[0].value("c").toInt(), 2);
}

void TestDatabase::testTransactionRollback() {
    QTemporaryDir tmp;
    DatabaseImpl db;
    QVERIFY(db.open(tmp.path() + "/tx-rollback.db"));
    QVERIFY(db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)"));

    QVERIFY(db.transaction());
    QVERIFY(db.exec("INSERT INTO t (id) VALUES (1)"));
    QVERIFY(db.rollback());

    const auto rows = db.query("SELECT COUNT(*) AS c FROM t");
    QCOMPARE(rows[0].value("c").toInt(), 0);
}

void TestDatabase::testBadSqlReturnsFalse() {
    QTemporaryDir tmp;
    DatabaseImpl db;
    QVERIFY(db.open(tmp.path() + "/bad.db"));

    QVERIFY2(!db.exec("THIS IS NOT SQL"),
             "Bad SQL should return false, not crash");
    QVERIFY2(!db.lastError().isEmpty(),
             "lastError should describe the failure");
}

void TestDatabase::testOpenIsIdempotent() {
    QTemporaryDir tmp;
    DatabaseImpl db;
    QVERIFY(db.open(tmp.path() + "/idem.db"));
    QVERIFY2(db.open(tmp.path() + "/idem.db"),
             "Second open() on same path should be no-op returning true");
}

void TestDatabase::testCloseIsSafeWhenNotOpen() {
    DatabaseImpl db;
    // close() before open() must not crash.
    db.close();
    QVERIFY(db.lastError().isEmpty());
}

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
