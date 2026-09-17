#include "LlamaPetPlugin.h"
#include "core/HalProvider.h"
#include "core/LlamaCppIt.h"
#include "core/OllamaIt.h"
#include "core/OpenAiIt.h"
#include "Margin/EventBus.h"
#include "Margin/HostServices.h"
#include "Margin/Logger.h"
#include "Margin/QmlService.h"
#include "Margin/Settings.h"
#include "Margin/TrayService.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>

namespace Margin::Plugins::LlamaPet {

namespace {
constexpr const char* kId = "llamapet";
constexpr const char* kVersion = "0.1.0";
constexpr const char* kTag = "llamapet";
constexpr int kHotkeyToggleId = 1;
constexpr const char* kRestartCmdTemplate =
    "# 请按你的实际部署调整后执行：\n# llama-server -m <model_path> --port 8080 --slots --slot-save-path ./slot-state";
}

LlamaPetPlugin::LlamaPetPlugin(QObject* parent) : QObject(parent) {
}

LlamaPetPlugin::~LlamaPetPlugin() = default;

std::string LlamaPetPlugin::id() const {
    return kId;
}

std::string LlamaPetPlugin::version() const {
    return kVersion;
}

Result<void, std::string> LlamaPetPlugin::onLoad(const PluginContext& ctx) {
    m_ctx = ctx;
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("LlamaPetPlugin onLoad"));
    }

    EngineConfig cfg;

    // 1. 宿主 Settings 检查与外部明文 config.toml 自动迁移
    if (m_ctx.host) {
        auto& settings = m_ctx.host->settings();
        QString endpoint = settings.get(QStringLiteral("plugins.llamapet.endpointUrl")).toString();
        if (endpoint.isEmpty()) {
            if (auto tomlCfg = EngineConfig::loadFromLlamaPetToml()) {
                cfg = *tomlCfg;
                settings.set(QStringLiteral("plugins.llamapet.endpointUrl"), cfg.inference.endpointUrl);
                settings.set(QStringLiteral("plugins.llamapet.apiKey"), cfg.inference.apiKey);
                settings.set(QStringLiteral("plugins.llamapet.engine"), cfg.inference.engine);
                settings.set(QStringLiteral("plugins.llamapet.pollIntervalMs"), cfg.inference.pollIntervalMs);
                settings.set(QStringLiteral("plugins.llamapet.vramDanger"), cfg.hardware.vramDangerThreshold);
                settings.set(QStringLiteral("plugins.llamapet.vramWarn"), cfg.hardware.vramWarnThreshold);
                settings.set(QStringLiteral("plugins.llamapet.defaultForm"), cfg.appearance.defaultMode);
                m_ctx.host->logger().info(
                    QString::fromLatin1(kTag),
                    QStringLiteral("Auto-migrated settings from LlamaPet config.toml: endpoint=%1")
                        .arg(cfg.inference.endpointUrl));
            }
        } else {
            cfg.inference.endpointUrl = endpoint;
            cfg.inference.apiKey = settings.get(QStringLiteral("plugins.llamapet.apiKey")).toString();
            cfg.inference.engine = settings.get(QStringLiteral("plugins.llamapet.engine"), cfg.inference.engine).toString();
            cfg.inference.pollIntervalMs = settings.get(QStringLiteral("plugins.llamapet.pollIntervalMs"), cfg.inference.pollIntervalMs).toInt();
            cfg.hardware.vramDangerThreshold = static_cast<float>(settings.get(QStringLiteral("plugins.llamapet.vramDanger"), cfg.hardware.vramDangerThreshold).toDouble());
            cfg.hardware.vramWarnThreshold = static_cast<float>(settings.get(QStringLiteral("plugins.llamapet.vramWarn"), cfg.hardware.vramWarnThreshold).toDouble());
            cfg.appearance.defaultMode = settings.get(QStringLiteral("plugins.llamapet.defaultForm"), cfg.appearance.defaultMode).toString();
        }
    }

    // 2. 采样与编排服务初始化
    m_service = std::make_unique<TelemetryService>(this);
    m_service->setConfig(cfg);

    // 硬件采集层初始化 (NVML / SMI)
    auto hal = createHal(cfg.hardware);
    if (hal) {
        m_service->setHalProvider(std::move(hal));
    }

    // 推理遥测层初始化 (默认使用带真实回环网络层的 LlamaCppIt)
    auto it = std::make_unique<LlamaCppIt>(cfg.inference.endpointUrl, cfg.inference.apiKey, this);
    m_service->setItProvider(std::move(it));

    // 订阅 TelemetryPayload 信号推送到 EventBus
    if (m_ctx.host) {
        connect(m_service.get(), &TelemetryService::telemetryPayload,
            this, [this](const QJsonObject& payload) {
                if (m_ctx.host) {
                    m_ctx.host->eventBus().publish(
                        QStringLiteral("margin.llamapet.telemetry"), payload);
                }
            });
    }

    m_service->start(cfg.inference.pollIntervalMs);

    // 3. 磁吸与 Peek 行为层初始化
    m_dockBehavior = std::make_unique<DockBehavior>();
    if (m_ctx.host) {
        m_dockBehavior->setSettings(&m_ctx.host->settings());
    }

    // 4. 悬浮窗系统初始化
    m_floatingWindows = std::make_unique<FloatingWindows>();
    auto* qml = m_ctx.host ? m_ctx.host->qml() : nullptr;
    auto* engine = qml ? qml->engine() : nullptr;
    if (engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("llamapet"), this);
        m_floatingWindows->initialize(engine, m_ctx.host, m_service.get(), m_dockBehavior.get());
    }

    // 5. 全局热键初始化
    m_hotkeyManager = std::make_unique<HotkeyManager>();
    bool hotkeyOk = m_hotkeyManager->registerHotkey(kHotkeyToggleId, cfg.shortcuts.toggleClickThrough);
    if (!hotkeyOk && m_ctx.host) {
        m_ctx.host->logger().warn(
            QString::fromLatin1(kTag),
            QStringLiteral("Failed to register hotkey: %1").arg(cfg.shortcuts.toggleClickThrough));
    }

    connect(m_hotkeyManager.get(), &HotkeyManager::hotkeyTriggered,
        this, [this](int id) {
            if (id == kHotkeyToggleId && m_floatingWindows) {
                m_floatingWindows->toggleVisibility();
            }
        });

    return Result<void, std::string>::ok();
}

void LlamaPetPlugin::onConfigChange(const QJsonObject& config) {
    EngineConfig cfg = EngineConfig::fromJson(config);
    applyConfig(cfg);
}

void LlamaPetPlugin::applyConfig(const EngineConfig& cfg) {
    if (m_service) {
        m_service->setConfig(cfg);
        auto it = std::make_unique<LlamaCppIt>(cfg.inference.endpointUrl, cfg.inference.apiKey, this);
        m_service->setItProvider(std::move(it));
    }
    if (m_hotkeyManager) {
        m_hotkeyManager->registerHotkey(kHotkeyToggleId, cfg.shortcuts.toggleClickThrough);
    }
    if (m_dockBehavior) {
        m_dockBehavior->setAutoDockHide(cfg.appearance.autoDockHide);
    }
    if (m_ctx.host) {
        m_ctx.host->tray().refreshPluginMenu(QStringLiteral("llamapet"));
    }
}

void LlamaPetPlugin::onUnload() {
    if (m_service) {
        m_service->stop();
    }
    if (m_hotkeyManager) {
        m_hotkeyManager->unregisterAll();
        m_hotkeyManager.reset();
    }
    if (m_floatingWindows) {
        m_floatingWindows->cleanup();
        m_floatingWindows.reset();
    }
    if (m_dockBehavior) {
        m_dockBehavior.reset();
    }
    if (m_service) {
        m_service.reset();
    }
    auto* qml = m_ctx.host ? m_ctx.host->qml() : nullptr;
    auto* engine = qml ? qml->engine() : nullptr;
    if (engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("llamapet"), nullptr);
        engine->rootContext()->setContextProperty(QStringLiteral("telemetryService"), nullptr);
        engine->rootContext()->setContextProperty(QStringLiteral("floatingWindows"), nullptr);
    }
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("LlamaPetPlugin onUnload"));
    }
}

DashboardTabContributor::TabInfo LlamaPetPlugin::tabInfo() const {
    DashboardTabContributor::TabInfo info;
    info.id = "llamapet";
    info.title = QCoreApplication::translate("LlamaPetPlugin", "LlamaPet").toStdString();
    info.icon = QUrl(QStringLiteral("qrc:/llamapet/assets/icons/llamapet.svg"));
    info.content_qml = QUrl(QStringLiteral("qrc:/llamapet/ui/HudTab.qml"));
    info.order = 50;
    return info;
}

SettingsPageContributor::PageInfo LlamaPetPlugin::pageInfo() const {
    SettingsPageContributor::PageInfo info;
    info.id = "llamapet";
    info.title = QCoreApplication::translate("LlamaPetPlugin", "LlamaPet").toStdString();
    info.icon = QUrl(QStringLiteral("qrc:/llamapet/assets/icons/llamapet.svg"));
    info.content_qml = QUrl(QStringLiteral("qrc:/llamapet/ui/SettingsPage.qml"));
    info.order = 50;
    return info;
}

QList<TrayMenuContributor::TrayItem> LlamaPetPlugin::contributeTrayItems() {
    QList<TrayMenuContributor::TrayItem> items;

    // 0. 打开详情操作
    TrayMenuContributor::TrayItem openDetailItem;
    openDetailItem.id = "open_dashboard_detail";
    openDetailItem.label = QCoreApplication::translate("LlamaPetPlugin", "Open Dashboard Details").toStdString();
    openDetailItem.checkable = false;
    items.append(openDetailItem);

    // 1. 开关组
    TrayMenuContributor::TrayItem alwaysOnTopItem;
    alwaysOnTopItem.id = "toggle_always_on_top";
    alwaysOnTopItem.label = QCoreApplication::translate("LlamaPetPlugin", "Always on Top").toStdString();
    alwaysOnTopItem.checkable = true;
    alwaysOnTopItem.checked = m_alwaysOnTop;
    items.append(alwaysOnTopItem);

    TrayMenuContributor::TrayItem clickThroughItem;
    clickThroughItem.id = "toggle_click_through";
    clickThroughItem.label = QCoreApplication::translate("LlamaPetPlugin", "Click-through").toStdString();
    clickThroughItem.checkable = true;
    clickThroughItem.checked = m_floatingWindows ? m_floatingWindows->isClickThrough() : false;
    items.append(clickThroughItem);

    TrayMenuContributor::TrayItem autoDockItem;
    autoDockItem.id = "toggle_auto_dock";
    autoDockItem.label = QCoreApplication::translate("LlamaPetPlugin", "Auto-dock Hide").toStdString();
    autoDockItem.checkable = true;
    autoDockItem.checked = m_dockBehavior ? m_dockBehavior->autoDockHide() : true;
    items.append(autoDockItem);

    // 2. 外观形态切换
    const bool isPet = m_floatingWindows && (m_floatingWindows->currentForm() == FloatingWindows::Form::MiniPet);

    TrayMenuContributor::TrayItem petFormItem;
    petFormItem.id = "form_mini_pet";
    petFormItem.label = QCoreApplication::translate("LlamaPetPlugin", "MiniPet (96x96)").toStdString();
    petFormItem.checkable = true;
    petFormItem.checked = isPet;
    items.append(petFormItem);

    TrayMenuContributor::TrayItem dockFormItem;
    dockFormItem.id = "form_dock";
    dockFormItem.label = QCoreApplication::translate("LlamaPetPlugin", "Dock (260x50)").toStdString();
    dockFormItem.checkable = true;
    dockFormItem.checked = !isPet;
    items.append(dockFormItem);

    // 3. 动作项
    TrayMenuContributor::TrayItem copyCmdItem;
    copyCmdItem.id = "copy_restart_cmd";
    copyCmdItem.label = QCoreApplication::translate("LlamaPetPlugin", "Copy Restart Command").toStdString();
    copyCmdItem.checkable = false;
    items.append(copyCmdItem);

    TrayMenuContributor::TrayItem clearKvItem;
    clearKvItem.id = "erase_idle_kv";
    clearKvItem.label = QCoreApplication::translate("LlamaPetPlugin", "Clear Idle KV Slots").toStdString();
    clearKvItem.checkable = false;
    clearKvItem.enabled = m_service ? !m_service->slotsDisabled() : true;
    items.append(clearKvItem);

    return items;
}

void LlamaPetPlugin::onTrayItemClicked(const std::string& id) {
    if (id == "open_dashboard_detail") {
        openDetail();
    } else if (id == "toggle_always_on_top") {
        setAlwaysOnTop(!m_alwaysOnTop);
    } else if (id == "toggle_click_through") {
        if (m_floatingWindows) {
            setClickThrough(!m_floatingWindows->isClickThrough());
        }
    } else if (id == "toggle_auto_dock") {
        if (m_dockBehavior) {
            setAutoDockHide(!m_dockBehavior->autoDockHide());
        }
    } else if (id == "form_mini_pet") {
        switchForm(QStringLiteral("mini_pet"));
    } else if (id == "form_dock") {
        switchForm(QStringLiteral("dock"));
    } else if (id == "copy_restart_cmd") {
        copyRestartCommand();
    } else if (id == "erase_idle_kv") {
        clearIdleKv();
    }

    if (m_ctx.host) {
        m_ctx.host->tray().refreshPluginMenu(QStringLiteral("llamapet"));
    }
}

void LlamaPetPlugin::openDetail() {
    auto* qml = m_ctx.host ? m_ctx.host->qml() : nullptr;
    auto* engine = qml ? qml->engine() : nullptr;
    if (engine) {
        QVariant dashboardVar = engine->rootContext()->contextProperty(QStringLiteral("dashboardRoot"));
        QObject* dashboard = dashboardVar.value<QObject*>();
        if (dashboard) {
            QMetaObject::invokeMethod(dashboard, "openDashboard",
                Qt::AutoConnection,
                Q_ARG(QVariant, QVariant::fromValue(QStringLiteral("llamapet"))));
        }
    }
}

void LlamaPetPlugin::openSettings() {
    auto* qml = m_ctx.host ? m_ctx.host->qml() : nullptr;
    auto* engine = qml ? qml->engine() : nullptr;
    if (engine) {
        QVariant settingsVar = engine->rootContext()->contextProperty(QStringLiteral("settingsRoot"));
        QObject* settings = settingsVar.value<QObject*>();
        if (settings) {
            QMetaObject::invokeMethod(settings, "openSettings",
                Qt::AutoConnection,
                Q_ARG(QVariant, QVariant::fromValue(QStringLiteral("llamapet"))));
        }
    }
}

void LlamaPetPlugin::copyRestartCommand() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(QString::fromUtf8(kRestartCmdTemplate));
    }
    if (m_ctx.host) {
        m_ctx.host->tray().showToast(
            QStringLiteral("LlamaPet"),
            QStringLiteral("Copied llama-server restart command to clipboard"));
    }
}

void LlamaPetPlugin::clearIdleKv() {
    if (!m_service) return;
    if (m_ctx.host) {
        m_ctx.host->logger().info(
            QString::fromLatin1(kTag),
            QStringLiteral("Triggering erase idle slots"));
    }
    m_service->eraseIdleSlots([this](Result<int, QString> res) {
        if (m_ctx.host) {
            if (res.isOk()) {
                m_ctx.host->tray().showToast(
                    QStringLiteral("LlamaPet"),
                    QStringLiteral("Successfully cleared %1 idle KV slots").arg(res.value()));
            } else {
                m_ctx.host->tray().showToast(
                    QStringLiteral("LlamaPet"),
                    QStringLiteral("Failed to clear idle slots: %1").arg(res.error()));
            }
        }
    });
}

void LlamaPetPlugin::switchForm(const QString& form) {
    if (m_floatingWindows) {
        if (form.toLower() == QStringLiteral("mini_pet") || form == QStringLiteral("0")) {
            m_floatingWindows->switchForm(static_cast<int>(FloatingWindows::Form::MiniPet));
        } else {
            m_floatingWindows->switchForm(static_cast<int>(FloatingWindows::Form::Dock));
        }
    }
    if (m_ctx.host) {
        m_ctx.host->settings().set(QStringLiteral("plugins.llamapet.defaultForm"), form);
        m_ctx.host->tray().refreshPluginMenu(QStringLiteral("llamapet"));
    }
}

void LlamaPetPlugin::setAlwaysOnTop(bool onTop) {
    m_alwaysOnTop = onTop;
    if (m_floatingWindows) {
        if (auto* w = m_floatingWindows->miniPetWindow()) {
            w->setFlag(Qt::WindowStaysOnTopHint, onTop);
        }
        if (auto* w = m_floatingWindows->dockWindow()) {
            w->setFlag(Qt::WindowStaysOnTopHint, onTop);
        }
    }
    if (m_ctx.host) {
        m_ctx.host->settings().set(QStringLiteral("plugins.llamapet.alwaysOnTop"), onTop);
        m_ctx.host->tray().refreshPluginMenu(QStringLiteral("llamapet"));
    }
}

void LlamaPetPlugin::setClickThrough(bool through) {
    if (m_floatingWindows) {
        m_floatingWindows->setClickThrough(through);
    }
    if (m_ctx.host) {
        m_ctx.host->settings().set(QStringLiteral("plugins.llamapet.clickThrough"), through);
        m_ctx.host->tray().refreshPluginMenu(QStringLiteral("llamapet"));
    }
}

void LlamaPetPlugin::setAutoDockHide(bool autoHide) {
    if (m_dockBehavior) {
        m_dockBehavior->setAutoDockHide(autoHide);
    }
    if (m_ctx.host) {
        m_ctx.host->settings().set(QStringLiteral("plugins.llamapet.autoDockHide"), autoHide);
        m_ctx.host->tray().refreshPluginMenu(QStringLiteral("llamapet"));
    }
}

} // namespace Margin::Plugins::LlamaPet
