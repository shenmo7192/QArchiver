// QArchiver / 小Q解压
// Regression tests for the frameless window edge/corner resize hit zones.
// 无边框窗口边缘/角落缩放命中区的回归测试。

#include <QApplication>
#include <QSet>
#include <QWidget>
#include <QtTest>

#include "ui/widgets/WindowResizeBorders.h"

class TestWindowResize : public QObject {
    Q_OBJECT

private slots:
    void createsEightRaisedHitZones();
    void raisesAboveNewChildren();
    void disablesZonesWhenMaximized();
};

namespace {

QList<QWidget *> resizeBorders(QWidget &window)
{
    return window.findChildren<QWidget *>(
        QStringLiteral("WindowResizeBorder"));
}

bool isResizeBorder(QWidget *widget)
{
    return widget
           && widget->objectName() == QStringLiteral("WindowResizeBorder");
}

} // namespace

void TestWindowResize::createsEightRaisedHitZones()
{
    QWidget window;
    window.resize(400, 300);
    WindowResizeBorders helper(&window);
    window.show();
    QTest::qWait(20);

    const QList<QWidget *> zones = resizeBorders(window);
    QCOMPARE(zones.size(), 8);
    for (QWidget *zone : zones) {
        QVERIFY(zone->isVisible());
        QCOMPARE(zone->cursor().shape() == Qt::ArrowCursor, false);
    }

    // The outer strip resizes; the content inside it still receives clicks.
    QVERIFY(isResizeBorder(window.childAt(QPoint(2, 150))));
    QVERIFY(isResizeBorder(window.childAt(QPoint(397, 150))));
    QVERIFY(isResizeBorder(window.childAt(QPoint(200, 2))));
    QVERIFY(isResizeBorder(window.childAt(QPoint(200, 297))));
    QVERIFY(isResizeBorder(window.childAt(QPoint(2, 2))));
    QVERIFY(isResizeBorder(window.childAt(QPoint(397, 2))));
    QVERIFY(isResizeBorder(window.childAt(QPoint(2, 297))));
    QVERIFY(isResizeBorder(window.childAt(QPoint(397, 297))));
    QVERIFY(!isResizeBorder(window.childAt(QPoint(200, 150))));

    QSet<int> shapes;
    for (QWidget *zone : zones) {
        shapes.insert(static_cast<int>(zone->cursor().shape()));
    }
    QCOMPARE(shapes.size(), 4);
}

void TestWindowResize::raisesAboveNewChildren()
{
    QWidget window;
    window.resize(400, 300);
    WindowResizeBorders helper(&window);
    window.show();
    QTest::qWait(20);

    auto *cover = new QWidget(&window);
    cover->setGeometry(0, 0, 100, 100);
    cover->show();
    cover->raise();
    QCOMPARE(window.childAt(QPoint(2, 50)), cover);

    helper.raiseBorders();
    QVERIFY(isResizeBorder(window.childAt(QPoint(2, 50))));
}

void TestWindowResize::disablesZonesWhenMaximized()
{
    QWidget window;
    window.resize(400, 300);
    WindowResizeBorders helper(&window);
    window.show();
    QTest::qWait(20);

    window.setWindowState(Qt::WindowMaximized);
    QCoreApplication::processEvents();
    QVERIFY(window.isMaximized());
    for (QWidget *zone : resizeBorders(window)) {
        QVERIFY(zone->isHidden());
    }

    window.setWindowState(Qt::WindowNoState);
    QCoreApplication::processEvents();
    QVERIFY(!window.isMaximized());
    for (QWidget *zone : resizeBorders(window)) {
        QVERIFY(!zone->isHidden());
    }
}

QTEST_MAIN(TestWindowResize)
#include "tst_windowresize.moc"
