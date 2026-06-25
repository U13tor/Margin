// FakePluginB impl — see fake_plugin_b.h.

#include "fake_plugin_b.h"

#include "Margin/EventBus.h"
#include "Margin/HostServices.h"
#include "Margin/PluginEntry.h"

#include <QJsonObject>
#include <QString>

namespace Margin::TestFakeB {

std::string FakePluginB::id() const {
    return "zlast";
}

std::string FakePluginB::version() const {
    return "0.1.0";
}

Result<void, std::string> FakePluginB::onLoad(const PluginContext& ctx) {
    m_ctx = ctx;
    if (m_ctx.host) {
        m_ctx.host->eventBus().publish(
            QStringLiteral("margin.zlast.loaded"), QJsonObject{});
    }
    return Result<void, std::string>::ok();
}

void FakePluginB::onUnload() {
    if (m_ctx.host) {
        m_ctx.host->eventBus().publish(
            QStringLiteral("margin.zlast.unloaded"), QJsonObject{});
    }
}

} // namespace Margin::TestFakeB

MARGIN_PLUGIN_ENTRY(Margin::TestFakeB::FakePluginB)
