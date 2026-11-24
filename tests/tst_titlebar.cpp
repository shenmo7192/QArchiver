// QArchiver / 小Q解压
// Regression tests for the immersive title bar and the runtime layout switch.
// 沉浸式标题栏与运行时布局切换的回归测试。
//
// Covered regressions:
//  1. switching the UI layout detached the immersive title bar because
//     teardownUi() called QMainWindow::menuBar(), which lazily creates a
//     QMenuBar and replaces the custom menu widget;
//  2. the QMenuBar stretched across the whole title bar and swallowed
//     left-button presses in the empty area, so the window could not be
//     dragged there;
//  3. the title bar colours were hardcoded in the style sheets instead of
//     using theme tokens, so they ignored the light/dark theme.
// 覆盖的回归问题：
//  1. 切换界面布局会丢标题栏：teardownUi() 调用了 QMainWindow::menuBar()，
//     后者懒创建 QMenuBar 并替换自定义菜单控件；
//  2. QMenuBar 拉伸占满标题栏，吞掉空白处的左键按下事件，导致无法拖窗；
//  3. 标题栏颜色在样式表中写死，未使用主题 token，深浅主题下都不跟随。

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenuBar>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QVBoxLayout>
#include <QWidget>

#include "core/AppSettings.h"
#include "ui/MainWindow.h"
#include "ui/widgets/ImmersiveTitleBar.h"

namespace {

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

// Extract the body of a single `selector { ... }` rule.
// 提取单个 `selector { ... }` 规则的内容。
QString ruleBlock(const QString &qss, const QString &selector)
{
    const int start = qss.indexOf(selector);
    if (start < 0) {
        return {};
    }
    const int open = qss.indexOf(QLatin1Char('{'), start);
    const int close = open < 0 ? -1 : qss.indexOf(QLatin1Char('}'), open);
    if (open < 0 || close < 0) {
        return {};
    }
    return qss.mid(open + 1, close - open - 1);
}

// Find the deepest visible child at a global position without relying on the
// platform's windowAt(), which the offscreen plugin does not implement.
// 不依赖平台 windowAt()（离屏插件未实现）找到全局位置上最深的可见子控件。
QWidget *widgetUnder(QWidget *root, const QPoint &globalPos)
{
    QWidget *widget = root;
    while (QWidget *child = widget->childAt(widget->mapFromGlobal(globalPos))) {
        if (!child->isVisible()) {
            break;
        }
        widget = child;
    }
    return widget;
}

// A plain window with an immersive title bar at the top and two menus.
// 一个顶部带沉浸式标题栏、含两个菜单的普通窗口。
struct TitleBarWindow {
    QWidget window;
    ImmersiveTitleBar *bar = nullptr;

    TitleBarWindow()
    {
        window.resize(800, 300);
        window.move(120, 80);

        auto *layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        bar = new ImmersiveTitleBar(&window);
        layout->addWidget(bar);

        auto *body = new QWidget(&window);
        body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(body, 1);

        bar->menuBarWidget()->addMenu(QStringLiteral("File"));
        bar->menuBarWidget()->addMenu(QStringLiteral("Edit"));

        window.show();
        QTest::qWait(50);
    }

    // A point in the empty area right of the menu items.
    // 菜单项右侧空白区域中的一点。
    QPoint emptyPoint() const
    {
        QMenuBar *menuBar = bar->menuBarWidget();
        return QPoint(menuBar->geometry().right() + 60, bar->height() / 2);
    }
};

} // namespace

class TestTitleBar : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void cleanupTestCase();

    void menuBarHugsItemsAndLeavesDragArea();
    void emptyMenuBarAreaDragsWindow();
    void emptyMenuBarAreaDoubleClickMaximizes();
    void titleBarSurvivesLayoutSwitch();
    void titleBarColorsUseThemeTokens();

private:
    QString m_settingsFile;
    QString m_savedLayout;
};

// Isolate the settings environment from the developer's real configuration.
// 隔离设置环境，避免影响开发者真实配置。
void TestTitleBar::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("QArchiverTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_titlebar"));
    m_settingsFile = QSettings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("QArchiverTests"),
                               QStringLiteral("tst_titlebar"))
                         .fileName();
    QFile::remove(m_settingsFile);
    m_savedLayout = AppSettings::instance().uiLayoutMode();
}

void TestTitleBar::cleanup()
{
    AppSettings::instance().setUiLayoutMode(m_savedLayout);
}

void TestTitleBar::cleanupTestCase()
{
    QFile::remove(m_settingsFile);
    QDir().rmdir(QFileInfo(m_settingsFile).absolutePath());
}

// The menu bar keeps its natural width; the remaining space belongs to the
// title bar so presses there reach the drag implementation.
// 菜单栏保持自然宽度，剩余空间归标题栏，空白处的按下事件才能进入拖拽逻辑。
void TestTitleBar::menuBarHugsItemsAndLeavesDragArea()
{
    TitleBarWindow fixture;
    QMenuBar *menuBar = fixture.bar->menuBarWidget();

    QCOMPARE(menuBar->sizePolicy().horizontalPolicy(), QSizePolicy::Maximum);
    QVERIFY2(menuBar->width() < fixture.bar->width() / 2,
             "the menu bar must not stretch across the remaining title bar width");

    const QPoint empty = fixture.emptyPoint();
    QVERIFY2(empty.x() < fixture.bar->width() - 160,
             "the test point must stay left of the window buttons");
    QVERIFY2(empty.x() > menuBar->geometry().right(),
             "the test point must be outside the menu bar");
    QCOMPARE(fixture.bar->childAt(empty), nullptr);
}

// Pressing the empty area and moving the mouse drags the window through the
// manual fallback used when the platform does not start a system move.
// 在空白处按下并移动鼠标时，通过手动回退路径拖动窗口（平台未启动系统移动）。
void TestTitleBar::emptyMenuBarAreaDragsWindow()
{
    if (QGuiApplication::platformName() != QLatin1String("offscreen")) {
        QSKIP("the manual drag fallback is deterministic only offscreen");
    }

    TitleBarWindow fixture;
    const QPoint local = fixture.emptyPoint();
    const QPoint global = fixture.bar->mapToGlobal(local);
    const QPoint startPos = fixture.window.pos();

    // Deliver the press to the widget that would receive it in the real event
    // path.  With a stretching QMenuBar that is the menu bar, which keeps the
    // event accepted and prevents the title bar from ever starting the drag.
    // 把按下事件投递给真实事件路径中会收到它的控件。若菜单栏被拉伸，那就是
    // 菜单栏本身，它会保留 accepted 状态，标题栏根本收不到，也就无法拖窗。
    QWidget *target = widgetUnder(&fixture.window, global);
    QVERIFY(target);
    QVERIFY2(target == fixture.bar,
             "the empty title bar area must be routed to the title bar, not to "
             "the menu bar");
    const QPoint targetLocal = target->mapFromGlobal(global);

    QMouseEvent press(QEvent::MouseButtonPress, targetLocal, global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(QApplication::sendEvent(target, &press));
    QVERIFY(press.isAccepted());

    const QPoint delta(70, 45);
    QMouseEvent move(QEvent::MouseMove, targetLocal + delta, global + delta,
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(QApplication::sendEvent(target, &move));
    QVERIFY(move.isAccepted());
    QCOMPARE(fixture.window.pos(), startPos + delta);

    QMouseEvent release(QEvent::MouseButtonRelease, targetLocal + delta,
                        global + delta, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(target, &release);
}

// Double-clicking the empty area toggles the maximized state.
// 双击空白区域切换最大化状态。
void TestTitleBar::emptyMenuBarAreaDoubleClickMaximizes()
{
    TitleBarWindow fixture;
    QSignalSpy spy(fixture.bar, &ImmersiveTitleBar::maximizeRequested);

    const QPoint local = fixture.emptyPoint();
    const QPoint global = fixture.bar->mapToGlobal(local);

    // Same real-event-path delivery as the drag test: the press must reach the
    // title bar instead of being swallowed by the menu bar.
    // 与拖拽测试使用同样的真实事件路径：按下必须到达标题栏，而不是被菜单栏吞掉。
    QWidget *target = widgetUnder(&fixture.window, global);
    QVERIFY(target);
    QVERIFY2(target == fixture.bar,
             "the empty title bar area must be routed to the title bar, not to "
             "the menu bar");
    QMouseEvent doubleClick(QEvent::MouseButtonDblClick,
                            target->mapFromGlobal(global), global,
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(QApplication::sendEvent(target, &doubleClick));
    QVERIFY(doubleClick.isAccepted());
    QCOMPARE(spy.count(), 1);
}

// Switching the layout rebuilds the chrome but must keep the immersive title
// bar attached to the menu-widget slot, including after a stray QMenuBar was
// created by QMainWindow::menuBar().
// 切换布局会重建界面外观，但必须让标题栏保持在菜单控件槽位；即使曾经误调
// QMainWindow::menuBar() 创建了 QMenuBar，也要能恢复。
void TestTitleBar::titleBarSurvivesLayoutSwitch()
{
    const QString original = AppSettings::instance().uiLayoutMode();
    const QString other = original == QLatin1String("modern")
        ? QStringLiteral("classic")
        : QStringLiteral("modern");

    MainWindow window;
    QVERIFY(window.m_titleBar);
    QCOMPARE(window.menuWidget(),
             static_cast<QWidget *>(window.m_titleBar));
    QVERIFY(!window.m_titleBar->menuBarWidget()->actions().isEmpty());

    // Both directions: the rebuild used to detach the title bar every time.
    // 两个方向都验证：以前每次重建都会把标题栏顶掉。
    for (const QString &mode : {other, original}) {
        AppSettings::instance().setUiLayoutMode(mode);
        window.teardownUi();

        // teardownUi() itself must not touch QMainWindow::menuBar(); doing so
        // detaches the title bar before buildUi() can re-add it.
        // teardownUi() 本身绝不能调用 QMainWindow::menuBar()，否则标题栏会在
        // buildUi() 重新挂回之前就被顶掉。
        QCOMPARE(window.menuWidget(),
                 static_cast<QWidget *>(window.m_titleBar));

        window.buildUi();

        QCOMPARE(window.menuWidget(),
                 static_cast<QWidget *>(window.m_titleBar));
        QVERIFY2(!window.m_titleBar->isHidden(), qPrintable(mode));
        QVERIFY2(!window.m_titleBar->menuBarWidget()->actions().isEmpty(),
                 qPrintable(mode));
    }

    // The defensive re-attach in buildUi() must recover from an accidental
    // QMainWindow::menuBar() call, which takes the menu-widget slot.
    // buildUi() 中的防御性重挂必须能从误调的 menuBar() 中恢复。
    AppSettings::instance().setUiLayoutMode(other);
    window.teardownUi();
    window.menuBar();
    QVERIFY(window.menuWidget() != static_cast<QWidget *>(window.m_titleBar));

    window.buildUi();
    QCOMPARE(window.menuWidget(), static_cast<QWidget *>(window.m_titleBar));
    QVERIFY(!window.m_titleBar->isHidden());
    QVERIFY(!window.m_titleBar->menuBarWidget()->actions().isEmpty());
}

// The title bar rule must be token-driven, and every theme variant must define
// the tokens; otherwise a theme switch leaves the colours unresolved.
// 标题栏规则必须使用 token，且每个主题变体都要定义这些 token，否则切换主题
// 时颜色无法解析。
void TestTitleBar::titleBarColorsUseThemeTokens()
{
    const QString root = QString::fromLatin1(QARCHIVER_SOURCE_DIR);
    const QStringList sheets = {
        QStringLiteral("resources/themes/modern/modern.qss"),
        QStringLiteral("resources/themes/aero/aero.qss")};

    const QRegularExpression hardcoded(QStringLiteral("#[0-9a-fA-F]{6}"));
    for (const QString &relative : sheets) {
        const QString qss = readText(root + QLatin1Char('/') + relative);
        QVERIFY2(!qss.isEmpty(), qPrintable(relative));

        const QString block = ruleBlock(qss, QStringLiteral("QWidget#ImmersiveTitleBar"));
        QVERIFY2(!block.isEmpty(), qPrintable(relative));
        for (const char *token : {"@titleBarTop@", "@titleBarMid@",
                                  "@titleBarBottom@", "@border@"}) {
            QVERIFY2(block.contains(QLatin1String(token)),
                     qPrintable(QStringLiteral("%1: %2 missing in the "
                                               "ImmersiveTitleBar rule")
                                    .arg(relative, QLatin1String(token))));
        }
        QVERIFY2(!hardcoded.match(block).hasMatch(),
                 qPrintable(QStringLiteral("%1: hardcoded colour in the "
                                           "ImmersiveTitleBar rule")
                                .arg(relative)));
        QVERIFY2(qss.contains(QStringLiteral("@titleBarText@")),
                 qPrintable(QStringLiteral("%1: title/menu/button text must use "
                                           "@titleBarText@")
                                .arg(relative)));
    }

    // Aero light/dark and modern light/dark: four definitions per token.
    // aero 浅色/深色与 modern 浅色/深色：每个 token 四处定义。
    const QString themeManager =
        readText(root + QStringLiteral("/src/ui/theme/ThemeManager.cpp"));
    QVERIFY(!themeManager.isEmpty());
    for (const char *token : {"titleBarTop", "titleBarMid",
                              "titleBarBottom", "titleBarText"}) {
        const QString needle = QLatin1Char('"') + QLatin1String(token)
            + QLatin1Char('"');
        QCOMPARE(themeManager.count(needle), 4);
    }
}

QTEST_MAIN(TestTitleBar)

#include "tst_titlebar.moc"
