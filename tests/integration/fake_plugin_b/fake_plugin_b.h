// Second PluginManager test fixture. Manifest priority 10 (loads before the
// priority-100 "fake" fixture) even though "zlast" sorts AFTER "fake"
// alphabetically — proves loadAll honors manifest priority, not discovery
// order. Reports lifecycle via EventBus (margin.zlast.loaded / .unloaded).

#pragma once

#include "Margin/PluginInterface.h"

namespace Margin::TestFakeB {

class FakePluginB : public PluginInterface {
public:
    std::string id() const override;
    std::string version() const override;
    Result<void, std::string> onLoad(const PluginContext& ctx) override;
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override;

private:
    PluginContext m_ctx;
};

} // namespace Margin::TestFakeB
