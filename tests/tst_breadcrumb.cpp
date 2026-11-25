// QArchiver / 小Q解压
// Regression tests for the Windows 7 style breadcrumb location bar.
// Windows 7 风格面包屑路径框的回归测试。

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QStackedLayout>
#include <QToolButton>
#include <QtTest>

#include "ui/widgets/BreadcrumbPathBar.h"

class TestBreadcrumb : public QObject {
    Q_OBJECT

private slots:
    void archiveSegmentsFollowInternalDirectory();
    void localSegmentsStartAtFilesystemRoot();
    void wideBarShowsEveryLevel();
    void narrowBarHidesMiddleLevelsBehindOverflow();
    void firstLevelElidesBeforeLast();
    void childWidgetClickOpensSelectableText();
    void overflowMenuListsHiddenLevels();
};

namespace {

QList<QToolButton *> levelButtons(BreadcrumbPathBar &bar)
{
    QList<QToolButton *> buttons;
    const auto all = bar.findChildren<QToolButton *>(
        QStringLiteral("BreadcrumbButton"));
    buttons = all;
    return buttons;
}

QToolButton *overflowButton(BreadcrumbPathBar &bar)
{
    return bar.findChild<QToolButton *>(
        QStringLiteral("BreadcrumbOverflowButton"));
}

void showAndResize(BreadcrumbPathBar &bar, int width)
{
    bar.setMinimumWidth(0);
    bar.resize(width, 32);
    bar.show();
    QApplication::processEvents();
}

} // namespace

void TestBreadcrumb::archiveSegmentsFollowInternalDirectory()
{
    BreadcrumbPathBar bar;
    bar.setArchiveLocation(QStringLiteral("archive.zip"),
                           QStringLiteral("a/b/c"));

    const QList<QToolButton *> buttons = levelButtons(bar);
    QCOMPARE(buttons.size(), 4);
    QCOMPARE(buttons.at(0)->text(), QStringLiteral("archive.zip"));
    QCOMPARE(buttons.at(1)->text(), QStringLiteral("a"));
    QCOMPARE(buttons.at(2)->text(), QStringLiteral("b"));
    QCOMPARE(buttons.at(3)->text(), QStringLiteral("c"));
    QCOMPARE(bar.fullPathText(), QStringLiteral("archive.zip/a/b/c"));

    QSignalSpy spy(&bar, &BreadcrumbPathBar::archiveSegmentActivated);
    buttons.at(2)->click();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("a/b"));
}

void TestBreadcrumb::localSegmentsStartAtFilesystemRoot()
{
    BreadcrumbPathBar bar;
    const QString home = QDir::homePath();
    bar.setLocalLocation(home);

    const QList<QToolButton *> buttons = levelButtons(bar);
    QVERIFY(!buttons.isEmpty());
    QCOMPARE(bar.fullPathText(), QDir::toNativeSeparators(home));

    QSignalSpy spy(&bar, &BreadcrumbPathBar::localSegmentActivated);
    buttons.first()->click();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QDir::rootPath());
}

void TestBreadcrumb::wideBarShowsEveryLevel()
{
    BreadcrumbPathBar bar;
    bar.setArchiveLocation(QStringLiteral("archive.zip"),
                           QStringLiteral("one/two/three/four"));
    showAndResize(bar, 1400);

    const QList<QToolButton *> buttons = levelButtons(bar);
    for (QToolButton *button : buttons) {
        QVERIFY2(!button->isHidden(), qPrintable(button->text()));
    }
    QVERIFY(overflowButton(bar));
    QVERIFY(overflowButton(bar)->isHidden());
}

void TestBreadcrumb::narrowBarHidesMiddleLevelsBehindOverflow()
{
    BreadcrumbPathBar bar;
    bar.setArchiveLocation(QStringLiteral("archive.zip"),
                           QStringLiteral("one/two/three/four/five/six"));
    showAndResize(bar, 200);

    const QList<QToolButton *> buttons = levelButtons(bar);
    QVERIFY(buttons.size() > 2);
    QVERIFY2(!buttons.first()->isHidden(), "the root level must stay visible");
    QVERIFY2(!buttons.last()->isHidden(), "the current level must stay visible");

    int hidden = 0;
    for (QToolButton *button : buttons) {
        if (button->isHidden()) {
            ++hidden;
        }
    }
    QVERIFY2(hidden > 0, "a narrow bar must hide middle levels");
    QVERIFY(overflowButton(bar));
    QVERIFY2(!overflowButton(bar)->isHidden(),
             "hidden levels are represented by the overflow button");
}

void TestBreadcrumb::firstLevelElidesBeforeLast()
{
    BreadcrumbPathBar bar;
    bar.setArchiveLocation(
        QStringLiteral("a-very-long-archive-name.zip"),
        QStringLiteral("one-very-long/two-very-long/three-very-long"));
    showAndResize(bar, 120);

    const QList<QToolButton *> buttons = levelButtons(bar);
    QVERIFY(!buttons.isEmpty());
    QVERIFY2(buttons.first()->text().endsWith(QChar(0x2026)),
             qPrintable(buttons.first()->text()));
    QVERIFY2(!buttons.last()->isHidden(), "the current level stays visible");
}

void TestBreadcrumb::childWidgetClickOpensSelectableText()
{
    BreadcrumbPathBar bar;
    bar.setArchiveLocation(QStringLiteral("archive.zip"),
                           QStringLiteral("folder/subfolder"));
    showAndResize(bar, 500);

    QWidget *blank = bar.childAt(QPoint(bar.width() - 4, bar.height() / 2));
    QVERIFY(blank);
    QTest::mouseClick(blank, Qt::LeftButton, Qt::NoModifier,
                      blank->mapFrom(&bar, QPoint(bar.width() - 4,
                                                   bar.height() / 2)));

    QLineEdit *edit = bar.findChild<QLineEdit *>(QStringLiteral("BreadcrumbEdit"));
    QVERIFY(edit);
    QVERIFY(edit->isVisible());
    QVERIFY(edit->isReadOnly());
    QCOMPARE(edit->text(), bar.fullPathText());
    QVERIFY(edit->hasSelectedText());

    QTest::keyClick(edit, Qt::Key_Escape);
    QVERIFY(!edit->isVisible());
}

void TestBreadcrumb::overflowMenuListsHiddenLevels()
{
    BreadcrumbPathBar bar;
    bar.setArchiveLocation(QStringLiteral("archive.zip"),
                           QStringLiteral("one/two/three/four/five/six"));
    showAndResize(bar, 200);

    QToolButton *overflow = overflowButton(bar);
    QVERIFY(overflow);
    QVERIFY(!overflow->isHidden());
    overflow->click();
    QApplication::processEvents();

    QMenu *menu = bar.findChild<QMenu *>();
    QVERIFY(menu);
    QVERIFY(!menu->actions().isEmpty());

    int hidden = 0;
    for (QToolButton *button : levelButtons(bar)) {
        if (button->isHidden()) {
            ++hidden;
        }
    }
    QCOMPARE(menu->actions().size(), hidden);

    QSignalSpy spy(&bar, &BreadcrumbPathBar::archiveSegmentActivated);
    QAction *action = menu->actions().last();
    const QString expected = action->text();
    action->trigger();
    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.first().at(0).toString().isEmpty());
    QVERIFY(!expected.isEmpty());
}

QTEST_MAIN(TestBreadcrumb)
#include "tst_breadcrumb.moc"
