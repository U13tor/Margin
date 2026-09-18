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

namespace Margin {
class Database;
}

namespace Margin::Plugins::LlamaPet {

class TokenStore;

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

    Q_PROPERTY(int hudSubView READ hudSubView WRITE setHudSubView NOTIFY hudSubViewChanged)

    // QML 交互接口
    Q_INVOKABLE void openDetail();
    Q_INVOKABLE void openStats();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void copyRestartCommand();
    Q_INVOKABLE void clearIdleKv();
    Q_INVOKABLE void switchForm(const QString& form);
    Q_INVOKABLE void setAlwaysOnTop(bool onTop);
    Q_INVOKABLE void setClickThrough(bool through);
    Q_INVOKABLE void setAutoDockHide(bool autoHide);

    // Token 统计与活动热力图接口
    Q_INVOKABLE QVariantMap tokenSummary();
    Q_INVOKABLE QVariantList tokenHeatmap();
    Q_INVOKABLE QVariantList tokenTrends(int days = 7);
    Q_INVOKABLE bool isUsingMetrics() const;

    TelemetryService* telemetryService() { return m_service.get(); }
    FloatingWindows* floatingWindows() { return m_floatingWindows.get(); }
    DockBehavior* dockBehavior() { return m_dockBehavior.get(); }
    HotkeyManager* hotkeyManager() { return m_hotkeyManager.get(); }
    TokenStore* tokenStore() { return m_tokenStore.get(); }

    int hudSubView() const { return m_hudSubView; }
    void setHudSubView(int view) {
        if (m_hudSubView != view) {
            m_hudSubView = view;
            Q_EMIT hudSubViewChanged();
        }
    }

Q_SIGNALS:
    void hudSubViewChanged();

private:
    void applyConfig(const EngineConfig& cfg);

    PluginContext m_ctx;
    bool m_alwaysOnTop{true};
    int m_hudSubView{0};
    Margin::Database* m_database{nullptr};
    std::unique_ptr<TokenStore> m_tokenStore;
    std::unique_ptr<TelemetryService> m_service;
    std::unique_ptr<DockBehavior> m_dockBehavior;
    std::unique_ptr<FloatingWindows> m_floatingWindows;
    std::unique_ptr<HotkeyManager> m_hotkeyManager;
};

} // namespace Margin::Plugins::LlamaPet
