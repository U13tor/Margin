#pragma once

#include "Margin/DashboardTabContributor.h"
#include "Margin/PluginContext.h"
#include "Margin/PluginInterface.h"
#include "Margin/Result.h"
#include "Margin/SettingsPageContributor.h"
#include "Margin/TrayMenuContributor.h"
#include "app/DockBehavior.h"
#include "app/FloatingWindows.h"
#include "app/HotkeyManager.h"
#include "app/TelemetryService.h"

#include <QObject>
#include <memory>
#include <string>

namespace Margin::Plugins::LlamaPet {

class LlamaPetPlugin : public QObject,
                       public PluginInterface,
                       public TrayMenuContributor,
                       public DashboardTabContributor,
                       public SettingsPageContributor {
    Q_OBJECT

public:
    explicit LlamaPetPlugin(QObject* parent = nullptr);
    ~LlamaPetPlugin() override;

    // PluginInterface
    std::string id() const override;
    std::string version() const override;
    Result<void, std::string> onLoad(const PluginContext& ctx) override;
    void onConfigChange(const QJsonObject& config) override;
    void onUnload() override;

    DashboardTabContributor* asDashboardTab() override { return this; }
    SettingsPageContributor* asSettingsPage() override { return this; }
    TrayMenuContributor* asTrayMenu() override { return this; }

    // DashboardTabContributor
    TabInfo tabInfo() const override;

    // SettingsPageContributor
    PageInfo pageInfo() const override;

    // TrayMenuContributor
    QList<TrayItem> contributeTrayItems() override;
    void onTrayItemClicked(const std::string& id) override;

    // QML 交互接口
    Q_INVOKABLE void copyRestartCommand();
    Q_INVOKABLE void clearIdleKv();
    Q_INVOKABLE void switchForm(const QString& form);
    Q_INVOKABLE void setAlwaysOnTop(bool onTop);
    Q_INVOKABLE void setClickThrough(bool through);
    Q_INVOKABLE void setAutoDockHide(bool autoHide);

    TelemetryService* telemetryService() { return m_service.get(); }
    FloatingWindows* floatingWindows() { return m_floatingWindows.get(); }
    DockBehavior* dockBehavior() { return m_dockBehavior.get(); }
    HotkeyManager* hotkeyManager() { return m_hotkeyManager.get(); }

private:
    void applyConfig(const EngineConfig& cfg);

    PluginContext m_ctx;
    bool m_alwaysOnTop{true};
    std::unique_ptr<TelemetryService> m_service;
    std::unique_ptr<DockBehavior> m_dockBehavior;
    std::unique_ptr<FloatingWindows> m_floatingWindows;
    std::unique_ptr<HotkeyManager> m_hotkeyManager;
};

} // namespace Margin::Plugins::LlamaPet
