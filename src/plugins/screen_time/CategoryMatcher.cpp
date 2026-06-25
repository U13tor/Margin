// CategoryMatcher impl — see CategoryMatcher.h.

#include "CategoryMatcher.h"

#include "Margin/Settings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace Margin::Plugins::ScreenTime {

namespace {
constexpr const char* kUncategorized = "Uncategorized";

// Compile one rule's pattern into a QRegularExpression. Returns
// whether the pattern is valid; if not, *outErr gets the error message.
QRegularExpression compilePattern(const QString& pattern, QString* outErr = nullptr) {
    // Anchored + case-insensitive. Anchored because "Code.exe" should
    // match exactly "Code.exe", not "VSCode.exe-loader". Case-insensitive
    // because Windows paths are case-preserving but case-insensitive.
    QRegularExpression re(QRegularExpression::anchoredPattern(pattern),
                          QRegularExpression::CaseInsensitiveOption);
    if (!re.isValid() && outErr) *outErr = re.errorString();
    return re;
}
} // namespace

struct CategoryMatcher::Impl {
    // Pre-compiled QRegularExpression parallel to m_rules. Each entry
    // is the compiled form of m_rules[i].matchPattern. Rule lookups
    // hit this list rather than re-compiling per match() call.
    QList<QRegularExpression> compiled;
};

CategoryMatcher::CategoryMatcher()
    : m_impl(std::make_unique<Impl>()) {}

CategoryMatcher::~CategoryMatcher() = default;

int CategoryMatcher::loadDefaults(const QString& qrcJsonPath) {
    QFile f(qrcJsonPath);
    if (!f.open(QIODevice::ReadOnly)) return 0;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return 0;
    const QJsonArray rules = doc.object().value(QStringLiteral("rules")).toArray();

    int added = 0;
    for (const QJsonValue& v : rules) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        Rule r{ obj.value(QStringLiteral("match")).toString(),
                obj.value(QStringLiteral("category")).toString() };
        if (r.matchPattern.isEmpty() || r.category.isEmpty()) continue;

        QString err;
        QRegularExpression re = compilePattern(r.matchPattern, &err);
        if (!re.isValid()) {
            qWarning("CategoryMatcher: skipping bad default regex '%s': %s",
                     qPrintable(r.matchPattern), qPrintable(err));
            continue;
        }
        m_rules.append(r);
        m_impl->compiled.append(re);
        ++added;
    }
    return added;
}

int CategoryMatcher::loadUserOverrides(Margin::Settings& settings) {
    const QString kKey = QStringLiteral("plugins.screen_time.category_overrides");

    // Empty / unset → no overrides, no work.
    const QString raw = settings.get(kKey, QVariant(QString())).toString();
    if (raw.trimmed().isEmpty()) return 0;

    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray()) {
        qWarning("CategoryMatcher: user overrides present but not a JSON array — ignoring");
        return 0;
    }
    const QJsonArray arr = doc.array();

    QList<Rule> userRules;
    QList<QRegularExpression> userCompiled;
    int added = 0;
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        Rule r{ obj.value(QStringLiteral("match")).toString(),
                obj.value(QStringLiteral("category")).toString() };
        if (r.matchPattern.isEmpty() || r.category.isEmpty()) continue;

        QString err;
        QRegularExpression re = compilePattern(r.matchPattern, &err);
        if (!re.isValid()) {
            qWarning("CategoryMatcher: skipping bad user regex '%s': %s",
                     qPrintable(r.matchPattern), qPrintable(err));
            continue;
        }
        userRules.append(r);
        userCompiled.append(re);
        ++added;
    }

    if (added == 0) return 0;

    // Prepend user rules to the live list so they win on first match.
    // We rebuild both m_rules and compiled list to keep them in lock-step.
    QList<Rule> mergedRules;
    mergedRules.reserve(userRules.size() + m_rules.size());
    mergedRules.append(userRules);
    mergedRules.append(m_rules);

    QList<QRegularExpression> mergedCompiled;
    mergedCompiled.reserve(userCompiled.size() + m_impl->compiled.size());
    mergedCompiled.append(userCompiled);
    mergedCompiled.append(m_impl->compiled);

    m_rules = std::move(mergedRules);
    m_impl->compiled = std::move(mergedCompiled);
    return added;
}

QString CategoryMatcher::match(const QString& processName) const {
    if (processName.isEmpty()) return QString::fromLatin1(kUncategorized);

    for (int i = 0; i < m_rules.size() && i < m_impl->compiled.size(); ++i) {
        if (m_impl->compiled[i].match(processName).hasMatch()) {
            return m_rules[i].category;
        }
    }
    return QString::fromLatin1(kUncategorized);
}

} // namespace Margin::Plugins::ScreenTime
