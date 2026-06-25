// SystemTray impl — wraps QSystemTrayIcon + QMenu. Loaded from :/icons/*
// (compiled into host.qrc via AUTORCC). Menu structure (M4-C16):
//   1. Header: disabled "Margin v<X>" with app icon
//   2. Toggle group: every plugin's read_only=false TrayItems
//   3. Preview group: every plugin's read_only=true TrayItems
//   4. Open Dashboard + About
//   5. Quit
// Spec: docs/06 §4.8 + docs/05 §6.

#include "SystemTray.h"

#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QSystemTrayIcon>

namespace Margin {

SystemTray::SystemTray() {
    loadIcons();

    m_tray = std::make_unique<QSystemTrayIcon>(m_iconNormal);
    m_tray->setToolTip(QStringLiteral("Margin"));

    // Left-click / double-click the tray icon → open the dashboard
    // (docs/06 §3.2). Context-menu (right-click) is handled by setContextMenu.
    connect(m_tray.get(), &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    emit openDashboardRequested();
                }
            });

    buildMenu();
    m_tray->setContextMenu(m_menu.get());
}

SystemTray::~SystemTray() = default;

void SystemTray::loadIcons() {
    // PR5 (round-2 #6): pre-render SVGs to QPixmaps at load time instead of
    // passing the SVG path directly to QIcon. Reason: QSystemTrayIcon and
    // QMenu on Windows shell-render icons, and the native pipeline
    // monochromes stroke-only SVGs (e.g. tray.svg's stroke="#7C3AED" becomes
    // a generic gray). By rendering via QSvgRenderer into a 32x32 transparent
    // QPixmap once, we freeze the purple brand color into a raster that the
    // shell only blits — no more color loss. Same trick applies to the
    // auxiliary tray state icons (locked / stretching). The other menu
    // icons (open/settings/about/quit) are white-stroke primitives and
    // render fine either way; we keep them as QIcon(svg path) for symmetry
    // with the resource system.
    m_iconNormal     = renderSvgIcon(QStringLiteral(":/icons/tray.svg"));
    m_iconLocked     = renderSvgIcon(QStringLiteral(":/icons/tray-locked.svg"));
    m_iconStretching = renderSvgIcon(QStringLiteral(":/icons/tray-stretching.svg"));
    m_iconOpen       = QIcon(QStringLiteral(":/icons/icon-dashboard.svg"));
    m_iconSettings   = QIcon(QStringLiteral(":/icons/settings.svg"));
    m_iconAbout      = QIcon(QStringLiteral(":/icons/icon-info.svg"));
    m_iconQuit       = QIcon(QStringLiteral(":/icons/icon-quit.svg"));
}

QIcon SystemTray::renderSvgIcon(const QString& svgPath) {
    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid()) {
        // Fallback to QIcon(path) so a broken SVG doesn't break the tray —
        // we'll lose color fidelity but keep a visible icon.
        return QIcon(svgPath);
    }
    QPixmap pix(QSize(32, 32));
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter);
    return QIcon(pix);
}

void SystemTray::applyMenuStyle() {
    // M4-C16: QSS dark-theme pass. Pulls hex from docs/06 §2 (Theme.qml:
    // bgElevated #161618 / bgHover #1F1F23 / fgPrimary #E4E4E7 / fgMuted
    // #71717A / borderSubtle #27272A / borderStrong #3F3F46). Native QMenu
    // on Win11 defaults to a light Win11 context-menu look; this QSS brings
    // the surface in line with the rest of the app. Cannot reproduce:
    //   - centered header bar (QAction is single-line)
    //   - purple ✓ check indicator (needs custom icon paint)
    //   - "Toggle ON" sublabel (single text per item)
    // Those remain deferred — QSS is the cheap-but-effective layer.
    static const char* kQss = R"(
        QMenu {
            background-color: #161618;
            color: #E4E4E7;
            border: 1px solid #3F3F46;
            border-radius: 8px;
            padding: 6px 0;
        }
        QMenu::item {
            background: transparent;
            padding: 6px 28px 6px 28px;
            margin: 0 6px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #1F1F23;
            color: #E4E4E7;
        }
        QMenu::item:disabled {
            color: #71717A;
        }
        QMenu::item:selected:disabled {
            background-color: transparent;
        }
        QMenu::separator {
            height: 1px;
            background: #27272A;
            margin: 4px 10px;
        }
        QMenu::icon {
            padding-left: 12px;
        }
    )";
    if (m_menu) m_menu->setStyleSheet(QString::fromLatin1(kQss));
}

void SystemTray::buildMenu() {
    m_menu = std::make_unique<QMenu>();
    applyMenuStyle();
    rebuildMenu();
}

void SystemTray::rebuildMenu() {
    if (!m_menu) return;

    // Clear existing actions + reset the action map.
    m_menu->clear();
    m_actionMap.clear();

    // Helper: avoid back-to-back separators.
    auto addSepIfNeeded = [this]() {
        const auto actions = m_menu->actions();
        if (actions.isEmpty()) return;
        if (actions.last()->isSeparator()) return;
        m_menu->addSeparator();
    };

    // ── Section 1: Header ───────────────────────────────────────────
    // Disabled label "Margin v<x>" with the app icon. Acts as visual anchor,
    // not clickable. Native QMenu styling means we cannot reproduce the
    // prototype's centered header bar — content fidelity only (M4-C16).
    auto* header = m_menu->addAction(
        QStringLiteral("Margin v%1").arg(QLatin1String(MARGIN_VERSION)));
    header->setEnabled(false);
    header->setIcon(m_iconNormal);

    // ── Section 2: Plugin toggle group ──────────────────────────────
    // Iterate plugins in insertion order; pick read_only=false items so
    // toggles (Aura / Rhythm) cluster together.
    bool addedToggle = false;
    for (const auto& [pluginId, items] : m_pluginItems) {
        for (const auto& item : items) {
            if (item.read_only) continue;
            if (!addedToggle) {
                addSepIfNeeded();
                addedToggle = true;
            }
            QAction* act = m_menu->addAction(
                QString::fromStdString(item.label));
            act->setEnabled(item.enabled);
            act->setCheckable(item.checkable);
            act->setChecked(item.checked);
            m_actionMap.insert(act, PluginAction{pluginId,
                QString::fromStdString(item.id)});
            connect(act, &QAction::triggered, this, [this, act]() {
                const auto& pa = m_actionMap.value(act);
                emit pluginItemClicked(pa.pluginId, pa.itemId);
            });
        }
    }

    // ── Section 3: Plugin read-only preview group ───────────────────
    bool addedPreview = false;
    for (const auto& [pluginId, items] : m_pluginItems) {
        for (const auto& item : items) {
            if (!item.read_only) continue;
            if (!addedPreview) {
                addSepIfNeeded();
                addedPreview = true;
            }
            QAction* act = m_menu->addAction(
                QString::fromStdString(item.label));
            // Read-only info lines never trigger clicks — disabled makes
            // the intent visible (gray text) in native QMenu.
            act->setEnabled(false);
            (void)pluginId;  // not routed, no entry in m_actionMap
        }
    }

    // ── Section 4: Open Dashboard + Settings + About ───────────────
    addSepIfNeeded();
    QAction* openAction = m_menu->addAction(m_iconOpen,
        QCoreApplication::translate("SystemTray", "Open Dashboard..."));
    connect(openAction, &QAction::triggered, this, [this]() {
        emit openDashboardRequested();
    });

    // M5-C3: Settings window entry. Same signal-pattern as Open Dashboard.
    // Empty pageId → SettingsWindow keeps sidebar currentIndex 0 (General).
    QAction* settingsAction = m_menu->addAction(m_iconSettings,
        QCoreApplication::translate("SystemTray", "Settings..."));
    connect(settingsAction, &QAction::triggered, this, [this]() {
        emit openSettingsRequested({});
    });

    // About menu entry — no longer pops a native QDialog. Routes through the
    // same openSettingsRequested signal with pageId="about" so the user lands
    // on the Settings → About page (M5-C4). Native AboutDialog.{h,cpp} stays
    // for the standalone unit test test_about_dialog.cpp; production paths
    // no longer touch it.
    QAction* aboutAction = m_menu->addAction(m_iconAbout,
        QCoreApplication::translate("SystemTray", "About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        emit openSettingsRequested(QStringLiteral("about"));
    });

    // ── Section 5: Quit ─────────────────────────────────────────────
    addSepIfNeeded();
    QAction* quitAction = m_menu->addAction(m_iconQuit,
        QCoreApplication::translate("SystemTray", "Quit"));
    connect(quitAction, &QAction::triggered,
            qApp, &QCoreApplication::quit);
}

void SystemTray::retranslate() {
    // Re-pull plugin items so labels built with translate() in plugin code
    // (e.g. "Rhythm: ON") pick up the newly installed catalog. Without this,
    // plugin tray items stay in the language they were first loaded under.
    if (m_lookup) {
        QStringList pluginIds;
        pluginIds.reserve(m_pluginItems.size());
        for (const auto& pair : m_pluginItems) pluginIds << pair.first;
        m_pluginItems.clear();
        for (const QString& pid : pluginIds) {
            if (auto* c = m_lookup(pid)) {
                m_pluginItems.append({pid, c->contributeTrayItems()});
            }
        }
    }
    // rebuildMenu re-evaluates the host QCoreApplication::translate() calls
    // in this file (Open Dashboard... / Settings... / About / Quit), so a
    // single call covers both host actions and refreshed plugin labels.
    rebuildMenu();
}

void SystemTray::addPluginItems(const QString& pluginId,
                                const QList<TrayMenuContributor::TrayItem>& items) {
    // Replace if plugin already present (defensive — PluginManager should
    // removePluginItems before re-adding, but be tolerant).
    for (auto& [pid, list] : m_pluginItems) {
        if (pid == pluginId) {
            list = items;
            rebuildMenu();
            return;
        }
    }
    m_pluginItems.append({pluginId, items});
    rebuildMenu();
}

void SystemTray::removePluginItems(const QString& pluginId) {
    for (int i = 0; i < m_pluginItems.size(); ++i) {
        if (m_pluginItems[i].first == pluginId) {
            m_pluginItems.removeAt(i);
            rebuildMenu();
            return;
        }
    }
}

void SystemTray::setContributorLookup(TrayContributorLookup lookup) {
    m_lookup = std::move(lookup);
}

void SystemTray::refreshPluginMenu(const QString& pluginId) {
    if (!m_lookup) return;
    TrayMenuContributor* c = m_lookup(pluginId);
    if (!c) return;
    // addPluginItems already handles in-place replacement + rebuild.
    addPluginItems(pluginId, c->contributeTrayItems());
}

void SystemTray::showToast(const QString& title,
                           const QString& message,
                           int timeoutMs) {
    if (m_tray) {
        m_tray->showMessage(title, message,
                            QSystemTrayIcon::Information, timeoutMs);
    }
}

void SystemTray::setIconState(IconState state) {
    if (!m_tray) return;
    switch (state) {
        case IconState::Normal:     m_tray->setIcon(m_iconNormal);     break;
        case IconState::Locked:     m_tray->setIcon(m_iconLocked);     break;
        case IconState::Stretching: m_tray->setIcon(m_iconStretching); break;
    }
}

void SystemTray::setTooltip(const QString& text) {
    if (m_tray) m_tray->setToolTip(text);
}

void SystemTray::show() { if (m_tray) m_tray->show(); }
void SystemTray::hide() { if (m_tray) m_tray->hide(); }

} // namespace Margin
