#pragma once

#include <QAbstractNativeEventFilter>
#include <QMap>
#include <QObject>
#include <QString>

namespace Margin::Plugins::LlamaPet {

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    static bool parseHotkeyString(const QString& str, quint32& outMods, quint32& outVk);

    bool registerHotkey(int id, const QString& sequence);
    bool registerHotkey(int id, quint32 mods, quint32 vk);
    void unregisterHotkey(int id);
    void unregisterAll();

    bool isRegistered(int id) const { return m_registered.contains(id); }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

Q_SIGNALS:
    void hotkeyTriggered(int id);

private:
    bool m_filterInstalled{false};
    QMap<int, QPair<quint32, quint32>> m_registered;
};

} // namespace Margin::Plugins::LlamaPet
