#include <QtTest>
#include "app/HotkeyManager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace Margin::Plugins::LlamaPet;

class TstHotkey : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testParseHotkeyString();
    void testParseInvalidHotkeys();
    void testRegisterAndUnregister();
};

void TstHotkey::testParseHotkeyString() {
    quint32 mods = 0;
    quint32 vk = 0;

    // 默认热键 Ctrl+Alt+P
    QVERIFY(HotkeyManager::parseHotkeyString(QStringLiteral("Ctrl+Alt+P"), mods, vk));
#ifdef Q_OS_WIN
    QCOMPARE(mods, static_cast<quint32>(MOD_CONTROL | MOD_ALT));
    QCOMPARE(vk, static_cast<quint32>('P'));
#endif

    // Ctrl+Shift+F1
    QVERIFY(HotkeyManager::parseHotkeyString(QStringLiteral("Ctrl+Shift+F1"), mods, vk));
#ifdef Q_OS_WIN
    QCOMPARE(mods, static_cast<quint32>(MOD_CONTROL | MOD_SHIFT));
    QCOMPARE(vk, static_cast<quint32>(VK_F1));
#endif

    // Alt+Space
    QVERIFY(HotkeyManager::parseHotkeyString(QStringLiteral("Alt+Space"), mods, vk));
#ifdef Q_OS_WIN
    QCOMPARE(mods, static_cast<quint32>(MOD_ALT));
    QCOMPARE(vk, static_cast<quint32>(VK_SPACE));
#endif
}

void TstHotkey::testParseInvalidHotkeys() {
    quint32 mods = 0;
    quint32 vk = 0;

    QVERIFY(!HotkeyManager::parseHotkeyString(QStringLiteral(""), mods, vk));
    QVERIFY(!HotkeyManager::parseHotkeyString(QStringLiteral("Invalid+Key"), mods, vk));
    QVERIFY(!HotkeyManager::parseHotkeyString(QStringLiteral("Ctrl+"), mods, vk));
    QVERIFY(!HotkeyManager::parseHotkeyString(QStringLiteral("F25"), mods, vk));
}

void TstHotkey::testRegisterAndUnregister() {
    HotkeyManager mgr;
    // 注册非冲突热键测试
    bool reg = mgr.registerHotkey(101, QStringLiteral("Ctrl+Alt+F11"));
    // 在 CI 或无头环境中可能无法注册成功或成功，但无论如何不崩溃
    if (reg) {
        QVERIFY(mgr.isRegistered(101));
        mgr.unregisterHotkey(101);
        QVERIFY(!mgr.isRegistered(101));
    }
}

QTEST_MAIN(TstHotkey)
#include "tst_hotkey.moc"
