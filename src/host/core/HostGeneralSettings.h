// HostGeneralSettings — QML bridge over the host Settings service for keys
// shown on the Settings → General page (docs/06 §4.6, M5-C4d).
//
// Why a bridge: HostCore is a plain C++ class (not QObject) and Settings is
// a pure interface (no Q_PROPERTY). To bind a QML dropdown to a host-owned
// setting we need a QObject that exposes a NOTIFY-ing Q_PROPERTY and writes
// through to Settings on set. This is that QObject — one property today
// (logLevel); grows as the General page adds toggles.
//
// Logger coupling is intentionally absent: setLogLevel() writes the new
// value to Settings and emits logLevelChanged. HostCore subscribes to
// Settings::onChange("general.log_level") and runs applyLogLevel() in
// response — so the level actually takes effect without restart, but the
// HostGeneralSettings class stays focused on QML ↔ Settings translation.

#pragma once

#include <QObject>
#include <QString>

namespace Margin {

class Settings;

class HostGeneralSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString logLevel READ logLevel WRITE setLogLevel
               NOTIFY logLevelChanged)
    // PR6 round-2 #7: "auto" follows system locale (default), "zh_CN" /
    // "en" force one of the bundled translations. HostCore subscribes to
    // Settings::onChange("general.language") and swaps QTranslator +
    // retranslates QML without restart.
    Q_PROPERTY(QString language READ language WRITE setLanguage
               NOTIFY languageChanged)
    // Autostart toggle (Win-only in v1.0). Persisted to general.auto_start;
    // HostCore subscribes and pushes to PlatformBackend -> Windows Run key.
    // On Mac/Linux the platform backend is nullptr so the toggle has no
    // effect even though the preference persists — matches the existing
    // logLevel/language pattern (Settings is source of truth).
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart
               NOTIFY autoStartChanged)

public:
    explicit HostGeneralSettings(Settings& settings, QObject* parent = nullptr);

    QString logLevel() const { return m_logLevel; }
    void setLogLevel(const QString& level);

    QString language() const { return m_language; }
    void setLanguage(const QString& language);

    bool autoStart() const { return m_autoStart; }
    void setAutoStart(bool enabled);

signals:
    void logLevelChanged();
    void languageChanged();
    void autoStartChanged();

private:
    Settings& m_settings;
    QString   m_logLevel;  // "Debug" / "Info" / "Warn" / "Error" / "Fatal"
    QString   m_language;  // "auto" / "zh_CN" / "en"
    bool      m_autoStart = false;
};

} // namespace Margin
