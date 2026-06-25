// HelloPlugin — minimal reference plugin (M0-C6).
// Spec: docs/04-plugin-spec.md §8 (Hello example).
//
// Contributes a single tray menu item "Say Hello". On click: log via the
// injected HostServices and publish margin.hello.ping.

#pragma once

#include "Margin/PluginInterface.h"
#include "Margin/PluginContext.h"
#include "Margin/Result.h"
#include "Margin/TrayMenuContributor.h"

namespace Margin::Plugins::Hello {

class HelloPlugin : public PluginInterface, public TrayMenuContributor {
public:
    std::string id() const override;
    std::string version() const override;
    Result<void, std::string> onLoad(const PluginContext& ctx) override;
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override;

    // TrayMenuContributor
    TrayMenuContributor* asTrayMenu() override { return this; }
    QList<TrayItem> contributeTrayItems() override;
    void onTrayItemClicked(const std::string& id) override;

private:
    PluginContext m_ctx;
};

} // namespace Margin::Plugins::Hello
