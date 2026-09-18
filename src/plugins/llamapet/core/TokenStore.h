#pragma once

#include <QDate>
#include <QList>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <memory>
#include <mutex>

namespace Margin {
class Database;
}

namespace Margin::Plugins::LlamaPet {

struct ActivitySummary {
    quint64 totalTokens{0};
    quint64 peakTokens{0};
    QString peakDate;
    quint64 totalActiveSeconds{0};
    int currentStreak{0};
    int longestStreak{0};

    QVariantMap toVariantMap() const;
};

class TokenStore {
public:
    TokenStore();
    ~TokenStore();

    TokenStore(const TokenStore&) = delete;
    TokenStore& operator=(const TokenStore&) = delete;

    /// Initialize SQLite table and index. Safe to call multiple times.
    bool ensureSchema(Database& db);

    /// Record incremental tokens into in-memory buffer.
    void recordTokens(quint64 promptTokens, quint64 completionTokens, quint32 activeSeconds);

    /// Flush unflushed memory buffer into database.
    bool flush(Database& db);

    /// Query high-level activity summary.
    ActivitySummary summary(Database& db);

    /// Query 52-week heatmap grid (364 days ending today) formatted for QML.
    QVariantList yearlyHeatmap(Database& db);

    /// Query recent N days usage trends (e.g. 7 or 30 days).
    QVariantList recentTrends(Database& db, int days);

    // ── Helper Math Functions (Public for testing) ───────────────────
    static int dayLocalFromDate(const QDate& date);
    static QDate dateFromDayLocal(int dayLocal);
    static int computeCurrentStreak(const QSet<int>& activeDays, const QDate& today);
    static int computeLongestStreak(const QList<int>& sortedActiveDays);
    static int computeLevel(quint64 tokens, quint64 q1, quint64 q2, quint64 q3);

private:
    std::mutex m_mutex;
    quint64 m_unflushedPrompt{0};
    quint64 m_unflushedCompletion{0};
    quint32 m_unflushedActiveSec{0};
    int m_lastDayLocal{0};
};

} // namespace Margin::Plugins::LlamaPet

