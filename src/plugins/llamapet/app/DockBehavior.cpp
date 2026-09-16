#include "DockBehavior.h"
#include "Margin/Settings.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <cmath>

namespace Margin::Plugins::LlamaPet {

namespace {
constexpr const char* kKeyDockPos = "plugins.llamapet.dockPos";
constexpr const char* kKeyAutoDockHide = "plugins.llamapet.autoDockHide";
constexpr int kDefaultPeekExpose = 20;
constexpr int kDefaultSnapThreshold = 20;
constexpr int kPeekDelayMs = 3000;
constexpr int kSlideInDurationMs = 180;
constexpr int kSlideOutDurationMs = 200;
} // namespace

DockBehavior::DockBehavior(QObject* parent) : QObject(parent) {
    m_peekTimer.setSingleShot(true);
    m_peekTimer.setInterval(kPeekDelayMs);
}

DockBehavior::~DockBehavior() {
    if (m_anim) {
        m_anim->stop();
    }
}

void DockBehavior::setAutoDockHide(bool enable) {
    m_autoDockHide = enable;
    if (m_settings) {
        m_settings->set(QString::fromLatin1(kKeyAutoDockHide), enable);
    }
    if (!m_autoDockHide && m_isPeeking) {
        m_peekTimer.stop();
    }
}

SnapResult DockBehavior::calculateEdgeSnap(const QRect& winGeo, const QRect& screenGeo, int snapThreshold) {
    SnapResult res;
    res.targetX = winGeo.x();

    const int distLeft = std::abs(winGeo.x() - screenGeo.x());
    const int distRight = std::abs((winGeo.x() + winGeo.width()) - (screenGeo.x() + screenGeo.width()));

    if (distLeft <= snapThreshold) {
        res.snappedLeft = true;
        res.targetX = screenGeo.x();
    } else if (distRight <= snapThreshold) {
        res.snappedRight = true;
        res.targetX = screenGeo.x() + screenGeo.width() - winGeo.width();
    }

    return res;
}

int DockBehavior::calculatePeekTargetX(bool isSnappedLeft, bool isSnappedRight, int winWidth, const QRect& screenGeo, int peekExpose) {
    if (isSnappedLeft) {
        return screenGeo.x() - (winWidth - peekExpose);
    } else if (isSnappedRight) {
        return screenGeo.x() + screenGeo.width() - peekExpose;
    }
    return screenGeo.x();
}

int DockBehavior::calculateReanchorX(bool isSnappedLeft, bool isSnappedRight, int currentX, int oldWidth, int newWidth, const QRect& screenGeo) {
    if (isSnappedLeft) {
        return screenGeo.x();
    } else if (isSnappedRight) {
        return screenGeo.x() + screenGeo.width() - newWidth;
    }
    // 未贴边时保持中心对齐或居中扩展
    int delta = (oldWidth - newWidth) / 2;
    int x = currentX + delta;
    if (x < screenGeo.x()) x = screenGeo.x();
    if (x + newWidth > screenGeo.x() + screenGeo.width()) {
        x = screenGeo.x() + screenGeo.width() - newWidth;
    }
    return x;
}

QPoint DockBehavior::clampToScreens(const QPoint& pos, const QSize& winSize, const QList<QRect>& screenGeometries, const QRect& primaryScreenGeo) {
    if (screenGeometries.isEmpty()) {
        return QPoint(primaryScreenGeo.x() + primaryScreenGeo.width() - winSize.width(),
                      primaryScreenGeo.y() + 100);
    }

    const QRect winRect(pos, winSize);

    // 检查是否与任意屏幕有相交且大部分在屏幕内
    bool valid = false;
    for (const auto& sGeo : screenGeometries) {
        if (sGeo.intersects(winRect)) {
            // 确保至少有 30px 可见
            QRect inter = sGeo.intersected(winRect);
            if (inter.width() >= 30 && inter.height() >= 30) {
                valid = true;
                break;
            }
        }
    }

    if (valid) {
        return pos;
    }

    // 越界回退：默认主屏右侧
    int defaultX = primaryScreenGeo.x() + primaryScreenGeo.width() - winSize.width();
    int defaultY = primaryScreenGeo.y() + (primaryScreenGeo.height() - winSize.height()) / 2;
    return QPoint(defaultX, defaultY);
}

void DockBehavior::checkEdgeSnapping(QQuickWindow* win) {
    if (!win) return;
    // 关键纪律：动画中或处于 Peek 状态时直接返回，严禁破坏状态或覆盖屏幕外坐标
    if (m_isAnimating || m_isPeeking) {
        return;
    }

    QScreen* screen = win->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return;

    const QRect sGeo = screen->availableGeometry();
    const QRect winGeo(win->position(), win->size());

    SnapResult snap = calculateEdgeSnap(winGeo, sGeo, kDefaultSnapThreshold);
    if (snap.snappedLeft || snap.snappedRight) {
        if (win->x() != snap.targetX) {
            win->setX(snap.targetX);
        }
        m_normalSnappedX = snap.targetX;
    }

    bool stateChanged = (m_isSnappedLeft != snap.snappedLeft || m_isSnappedRight != snap.snappedRight);
    m_isSnappedLeft = snap.snappedLeft;
    m_isSnappedRight = snap.snappedRight;

    if (stateChanged) {
        Q_EMIT snapStateChanged(m_isSnappedLeft, m_isSnappedRight);
    }

    // 仅在非 Peek 状态且非动画期间持久化
    savePosition(win);
}

void DockBehavior::handleMouseEnter(QQuickWindow* win, bool isDockForm) {
    m_peekTimer.stop();
    if (m_isPeeking) {
        if (isDockForm) {
            restoreFromPeek(win, true);
        } else {
            restoreFromPeek(win, false);
        }
    }
}

void DockBehavior::handleMouseLeave(QQuickWindow* win, bool isDockForm) {
    m_peekTimer.stop();
    if (!m_autoDockHide || !isDockForm || (!m_isSnappedLeft && !m_isSnappedRight) || m_isPeeking) {
        return;
    }

    m_peekTimer.disconnect();
    connect(&m_peekTimer, &QTimer::timeout, this, [this, win, isDockForm]() {
        if (!m_autoDockHide || !isDockForm || (!m_isSnappedLeft && !m_isSnappedRight) || m_isPeeking) {
            return;
        }
        if (!win) return;

        QScreen* screen = win->screen();
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (!screen) return;

        const QRect sGeo = screen->availableGeometry();
        m_normalSnappedX = win->x();

        int targetX = calculatePeekTargetX(m_isSnappedLeft, m_isSnappedRight, win->width(), sGeo, kDefaultPeekExpose);

        animateToX(win, targetX, kSlideOutDurationMs, [this]() {
            m_isPeeking = true;
            Q_EMIT peekStateChanged(true);
            // 注意：此处严禁调用 savePosition 写入屏幕外 targetX
        });
    });

    m_peekTimer.start(kPeekDelayMs);
}

void DockBehavior::restoreFromPeek(QQuickWindow* win, bool animated) {
    m_peekTimer.stop();
    if (!m_isPeeking || !win) return;

    int targetX = m_normalSnappedX;
    QScreen* screen = win->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect sGeo = screen->availableGeometry();
        if (m_isSnappedRight) {
            targetX = sGeo.x() + sGeo.width() - win->width();
        } else if (m_isSnappedLeft) {
            targetX = sGeo.x();
        }
    }

    if (animated) {
        animateToX(win, targetX, kSlideInDurationMs, [this, win, targetX]() {
            m_isPeeking = false;
            m_normalSnappedX = targetX;
            Q_EMIT peekStateChanged(false);
            savePosition(win);
        });
    } else {
        if (m_anim) m_anim->stop();
        m_isAnimating = false;
        win->setX(targetX);
        m_isPeeking = false;
        m_normalSnappedX = targetX;
        Q_EMIT peekStateChanged(false);
        savePosition(win);
    }
}

void DockBehavior::onFormSwitched(QQuickWindow* win, int oldWidth, int newWidth) {
    if (!win) return;
    if (m_isPeeking) {
        restoreFromPeek(win, false);
    }

    QScreen* screen = win->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    const QRect sGeo = screen->availableGeometry();
    int newX = calculateReanchorX(m_isSnappedLeft, m_isSnappedRight, win->x(), oldWidth, newWidth, sGeo);
    win->setX(newX);
    m_normalSnappedX = newX;

    savePosition(win);
}

void DockBehavior::loadPosition(QQuickWindow* win) {
    if (!win) return;

    bool hasStoredPos = false;
    QPoint targetPos;
    if (m_settings) {
        QVariant var = m_settings->get(QString::fromLatin1(kKeyDockPos));
        if (var.isValid() && var.canConvert<QJsonObject>()) {
            QJsonObject obj = var.toJsonObject();
            if (obj.contains(QStringLiteral("x")) && obj.contains(QStringLiteral("y"))) {
                targetPos.setX(obj[QStringLiteral("x")].toInt());
                targetPos.setY(obj[QStringLiteral("y")].toInt());
                hasStoredPos = true;
            }
        }
        m_autoDockHide = m_settings->get(QString::fromLatin1(kKeyAutoDockHide), true).toBool();
    }

    QList<QRect> screenGeos;
    const auto screens = QGuiApplication::screens();
    for (auto* s : screens) {
        screenGeos.append(s->availableGeometry());
    }

    QRect primaryGeo;
    if (auto* ps = QGuiApplication::primaryScreen()) {
        primaryGeo = ps->availableGeometry();
    } else if (!screenGeos.isEmpty()) {
        primaryGeo = screenGeos.first();
    }

    QPoint safePos;
    if (hasStoredPos) {
        safePos = clampToScreens(targetPos, win->size(), screenGeos, primaryGeo);
    } else {
        // S02 规范：首启无历史配置时默认贴主屏右缘垂直居中
        int defaultX = primaryGeo.x() + primaryGeo.width() - win->width();
        int defaultY = primaryGeo.y() + (primaryGeo.height() - win->height()) / 2;
        safePos = QPoint(defaultX, defaultY);
    }

    // 创建即定位：必须先 setPosition 后 show，杜绝居中或原点跳变
    win->setPosition(safePos);
    m_normalSnappedX = safePos.x();

    // 初始磁吸检查
    if (auto* curScreen = win->screen()) {
        SnapResult snap = calculateEdgeSnap(QRect(win->position(), win->size()), curScreen->availableGeometry());
        m_isSnappedLeft = snap.snappedLeft;
        m_isSnappedRight = snap.snappedRight;
    }
}

void DockBehavior::savePosition(QQuickWindow* win) {
    if (!win || m_isAnimating || m_isPeeking || !m_settings) {
        return;
    }
    QJsonObject obj;
    obj[QStringLiteral("x")] = win->x();
    obj[QStringLiteral("y")] = win->y();
    m_settings->set(QString::fromLatin1(kKeyDockPos), obj);
}

void DockBehavior::animateToX(QQuickWindow* win, int targetX, int durationMs, std::function<void()> onFinished) {
    if (!win) return;
    if (m_anim) {
        m_anim->stop();
    }

    m_isAnimating = true;
    m_anim = new QPropertyAnimation(win, "x", this);
    m_anim->setDuration(durationMs);
    m_anim->setStartValue(win->x());
    m_anim->setEndValue(targetX);
    m_anim->setEasingCurve(QEasingCurve::OutQuad);

    connect(m_anim.data(), &QPropertyAnimation::finished, this, [this, onFinished]() {
        m_isAnimating = false;
        if (onFinished) {
            onFinished();
        }
    });

    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace Margin::Plugins::LlamaPet
