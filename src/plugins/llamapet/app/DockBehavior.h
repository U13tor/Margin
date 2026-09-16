#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRect>
#include <QSize>
#include <QTimer>

class QQuickWindow;

namespace Margin {
class Settings;
}

namespace Margin::Plugins::LlamaPet {

struct SnapResult {
    bool snappedLeft{false};
    bool snappedRight{false};
    int targetX{0};
};

class DockBehavior : public QObject {
    Q_OBJECT

public:
    explicit DockBehavior(QObject* parent = nullptr);
    ~DockBehavior() override;

    // 纯函数计算接口（单测锚点）
    static SnapResult calculateEdgeSnap(const QRect& winGeo, const QRect& screenGeo, int snapThreshold = 20);
    static int calculatePeekTargetX(bool isSnappedLeft, bool isSnappedRight, int winWidth, const QRect& screenGeo, int peekExpose = 20);
    static int calculateReanchorX(bool isSnappedLeft, bool isSnappedRight, int currentX, int oldWidth, int newWidth, const QRect& screenGeo);
    static QPoint clampToScreens(const QPoint& pos, const QSize& winSize, const QList<QRect>& screenGeometries, const QRect& primaryScreenGeo);

    // 运行时行为接口
    void setSettings(Margin::Settings* settings) { m_settings = settings; }
    void setAutoDockHide(bool enable);
    bool autoDockHide() const { return m_autoDockHide; }

    bool isSnappedLeft() const { return m_isSnappedLeft; }
    bool isSnappedRight() const { return m_isSnappedRight; }
    bool isPeeking() const { return m_isPeeking; }
    bool isAnimating() const { return m_isAnimating; }

    void checkEdgeSnapping(QQuickWindow* win);
    void handleMouseEnter(QQuickWindow* win, bool isDockForm);
    void handleMouseLeave(QQuickWindow* win, bool isDockForm);
    void restoreFromPeek(QQuickWindow* win, bool animated = true);
    void onFormSwitched(QQuickWindow* win, int oldWidth, int newWidth);

    void loadPosition(QQuickWindow* win);
    void savePosition(QQuickWindow* win);

Q_SIGNALS:
    void snapStateChanged(bool snappedLeft, bool snappedRight);
    void peekStateChanged(bool isPeeking);

private:
    void animateToX(QQuickWindow* win, int targetX, int durationMs, std::function<void()> onFinished = nullptr);

    Margin::Settings* m_settings{nullptr};
    bool m_autoDockHide{true};

    bool m_isSnappedLeft{false};
    bool m_isSnappedRight{false};
    bool m_isPeeking{false};
    bool m_isAnimating{false};

    int m_normalSnappedX{0};
    QTimer m_peekTimer;
    QPointer<QPropertyAnimation> m_anim;
};

} // namespace Margin::Plugins::LlamaPet
