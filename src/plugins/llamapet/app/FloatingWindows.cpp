#include "FloatingWindows.h"
#include "ClickThrough.h"
#include "DockBehavior.h"
#include "TelemetryService.h"
#include "Margin/HostServices.h"
#include "Margin/Logger.h"
#include "Margin/TrayService.h"

#include <QEvent>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

namespace Margin::Plugins::LlamaPet {

namespace {
constexpr const char* kTag = "llamapet";
constexpr const char* kMiniPetQml = "qrc:/llamapet/ui/MiniPetWindow.qml";
constexpr const char* kDockQml = "qrc:/llamapet/ui/DockWindow.qml";
const QSize kMiniPetSize(96, 96);
const QSize kDockSize(260, 50);
} // namespace

FloatingWindows::FloatingWindows(QObject* parent) : QObject(parent) {}

FloatingWindows::~FloatingWindows() {
    cleanup();
}

QString FloatingWindows::skinJson() const {
    QFile file(QStringLiteral(":/llamapet/assets/skins/pixel_llama/skin.json"));
    if (file.open(QIODevice::ReadOnly)) {
        return QString::fromUtf8(file.readAll());
    }
    return QStringLiteral("{}");
}

bool FloatingWindows::initialize(QQmlEngine* engine, Margin::HostServices* host,
                                 TelemetryService* telemetry, DockBehavior* dockBehavior) {
    if (!engine) return false;
    m_host = host;
    m_telemetry = telemetry;
    m_dockBehavior = dockBehavior;

    // 暴露 context property 给 QML 纯展示绑定
    engine->rootContext()->setContextProperty(QStringLiteral("telemetryService"), m_telemetry);
    engine->rootContext()->setContextProperty(QStringLiteral("floatingWindows"), this);

    // 1. 创建 MiniPetWindow
    m_miniPetWindow = createWindowFromQml(engine, QString::fromLatin1(kMiniPetQml), kMiniPetSize);
    if (!m_miniPetWindow) {
        if (m_host) {
            m_host->logger().warn(QString::fromLatin1(kTag), QStringLiteral("MiniPetWindow creation failed"));
        }
        return false;
    }

    // 2. 创建 DockWindow
    m_dockWindow = createWindowFromQml(engine, QString::fromLatin1(kDockQml), kDockSize);
    if (!m_dockWindow) {
        if (m_host) {
            m_host->logger().warn(QString::fromLatin1(kTag), QStringLiteral("DockWindow creation failed"));
        }
        return false;
    }

    // 3. 不变量 1 & 5 & 8：创建即定位后再 show（先 loadPosition / setPosition 再 show，杜绝中间跳变）
    if (m_dockBehavior) {
        m_dockBehavior->loadPosition(m_miniPetWindow);
        // 同步位置至 Dock
        m_dockWindow->setPosition(m_miniPetWindow->position());
    }

    // 安装事件过滤器以捕获 moveEvent
    m_miniPetWindow->installEventFilter(this);
    m_dockWindow->installEventFilter(this);

    // 默认展示 MiniPet 形态
    m_currentForm = Form::MiniPet;
    showCurrent();

    if (m_host) {
        m_host->logger().info(QString::fromLatin1(kTag), QStringLiteral("FloatingWindows initialized successfully"));
    }

    return true;
}

QQuickWindow* FloatingWindows::createWindowFromQml(QQmlEngine* engine, const QString& qmlUrl, const QSize& size) {
    QQmlComponent comp(engine, QUrl(qmlUrl));
    QObject* obj = comp.create();
    if (comp.isError()) {
        for (const auto& err : comp.errors()) {
            if (m_host) {
                m_host->logger().warn(QString::fromLatin1(kTag),
                    QStringLiteral("QML Error in %1: %2").arg(qmlUrl, err.toString()));
            }
        }
        delete obj;
        return nullptr;
    }

    auto* win = qobject_cast<QQuickWindow*>(obj);
    if (!win) {
        delete obj;
        return nullptr;
    }

    // 不变量：无边框 + 置顶 + Qt::Tool（不在任务栏显示图标）
    win->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    win->resize(size);
    win->setMinimumSize(size);

    return win;
}

void FloatingWindows::cleanup() {
    if (m_miniPetWindow) {
        m_miniPetWindow->removeEventFilter(this);
        m_miniPetWindow->hide();
        m_miniPetWindow->deleteLater();
        m_miniPetWindow = nullptr;
    }
    if (m_dockWindow) {
        m_dockWindow->removeEventFilter(this);
        m_dockWindow->hide();
        m_dockWindow->deleteLater();
        m_dockWindow = nullptr;
    }
}

QQuickWindow* FloatingWindows::currentWindow() const {
    return (m_currentForm == Form::MiniPet) ? m_miniPetWindow.data() : m_dockWindow.data();
}

void FloatingWindows::showCurrent() {
    if (!m_userVisible) return;
    auto* cur = currentWindow();
    if (cur) {
        cur->show();
        cur->requestActivate();
    }
}

void FloatingWindows::hideCurrent() {
    if (m_miniPetWindow) m_miniPetWindow->hide();
    if (m_dockWindow) m_dockWindow->hide();
}

void FloatingWindows::toggleVisibility() {
    m_userVisible = !m_userVisible;
    if (m_userVisible) {
        showCurrent();
    } else {
        hideCurrent();
    }
    Q_EMIT visibilityChanged(m_userVisible);
}

void FloatingWindows::switchForm(int formIndex) {
    Form target = (formIndex == 1) ? Form::Dock : Form::MiniPet;
    if (m_currentForm == target) return;

    auto* oldWin = currentWindow();
    int oldWidth = oldWin ? oldWin->width() : (m_currentForm == Form::MiniPet ? kMiniPetSize.width() : kDockSize.width());
    int oldX = oldWin ? oldWin->x() : 0;
    int oldY = oldWin ? oldWin->y() : 0;

    if (oldWin) {
        oldWin->hide();
    }

    m_currentForm = target;
    auto* newWin = currentWindow();
    if (newWin) {
        int newWidth = (m_currentForm == Form::MiniPet) ? kMiniPetSize.width() : kDockSize.width();
        newWin->setPosition(QPoint(oldX, oldY));

        // 不变量 3：形态切换重锚定
        if (m_dockBehavior) {
            m_dockBehavior->onFormSwitched(newWin, oldWidth, newWidth);
        }

        // 维持穿透状态
        if (m_clickThrough) {
            ClickThrough::setClickThrough(newWin, true);
        }

        // 命令式显示新窗口
        if (m_userVisible) {
            newWin->show();
            newWin->requestActivate();
        }
    }

    Q_EMIT formChanged(m_currentForm);
}

void FloatingWindows::setClickThrough(bool enabled) {
    m_clickThrough = enabled;
    if (m_miniPetWindow) {
        ClickThrough::setClickThrough(m_miniPetWindow, enabled);
    }
    if (m_dockWindow) {
        ClickThrough::setClickThrough(m_dockWindow, enabled);
    }
    Q_EMIT clickThroughChanged(m_clickThrough);
}

void FloatingWindows::startSystemMove(QQuickWindow* win) {
    // 穿透状态下不响应拖拽
    if (m_clickThrough || !win) return;
    win->startSystemMove();
}

void FloatingWindows::handleMouseEnter(QQuickWindow* win) {
    if (m_dockBehavior) {
        m_dockBehavior->handleMouseEnter(win, m_currentForm == Form::Dock);
    }
}

void FloatingWindows::handleMouseLeave(QQuickWindow* win) {
    if (m_dockBehavior) {
        m_dockBehavior->handleMouseLeave(win, m_currentForm == Form::Dock);
    }
}

void FloatingWindows::showContextMenu(QQuickWindow* /*win*/) {
    if (m_host) {
        m_host->tray().refreshPluginMenu(QString::fromLatin1(kTag));
    }
}

bool FloatingWindows::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Move) {
        auto* win = qobject_cast<QQuickWindow*>(watched);
        if (win && win == currentWindow() && m_dockBehavior) {
            // 不变量 4：程序性移动期间（动画或 peek）抑制 moveEvent 重入
            if (!m_dockBehavior->isAnimating() && !m_dockBehavior->isPeeking()) {
                m_dockBehavior->checkEdgeSnapping(win);
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace Margin::Plugins::LlamaPet
