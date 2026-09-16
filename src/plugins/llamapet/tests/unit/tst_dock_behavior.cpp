#include <QtTest>
#include "app/DockBehavior.h"

using namespace Margin::Plugins::LlamaPet;

class TstDockBehavior : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testEdgeSnapping();
    void testPeekTargetX();
    void testReanchorX();
    void testClampToScreens();
};

void TstDockBehavior::testEdgeSnapping() {
    const QRect screenGeo(0, 0, 1920, 1080);

    // 1. 靠近左侧 15px (<=20px) -> 吸附到 x=0
    {
        QRect winGeo(15, 200, 260, 50);
        SnapResult res = DockBehavior::calculateEdgeSnap(winGeo, screenGeo, 20);
        QVERIFY(res.snappedLeft);
        QVERIFY(!res.snappedRight);
        QCOMPARE(res.targetX, 0);
    }

    // 2. 距离左侧 25px (>20px) -> 不吸附
    {
        QRect winGeo(25, 200, 260, 50);
        SnapResult res = DockBehavior::calculateEdgeSnap(winGeo, screenGeo, 20);
        QVERIFY(!res.snappedLeft);
        QVERIFY(!res.snappedRight);
        QCOMPARE(res.targetX, 25);
    }

    // 3. 靠近右侧 10px (winRight = 1910, <=20px) -> 吸附到 x = 1920 - 260 = 1660
    {
        QRect winGeo(1650, 200, 260, 50); // win right = 1910
        SnapResult res = DockBehavior::calculateEdgeSnap(winGeo, screenGeo, 20);
        QVERIFY(!res.snappedLeft);
        QVERIFY(res.snappedRight);
        QCOMPARE(res.targetX, 1660);
    }
}

void TstDockBehavior::testPeekTargetX() {
    const QRect screenGeo(0, 0, 1920, 1080);
    const int winWidth = 260;
    const int peekExpose = 20;

    // 贴左侧：滑出后保留 20px，targetX = 0 - (260 - 20) = -240
    int leftPeekX = DockBehavior::calculatePeekTargetX(true, false, winWidth, screenGeo, peekExpose);
    QCOMPARE(leftPeekX, -240);

    // 贴右侧：滑出后保留 20px，targetX = 1920 - 20 = 1900
    int rightPeekX = DockBehavior::calculatePeekTargetX(false, true, winWidth, screenGeo, peekExpose);
    QCOMPARE(rightPeekX, 1900);
}

void TstDockBehavior::testReanchorX() {
    const QRect screenGeo(0, 0, 1920, 1080);

    // 贴右侧：MiniPet (96) 切换到 Dock (260) -> 必须继续贴右缘 x = 1920 - 260 = 1660
    int dockX = DockBehavior::calculateReanchorX(false, true, 1824, 96, 260, screenGeo);
    QCOMPARE(dockX, 1660);

    // 贴右侧：Dock (260) 切换回 MiniPet (96) -> 贴右缘 x = 1920 - 96 = 1824
    int miniX = DockBehavior::calculateReanchorX(false, true, 1660, 260, 96, screenGeo);
    QCOMPARE(miniX, 1824);

    // 贴左侧：始终贴 x = 0
    int leftDockX = DockBehavior::calculateReanchorX(true, false, 0, 96, 260, screenGeo);
    QCOMPARE(leftDockX, 0);
}

void TstDockBehavior::testClampToScreens() {
    const QRect primary(0, 0, 1920, 1080);
    const QRect secondary(1920, 0, 1920, 1080);
    const QList<QRect> screens = {primary, secondary};
    const QSize winSize(96, 96);

    // 1. 合法屏幕内坐标
    QPoint validPos(500, 300);
    QCOMPARE(DockBehavior::clampToScreens(validPos, winSize, screens, primary), validPos);

    // 2. 副屏合法坐标
    QPoint subPos(2500, 400);
    QCOMPARE(DockBehavior::clampToScreens(subPos, winSize, screens, primary), subPos);

    // 3. 拔副屏后坐标失效（例如落在 3000, 2000 超出所有现存屏幕）
    QPoint invalidPos(3500, 2500);
    QPoint fallback = DockBehavior::clampToScreens(invalidPos, winSize, {primary}, primary);
    // 回退至主屏右侧
    QCOMPARE(fallback.x(), 1920 - 96);
    QCOMPARE(fallback.y(), (1080 - 96) / 2);
}

QTEST_MAIN(TstDockBehavior)
#include "tst_dock_behavior.moc"
