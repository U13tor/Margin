// HostGeneralSettings impl — see header.

#include "host/core/HostGeneralSettings.h"

#include "Margin/Settings.h"

#include <QLatin1String>
#include <QVariant>

namespace Margin {

namespace {
constexpr const char* kKeyLogLevel   = "general.log_level";
constexpr const char* kKeyLanguage   = "general.language";
constexpr const char* kKeyAutoStart  = "general.auto_start";
}  // namespace

HostGeneralSettings::HostGeneralSettings(Settings& settings, QObject* parent)
    : QObject(parent), m_settings(settings) {
    // Read once at construction; subsequent writes from QML persist + emit.
    // Default "Info" matches HostCore::applyLogLevel's default fallback.
    m_logLevel = m_settings.get(QLatin1String(kKeyLogLevel),
                                QVariant(QStringLiteral("Info"))).toString();
    if (m_logLevel.isEmpty()) m_logLevel = QStringLiteral("Info");
    // PR6 round-2 #7: default "auto" — QLocale::system().name() decides
    // at translator-install time. Explicit zh_CN / en override the detect.
    m_language = m_settings.get(QLatin1String(kKeyLanguage),
                                QVariant(QStringLiteral("auto"))).toString();
    if (m_language.isEmpty()) m_language = QStringLiteral("auto");
    // Default false — opt-in. HostCore's bootstrap pushes this value to
    // PlatformBackend so the OS registry stays in sync on every launch.
    m_autoStart = m_settings.get(QLatin1String(kKeyAutoStart),
                                 QVariant(false)).toBool();
}

void HostGeneralSettings::setLogLevel(const QString& level) {
    // Normalize to one of the 5 known values; reject silently otherwise
    // (QML dropdown only offers these 5 so a stray value would be a bug
    // elsewhere — defensive only). Match HostCore::applyLogLevel's
    // case-insensitive comparison.
    const QString norm = level.toLower();
    QString canonical;
    if      (norm == QLatin1String("debug")) canonical = QStringLiteral("Debug");
    else if (norm == QLatin1String("info"))  canonical = QStringLiteral("Info");
    else if (norm == QLatin1String("warn"))  canonical = QStringLiteral("Warn");
    else if (norm == QLatin1String("error")) canonical = QStringLiteral("Error");
    else if (norm == QLatin1String("fatal")) canonical = QStringLiteral("Fatal");
    else return;

    if (canonical == m_logLevel) return;
    m_logLevel = canonical;
    m_settings.set(QLatin1String(kKeyLogLevel), QVariant(m_logLevel));
    // HostCore subscribes to Settings::onChange for this key and runs
    // applyLogLevel() — effect is immediate, no restart.
    emit logLevelChanged();
}

void HostGeneralSettings::setLanguage(const QString& language) {
    // Canonicalize the three known values. Reject anything else silently
    // — the segmented-row QML only offers these 3, so a stray value
    // would be a bug elsewhere.
    QString canonical;
    const QString norm = language.toLower();
    if      (norm == QLatin1String("auto"))   canonical = QStringLiteral("auto");
    else if (norm == QLatin1String("zh_cn") ||
             norm == QLatin1String("zh-cn") ||
             norm == QLatin1String("zh"))    canonical = QStringLiteral("zh_CN");
    else if (norm == QLatin1String("en"))     canonical = QStringLiteral("en");
    else return;

    if (canonical == m_language) return;
    m_language = canonical;
    m_settings.set(QLatin1String(kKeyLanguage), QVariant(m_language));
    // HostCore subscribes to Settings::onChange for this key and swaps
    // QTranslator + retranslates the QML engine — effect is immediate.
    emit languageChanged();
}

void HostGeneralSettings::setAutoStart(bool enabled) {
    if (enabled == m_autoStart) return;
    m_autoStart = enabled;
    m_settings.set(QLatin1String(kKeyAutoStart), QVariant(m_autoStart));
    // HostCore subscribes to Settings::onChange for this key and calls
    // PlatformBackend::setAutoStartEnabled — registry is updated immediately.
    emit autoStartChanged();
}

} // namespace Margin
