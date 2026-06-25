// HelloPlugin impl — see HelloPlugin.h.

#include "HelloPlugin.h"

#include "Margin/EventBus.h"
#include "Margin/HostServices.h"
#include "Margin/Logger.h"

#include <QJsonObject>
#include <QString>

namespace Margin::Plugins::Hello {

namespace {
constexpr const char* kId       = "hello";
constexpr const char* kVersion  = "0.1.0";
constexpr const char* kTag      = "hello";
constexpr const char* kSayHello = "say_hello";
}

std::string HelloPlugin::id() const      { return kId; }
std::string HelloPlugin::version() const { return kVersion; }

Result<void, std::string> HelloPlugin::onLoad(const PluginContext& ctx) {
    m_ctx = ctx;
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("HelloPlugin onLoad"));
    }
    return Result<void, std::string>::ok();
}

void HelloPlugin::onUnload() {
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("HelloPlugin onUnload"));
    }
}

QList<TrayMenuContributor::TrayItem> HelloPlugin::contributeTrayItems() {
    TrayMenuContributor::TrayItem item;
    item.id    = kSayHello;
    item.label = "Say Hello";
    return { item };
}

void HelloPlugin::onTrayItemClicked(const std::string& id) {
    if (id != kSayHello) return;
    if (!m_ctx.host) return;

    m_ctx.host->logger().info(
        QString::fromLatin1(kTag),
        QStringLiteral("Hello from HelloPlugin"));

    m_ctx.host->eventBus().publish(
        QStringLiteral("margin.hello.ping"),
        QJsonObject{ {"msg", QStringLiteral("hello")} });
}

} // namespace Margin::Plugins::Hello
