// FakePlugin impl — see fake_plugin.h.
// Reports onLoad/onUnload via EventBus so the test observes lifecycle
// without DLL-exported globals.

#include "fake_plugin.h"

#include "Margin/EventBus.h"
#include "Margin/HostServices.h"
#include "Margin/PluginEntry.h"

#include <QJsonObject>
#include <QString>

namespace Margin::TestFake {

std::string FakePlugin::id() const {
    return "fake";
}

std::string FakePlugin::version() const {
    return "0.1.0";
}

Result<void, std::string> FakePlugin::onLoad(const PluginContext& ctx) {
    m_ctx = ctx;
    if (m_ctx.host) {
        m_ctx.host->eventBus().publish(
            QStringLiteral("margin.fake.loaded"), QJsonObject{});
    }
    return Result<void, std::string>::ok();
}

void FakePlugin::onUnload() {
    if (m_ctx.host) {
        m_ctx.host->eventBus().publish(
            QStringLiteral("margin.fake.unloaded"), QJsonObject{});
    }
}

} // namespace Margin::TestFake

MARGIN_PLUGIN_ENTRY(Margin::TestFake::FakePlugin)
