// CategoryMatcher — process-name → category resolver for screen_time.
// Spec: docs/11-roadmap.md M2-C5.
//
// Loads a list of {regex match, category string} rules from a built-in
// JSON resource (default_categories.json, shipped in the plugin's qrc)
// and optionally merges user-supplied overrides from Settings. The
// matcher walks rules in order; first hit wins. User rules go to the
// front of the list so users can override defaults without editing the
// shipped JSON.
//
// Regex syntax: QRegularExpression default (PCRE). The matcher is
// resilient — a bad regex in user overrides is skipped (logged via
// qWarning) rather than aborting the whole load, so one typo doesn't
// kill all categorization.

#pragma once

#include <QList>
#include <QString>

#include <memory>

namespace Margin {
class Settings;
}

namespace Margin::Plugins::ScreenTime {

class CategoryMatcher {
public:
    CategoryMatcher();
    ~CategoryMatcher();

    CategoryMatcher(const CategoryMatcher&) = delete;
    CategoryMatcher& operator=(const CategoryMatcher&) = delete;

    struct Rule {
        QString matchPattern;   // regex source, e.g. "Code.exe|idea64.exe"
        QString category;       // "Development"
    };

    /// Load the built-in rules JSON from a qrc path
    /// (e.g. ":/screen_time/default_categories.json"). Returns the
    /// number of rules loaded; 0 on parse failure.
    int loadDefaults(const QString& qrcJsonPath);

    /// Merge user overrides from Settings key
    /// `plugins.screen_time.category_overrides` (a JSON-encoded array
    /// of the same {match, category} shape). User rules are prepended
    /// so they win over built-ins. Bad regexes are skipped + warned.
    /// Returns the number of user rules added.
    int loadUserOverrides(Margin::Settings& settings);

    /// Match processName against the rule list. First hit wins. Returns
    /// the category string, or QStringLiteral("Uncategorized") if no
    /// rule matches (or if no rules are loaded).
    QString match(const QString& processName) const;

    /// Read-only access to the live rule list (defaults + user, in
    /// match order). Useful for the settings UI to display the active
    /// rules.
    QList<Rule> rules() const { return m_rules; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    QList<Rule>           m_rules;
};

} // namespace Margin::Plugins::ScreenTime
