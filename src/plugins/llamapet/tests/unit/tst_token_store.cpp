#include <QtTest>
#include "core/TokenStore.h"
#include "core/TokenTracker.h"
#include "core/LlamaCppIt.h"
#include "Margin/Database.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QUuid>

using namespace Margin::Plugins::LlamaPet;

namespace {

class InMemoryDatabase : public Margin::Database {
public:
    InMemoryDatabase() {
        m_connName = QStringLiteral("mem_db_") + QUuid::createUuid().toString(QUuid::Id128);
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connName);
        m_db.setDatabaseName(QStringLiteral(":memory:"));
        m_opened = m_db.open();
    }

    ~InMemoryDatabase() override {
        close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connName);
    }

    bool open(const QString&) override {
        return m_opened;
    }

    void close() override {
        if (m_opened) {
            m_db.close();
            m_opened = false;
        }
    }

    bool exec(const QString& sql, const QVariantMap& params = {}) override {
        QSqlQuery q(m_db);
        q.prepare(sql);
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            q.bindValue(QLatin1Char(':') + it.key(), it.value());
        }
        bool ok = q.exec();
        if (!ok) {
            m_lastError = q.lastError().text();
        } else {
            m_lastError.clear();
        }
        return ok;
    }

    QList<QVariantMap> query(const QString& sql, const QVariantMap& params = {}) override {
        QList<QVariantMap> rows;
        QSqlQuery q(m_db);
        q.prepare(sql);
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            q.bindValue(QLatin1Char(':') + it.key(), it.value());
        }
        if (!q.exec()) {
            m_lastError = q.lastError().text();
            return rows;
        }
        m_lastError.clear();
        while (q.next()) {
            QVariantMap row;
            QSqlRecord rec = q.record();
            for (int i = 0; i < rec.count(); ++i) {
                row.insert(rec.fieldName(i), rec.value(i));
            }
            rows.append(row);
        }
        return rows;
    }

    bool transaction() override { return m_db.transaction(); }
    bool commit() override { return m_db.commit(); }
    bool rollback() override { return m_db.rollback(); }
    QString lastError() const override { return m_lastError; }

private:
    QString m_connName;
    QSqlDatabase m_db;
    bool m_opened{false};
    QString m_lastError;
};

} // namespace

class TstTokenStore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testPrometheusMetricsParsing();
    void testTokenTrackerChannelA();
    void testTokenTrackerChannelB();
    void testStreakCalculations();
    void testQuantileLeveling();
    void testDatabasePersistenceAndQueries();
};

void TstTokenStore::testPrometheusMetricsParsing() {
    QString rawMetrics = QStringLiteral(
        "# HELP llamacpp:prompt_tokens_total Total prompt tokens\n"
        "# TYPE llamacpp:prompt_tokens_total counter\n"
        "llamacpp:prompt_tokens_total 45210\n"
        "# HELP llamacpp:tokens_predicted_total Total predicted tokens\n"
        "# TYPE llamacpp:tokens_predicted_total counter\n"
        "llamacpp:tokens_predicted_total 98765\n"
    );

    auto info = llama_slots::parsePrometheusMetrics(rawMetrics);
    QVERIFY(info.predictedTokensTotal.has_value());
    QVERIFY(info.promptTokensTotal.has_value());
    QCOMPARE(*info.predictedTokensTotal, 98765ull);
    QCOMPARE(*info.promptTokensTotal, 45210ull);
}

void TstTokenStore::testTokenTrackerChannelA() {
    TokenTracker tracker;

    // Tick 1: Baseline setup
    LlmTelemetry t1;
    t1.connected = true;
    t1.metricsSupported = true;
    t1.predictedTokensTotal = 1000;
    t1.promptTokensTotal = 500;

    auto d1 = tracker.process(t1);
    QVERIFY(tracker.isUsingMetrics());
    QCOMPARE(d1.completionTokens, 0ull);
    QCOMPARE(d1.promptTokens, 0ull);

    // Tick 2: 50 completion tokens generated
    LlmTelemetry t2;
    t2.connected = true;
    t2.metricsSupported = true;
    t2.predictedTokensTotal = 1050;
    t2.promptTokensTotal = 520;

    auto d2 = tracker.process(t2);
    QCOMPARE(d2.completionTokens, 50ull);
    QCOMPARE(d2.promptTokens, 20ull);
    QCOMPARE(d2.activeSeconds, 1u);

    // Tick 3: Server restarted! Counter reset from 1050 to 15
    LlmTelemetry t3;
    t3.connected = true;
    t3.metricsSupported = true;
    t3.predictedTokensTotal = 15;
    t3.promptTokensTotal = 10;

    auto d3 = tracker.process(t3);
    QCOMPARE(d3.completionTokens, 15ull);
    QCOMPARE(d3.promptTokens, 10ull);
}

void TstTokenStore::testTokenTrackerChannelB() {
    TokenTracker tracker;

    SlotInfo s0;
    s0.id = 0;
    s0.isActive = true;
    s0.promptTokens = 100;
    s0.decodedTokens = 20;

    // Tick 1: baseline
    LlmTelemetry t1;
    t1.connected = true;
    t1.metricsSupported = false;
    t1.slots = QList<SlotInfo>{s0};

    auto d1 = tracker.process(t1);
    QVERIFY(!tracker.isUsingMetrics());
    QCOMPARE(d1.completionTokens, 0ull);

    // Tick 2: s0 generated 15 tokens
    s0.decodedTokens = 35;
    LlmTelemetry t2;
    t2.connected = true;
    t2.metricsSupported = false;
    t2.slots = QList<SlotInfo>{s0};

    auto d2 = tracker.process(t2);
    QCOMPARE(d2.completionTokens, 15ull);
    QCOMPARE(d2.activeSeconds, 1u);

    // Tick 3: s0 was reset for new generation, now has decodedTokens = 5
    s0.decodedTokens = 5;
    s0.promptTokens = 120;
    LlmTelemetry t3;
    t3.connected = true;
    t3.metricsSupported = false;
    t3.slots = QList<SlotInfo>{s0};

    auto d3 = tracker.process(t3);
    QCOMPARE(d3.completionTokens, 5ull);
    QCOMPARE(d3.promptTokens, 20ull);
}

void TstTokenStore::testStreakCalculations() {
    QDate today(2026, 9, 18);

    // Case 1: Active today + yesterday + 2 days ago = streak 3
    QSet<int> activeDays;
    activeDays.insert(TokenStore::dayLocalFromDate(today));
    activeDays.insert(TokenStore::dayLocalFromDate(today.addDays(-1)));
    activeDays.insert(TokenStore::dayLocalFromDate(today.addDays(-2)));
    QCOMPARE(TokenStore::computeCurrentStreak(activeDays, today), 3);

    // Case 2: Today not active yet, but yesterday was active -> streak 2 preserved
    activeDays.remove(TokenStore::dayLocalFromDate(today));
    QCOMPARE(TokenStore::computeCurrentStreak(activeDays, today), 2);

    // Case 3: Neither today nor yesterday active -> streak 0
    activeDays.remove(TokenStore::dayLocalFromDate(today.addDays(-1)));
    QCOMPARE(TokenStore::computeCurrentStreak(activeDays, today), 0);

    // Longest streak
    QList<int> sortedDays = {
        20260101, 20260102, 20260103, // 3 days
        20260201, 20260202, 20260203, 20260204, 20260205 // 5 days
    };
    QCOMPARE(TokenStore::computeLongestStreak(sortedDays), 5);
}

void TstTokenStore::testQuantileLeveling() {
    quint64 q1 = 1000;
    quint64 q2 = 5000;
    quint64 q3 = 10000;

    QCOMPARE(TokenStore::computeLevel(0, q1, q2, q3), 0);
    QCOMPARE(TokenStore::computeLevel(500, q1, q2, q3), 1);
    QCOMPARE(TokenStore::computeLevel(2000, q1, q2, q3), 2);
    QCOMPARE(TokenStore::computeLevel(8000, q1, q2, q3), 3);
    QCOMPARE(TokenStore::computeLevel(15000, q1, q2, q3), 4);
}

void TstTokenStore::testDatabasePersistenceAndQueries() {
    InMemoryDatabase db;
    QVERIFY(db.open(""));

    TokenStore store;
    QVERIFY(store.ensureSchema(db));

    // Record tokens and flush
    store.recordTokens(500, 1500, 120);
    QVERIFY(store.flush(db));

    // Summary assertions
    auto sum = store.summary(db);
    QCOMPARE(sum.totalTokens, 2000ull);
    QCOMPARE(sum.totalActiveSeconds, 120ull);
    QCOMPARE(sum.peakTokens, 2000ull);
    QCOMPARE(sum.currentStreak, 1);
    QCOMPARE(sum.longestStreak, 1);

    // Yearly heatmap assertions
    auto heatmap = store.yearlyHeatmap(db);
    QCOMPARE(heatmap.size(), 52 * 7); // 364 days
    // Last cell is today
    auto todayCell = heatmap.last().toMap();
    QCOMPARE(todayCell.value("tokens").toULongLong(), 2000ull);
    QVERIFY(todayCell.value("level").toInt() >= 1);

    // Recent trends assertions
    auto trends7 = store.recentTrends(db, 7);
    QCOMPARE(trends7.size(), 7);
    auto todayTrend = trends7.last().toMap();
    QCOMPARE(todayTrend.value("promptTokens").toULongLong(), 500ull);
    QCOMPARE(todayTrend.value("completionTokens").toULongLong(), 1500ull);
    QCOMPARE(todayTrend.value("totalTokens").toULongLong(), 2000ull);
}

QTEST_MAIN(TstTokenStore)
#include "tst_token_store.moc"

