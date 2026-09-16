#include "HotkeyManager.h"
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Margin::Plugins::LlamaPet {

namespace {
#ifdef Q_OS_WIN
constexpr const char* kNativeEventType = "windows_generic_MSG";
#endif
} // namespace

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    if (auto* app = QCoreApplication::instance()) {
        app->installNativeEventFilter(this);
        m_filterInstalled = true;
    }
}

HotkeyManager::~HotkeyManager() {
    unregisterAll();
    if (m_filterInstalled) {
        if (auto* app = QCoreApplication::instance()) {
            app->removeNativeEventFilter(this);
        }
    }
}

bool HotkeyManager::parseHotkeyString(const QString& str, quint32& outMods, quint32& outVk) {
    outMods = 0;
    outVk = 0;
    const QString trimmed = str.trimmed();
    if (trimmed.isEmpty()) return false;

    const QStringList parts = trimmed.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    for (int i = 0; i < parts.size(); ++i) {
        const QString part = parts.at(i).trimmed();
        const QString lower = part.toLower();

        if (i < parts.size() - 1) {
            // 修饰键
            if (lower == QStringLiteral("ctrl") || lower == QStringLiteral("control")) {
#ifdef Q_OS_WIN
                outMods |= MOD_CONTROL;
#else
                outMods |= 0x0002;
#endif
            } else if (lower == QStringLiteral("alt")) {
#ifdef Q_OS_WIN
                outMods |= MOD_ALT;
#else
                outMods |= 0x0001;
#endif
            } else if (lower == QStringLiteral("shift")) {
#ifdef Q_OS_WIN
                outMods |= MOD_SHIFT;
#else
                outMods |= 0x0004;
#endif
            } else if (lower == QStringLiteral("win") || lower == QStringLiteral("meta")) {
#ifdef Q_OS_WIN
                outMods |= MOD_WIN;
#else
                outMods |= 0x0008;
#endif
            } else {
                return false;
            }
        } else {
            // 主键
            if (part.length() == 1) {
                const QChar ch = part.at(0).toUpper();
                if ((ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) ||
                    (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))) {
                    outVk = static_cast<quint32>(ch.toLatin1());
                } else {
                    return false;
                }
            } else if (lower.startsWith(QLatin1Char('f')) && lower.length() >= 2) {
                bool ok = false;
                int fNum = lower.mid(1).toInt(&ok);
                if (ok && fNum >= 1 && fNum <= 24) {
#ifdef Q_OS_WIN
                    outVk = VK_F1 + (fNum - 1);
#else
                    outVk = 0x70 + (fNum - 1);
#endif
                } else {
                    return false;
                }
            } else if (lower == QStringLiteral("space")) {
#ifdef Q_OS_WIN
                outVk = VK_SPACE;
#else
                outVk = 0x20;
#endif
            } else if (lower == QStringLiteral("esc") || lower == QStringLiteral("escape")) {
#ifdef Q_OS_WIN
                outVk = VK_ESCAPE;
#else
                outVk = 0x1B;
#endif
            } else {
                return false;
            }
        }
    }

    return (outVk != 0);
}

bool HotkeyManager::registerHotkey(int id, const QString& sequence) {
    quint32 mods = 0;
    quint32 vk = 0;
    if (!parseHotkeyString(sequence, mods, vk)) {
        return false;
    }
    return registerHotkey(id, mods, vk);
}

bool HotkeyManager::registerHotkey(int id, quint32 mods, quint32 vk) {
    unregisterHotkey(id); // 保证先注销旧热键

#ifdef Q_OS_WIN
    // 默认加上 MOD_NOREPEAT (0x4000) 避免长按导致暴风连击
    UINT winMods = static_cast<UINT>(mods | 0x4000);
    BOOL ok = RegisterHotKey(nullptr, id, winMods, static_cast<UINT>(vk));
    if (!ok) {
        // 部分旧 Windows 可能不支持 MOD_NOREPEAT，降级重试
        ok = RegisterHotKey(nullptr, id, static_cast<UINT>(mods), static_cast<UINT>(vk));
    }
    if (ok) {
        m_registered.insert(id, qMakePair(mods, vk));
        return true;
    }
    return false;
#else
    m_registered.insert(id, qMakePair(mods, vk));
    return true;
#endif
}

void HotkeyManager::unregisterHotkey(int id) {
    if (!m_registered.contains(id)) return;
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, id);
#endif
    m_registered.remove(id);
}

void HotkeyManager::unregisterAll() {
#ifdef Q_OS_WIN
    for (auto it = m_registered.constBegin(); it != m_registered.constEnd(); ++it) {
        UnregisterHotKey(nullptr, it.key());
    }
#endif
    m_registered.clear();
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/) {
#ifdef Q_OS_WIN
    if (eventType != kNativeEventType || !message) {
        return false;
    }
    const MSG* msg = static_cast<const MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        int id = static_cast<int>(msg->wParam);
        if (m_registered.contains(id)) {
            Q_EMIT hotkeyTriggered(id);
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

} // namespace Margin::Plugins::LlamaPet
