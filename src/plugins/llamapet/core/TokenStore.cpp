#include "TokenStore.h"
#include "Margin/Database.h"

#include <QDateTime>
#include <algorithm>
#include <vector>

namespace Margin::Plugins::LlamaPet {

QVariantMap ActivitySummary::toVariantMap() const {
    QVariantMap map;
    map[QStringLiteral("totalTokens")] = static_cast<qulonglong>(totalTokens);
    map[QStringLiteral("peakTokens")] = static_cast<qulonglong>(peakTokens);
    map[QStringLiteral("peakDate")] = peakDate;
    map[QStringLiteral("totalActiveSeconds")] = static_cast<qulonglong>(totalActiveSeconds);
    map[QStringLiteral("currentStreak")] = currentStreak;
    map[QStringLiteral("longestStreak")] = longestStreak;
    return map;
}

TokenStore::TokenStore() = default;
TokenStore::~TokenStore() = default;

int TokenStore::dayLocalFromDate(const QDate& date) {
    return date.year() * 10000 + date.month() * 100 + date.day();
}

QDate TokenStore::dateFromDayLocal(int dayLocal) {
    int y = dayLocal / 10000;
    int m = (dayLocal % 10000) / 100;
    int d = dayLocal % 100;
    return QDate(y, m, d);
}

bool TokenStore::ensureSchema(Database& db) {
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS llamapet_token_daily ("
        "  day_local INTEGER PRIMARY KEY,"
        "  date_str TEXT NOT NULL,"
        "  prompt_tokens INTEGER NOT NULL DEFAULT 0,"
        "  completion_tokens INTEGER NOT NULL DEFAULT 0,"
        "  total_tokens INTEGER NOT NULL DEFAULT 0,"
        "  active_seconds INTEGER NOT NULL DEFAULT 0,"
        "  updated_at INTEGER NOT NULL"
        ");"
    );
    return db.exec(sql);
}

void TokenStore::recordTokens(quint64 promptTokens, quint64 completionTokens, quint32 activeSeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_unflushedPrompt += promptTokens;
    m_unflushedCompletion += completionTokens;
    m_unflushedActiveSec += activeSeconds;
}

bool TokenStore::flush(Database& db) {
    quint64 prompt = 0;
    quint64 completion = 0;
    quint32 active = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_unflushedPrompt == 0 && m_unflushedCompletion == 0 && m_unflushedActiveSec == 0) {
            return true;
        }
        prompt = m_unflushedPrompt;
        completion = m_unflushedCompletion;
        active = m_unflushedActiveSec;

        m_unflushedPrompt = 0;
        m_unflushedCompletion = 0;
        m_unflushedActiveSec = 0;
    }

    const QDate today = QDate::currentDate();
    const int dayLocal = dayLocalFromDate(today);
    const QString dateStr = today.toString(QStringLiteral("yyyy-MM-dd"));
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const quint64 total = prompt + completion;

    const QString sql = QStringLiteral(
        "INSERT INTO llamapet_token_daily ("
        "  day_local, date_str, prompt_tokens, completion_tokens, total_tokens, active_seconds, updated_at"
        ") VALUES ("
        "  :day_local, :date_str, :prompt, :completion, :total, :active, :updated_at"
        ") ON CONFLICT(day_local) DO UPDATE SET "
        "  prompt_tokens = prompt_tokens + excluded.prompt_tokens, "
        "  completion_tokens = completion_tokens + excluded.completion_tokens, "
        "  total_tokens = total_tokens + excluded.total_tokens, "
        "  active_seconds = active_seconds + excluded.active_seconds, "
        "  updated_at = excluded.updated_at;"
    );

    QVariantMap params;
    params[QStringLiteral("day_local")] = dayLocal;
    params[QStringLiteral("date_str")] = dateStr;
    params[QStringLiteral("prompt")] = static_cast<qulonglong>(prompt);
    params[QStringLiteral("completion")] = static_cast<qulonglong>(completion);
    params[QStringLiteral("total")] = static_cast<qulonglong>(total);
    params[QStringLiteral("active")] = static_cast<qulonglong>(active);
    params[QStringLiteral("updated_at")] = nowMs;

    return db.exec(sql, params);
}

int TokenStore::computeCurrentStreak(const QSet<int>& activeDays, const QDate& today) {
    if (activeDays.isEmpty()) {
        return 0;
    }

    int todayKey = dayLocalFromDate(today);
    int streak = 0;
    QDate cur = today;

    // If today has recorded activity, count today and walk backward
    if (activeDays.contains(todayKey)) {
        streak++;
        cur = cur.addDays(-1);
        while (activeDays.contains(dayLocalFromDate(cur))) {
            streak++;
            cur = cur.addDays(-1);
        }
        return streak;
    }

    // If today has no activity yet, check yesterday to preserve ongoing streak
    QDate yesterday = today.addDays(-1);
    if (activeDays.contains(dayLocalFromDate(yesterday))) {
        streak++;
        cur = yesterday.addDays(-1);
        while (activeDays.contains(dayLocalFromDate(cur))) {
            streak++;
            cur = cur.addDays(-1);
        }
        return streak;
    }

    return 0;
}

int TokenStore::computeLongestStreak(const QList<int>& sortedActiveDays) {
    if (sortedActiveDays.isEmpty()) {
        return 0;
    }

    int longest = 1;
    int current = 1;

    for (int i = 1; i < sortedActiveDays.size(); ++i) {
        QDate prevDate = dateFromDayLocal(sortedActiveDays[i - 1]);
        QDate curDate = dateFromDayLocal(sortedActiveDays[i]);

        if (prevDate.addDays(1) == curDate) {
            current++;
            if (current > longest) {
                longest = current;
            }
        } else if (prevDate != curDate) {
            current = 1;
        }
    }

    return longest;
}

int TokenStore::computeLevel(quint64 tokens, quint64 q1, quint64 q2, quint64 q3) {
    if (tokens == 0) return 0;
    if (tokens < q1) return 1;
    if (tokens < q2) return 2;
    if (tokens < q3) return 3;
    return 4;
}

ActivitySummary TokenStore::summary(Database& db) {
    flush(db);

    ActivitySummary sum;

    // 1. Totals
    const QString sumSql = QStringLiteral(
        "SELECT SUM(total_tokens) AS sum_tokens, SUM(active_seconds) AS sum_sec FROM llamapet_token_daily;"
    );
    auto sumRows = db.query(sumSql);
    if (!sumRows.isEmpty()) {
        sum.totalTokens = sumRows[0].value(QStringLiteral("sum_tokens")).toULongLong();
        sum.totalActiveSeconds = sumRows[0].value(QStringLiteral("sum_sec")).toULongLong();
    }

    // 2. Peak daily tokens
    const QString peakSql = QStringLiteral(
        "SELECT total_tokens, date_str FROM llamapet_token_daily ORDER BY total_tokens DESC LIMIT 1;"
    );
    auto peakRows = db.query(peakSql);
    if (!peakRows.isEmpty()) {
        sum.peakTokens = peakRows[0].value(QStringLiteral("total_tokens")).toULongLong();
        sum.peakDate = peakRows[0].value(QStringLiteral("date_str")).toString();
    }

    // 3. Active days for streaks
    const QString streakSql = QStringLiteral(
        "SELECT day_local FROM llamapet_token_daily WHERE total_tokens > 0 ORDER BY day_local ASC;"
    );
    auto streakRows = db.query(streakSql);
    QList<int> sortedDays;
    QSet<int> daySet;
    sortedDays.reserve(streakRows.size());
    daySet.reserve(streakRows.size());

    for (const auto& row : streakRows) {
        int d = row.value(QStringLiteral("day_local")).toInt();
        sortedDays.append(d);
        daySet.insert(d);
    }

    sum.currentStreak = computeCurrentStreak(daySet, QDate::currentDate());
    sum.longestStreak = computeLongestStreak(sortedDays);

    return sum;
}

QVariantList TokenStore::yearlyHeatmap(Database& db) {
    flush(db);

    const int totalDays = 52 * 7; // 364 days
    const QDate today = QDate::currentDate();
    const QDate startDate = today.addDays(-(totalDays - 1));

    const int fromDay = dayLocalFromDate(startDate);
    const int toDay = dayLocalFromDate(today);

    const QString sql = QStringLiteral(
        "SELECT day_local, date_str, prompt_tokens, completion_tokens, total_tokens, active_seconds "
        "FROM llamapet_token_daily "
        "WHERE day_local >= :from_day AND day_local <= :to_day;"
    );

    QVariantMap params;
    params[QStringLiteral("from_day")] = fromDay;
    params[QStringLiteral("to_day")] = toDay;

    auto rows = db.query(sql, params);

    struct DayRecord {
        quint64 promptTokens{0};
        quint64 completionTokens{0};
        quint64 totalTokens{0};
        quint32 activeSeconds{0};
        QString dateStr;
    };

    QHash<int, DayRecord> records;
    std::vector<quint64> nonZeroTokens;
    records.reserve(rows.size());

    for (const auto& r : rows) {
        int dayLocal = r.value(QStringLiteral("day_local")).toInt();
        DayRecord rec;
        rec.promptTokens = r.value(QStringLiteral("prompt_tokens")).toULongLong();
        rec.completionTokens = r.value(QStringLiteral("completion_tokens")).toULongLong();
        rec.totalTokens = r.value(QStringLiteral("total_tokens")).toULongLong();
        rec.activeSeconds = static_cast<quint32>(r.value(QStringLiteral("active_seconds")).toUInt());
        rec.dateStr = r.value(QStringLiteral("date_str")).toString();

        records.insert(dayLocal, rec);
        if (rec.totalTokens > 0) {
            nonZeroTokens.push_back(rec.totalTokens);
        }
    }

    std::sort(nonZeroTokens.begin(), nonZeroTokens.end());
    quint64 q1 = 1000;
    quint64 q2 = 4000;
    quint64 q3 = 10000;
    if (!nonZeroTokens.empty()) {
        size_t n = nonZeroTokens.size();
        q1 = nonZeroTokens[n / 4];
        q2 = nonZeroTokens[n / 2];
        q3 = nonZeroTokens[(n * 3) / 4];
        if (q1 == 0) q1 = 1;
        if (q2 <= q1) q2 = q1 + 1;
        if (q3 <= q2) q3 = q2 + 1;
    }

    QVariantList list;
    list.reserve(totalDays);

    for (int i = 0; i < totalDays; ++i) {
        QDate curDate = startDate.addDays(i);
        int dayLocal = dayLocalFromDate(curDate);

        DayRecord rec = records.value(dayLocal);
        int level = computeLevel(rec.totalTokens, q1, q2, q3);

        QVariantMap map;
        map[QStringLiteral("date")] = curDate.toString(QStringLiteral("yyyy-MM-dd"));
        map[QStringLiteral("dateStr")] = rec.dateStr.isEmpty() ? curDate.toString(QStringLiteral("yyyy-MM-dd")) : rec.dateStr;
        map[QStringLiteral("formattedDate")] = curDate.toString(QStringLiteral("yyyy年M月d日"));
        map[QStringLiteral("dayOfWeek")] = curDate.dayOfWeek(); // 1 = Mon .. 7 = Sun
        map[QStringLiteral("month")] = curDate.month();
        map[QStringLiteral("day")] = curDate.day();
        map[QStringLiteral("tokens")] = static_cast<qulonglong>(rec.totalTokens);
        map[QStringLiteral("promptTokens")] = static_cast<qulonglong>(rec.promptTokens);
        map[QStringLiteral("completionTokens")] = static_cast<qulonglong>(rec.completionTokens);
        map[QStringLiteral("activeSeconds")] = static_cast<qulonglong>(rec.activeSeconds);
        map[QStringLiteral("level")] = level;

        list.append(map);
    }

    return list;
}

QVariantList TokenStore::recentTrends(Database& db, int days) {
    flush(db);

    if (days <= 0) days = 7;
    const QDate today = QDate::currentDate();
    const QDate startDate = today.addDays(-(days - 1));

    const int fromDay = dayLocalFromDate(startDate);
    const int toDay = dayLocalFromDate(today);

    const QString sql = QStringLiteral(
        "SELECT day_local, date_str, prompt_tokens, completion_tokens, total_tokens "
        "FROM llamapet_token_daily "
        "WHERE day_local >= :from_day AND day_local <= :to_day;"
    );

    QVariantMap params;
    params[QStringLiteral("from_day")] = fromDay;
    params[QStringLiteral("to_day")] = toDay;

    auto rows = db.query(sql, params);

    struct DayData {
        quint64 prompt{0};
        quint64 completion{0};
        quint64 total{0};
    };
    QHash<int, DayData> dataMap;
    for (const auto& r : rows) {
        int d = r.value(QStringLiteral("day_local")).toInt();
        DayData dd;
        dd.prompt = r.value(QStringLiteral("prompt_tokens")).toULongLong();
        dd.completion = r.value(QStringLiteral("completion_tokens")).toULongLong();
        dd.total = r.value(QStringLiteral("total_tokens")).toULongLong();
        dataMap.insert(d, dd);
    }

    QVariantList result;
    result.reserve(days);

    for (int i = 0; i < days; ++i) {
        QDate cur = startDate.addDays(i);
        int dayLocal = dayLocalFromDate(cur);
        DayData dd = dataMap.value(dayLocal);

        QVariantMap m;
        m[QStringLiteral("date")] = cur.toString(QStringLiteral("yyyy-MM-dd"));
        m[QStringLiteral("displayDate")] = cur.toString(QStringLiteral("MM-dd"));
        m[QStringLiteral("formattedDate")] = cur.toString(QStringLiteral("yyyy年M月d日"));
        m[QStringLiteral("promptTokens")] = static_cast<qulonglong>(dd.prompt);
        m[QStringLiteral("completionTokens")] = static_cast<qulonglong>(dd.completion);
        m[QStringLiteral("totalTokens")] = static_cast<qulonglong>(dd.total);
        result.append(m);
    }

    return result;
}

} // namespace Margin::Plugins::LlamaPet

