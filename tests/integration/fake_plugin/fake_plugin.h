// Minimal PluginInterface impl for the PluginManager integration test.
// Reports lifecycle via EventBus (margin.fake.loaded / .unloaded) so the
// test can observe onLoad/onUnload without needing DLL-exported globals.

#pragma once

#include "Margin/PluginInterface.h"

namespace Margin::TestFake {

class FakePlugin : public PluginInterface {
public:
    std::string id() const override;
    std::string version() const override;
    Result<void, std::string> onLoad(const PluginContext& ctx) override;
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override;

private:
    PluginContext m_ctx;
};

} // namespace Margin::TestFake
