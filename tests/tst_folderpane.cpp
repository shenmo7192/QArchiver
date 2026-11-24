// QArchiver / 小Q解压
// Regression tests for the modern layout's system-places sidebar.
// 现代布局系统位置侧栏的回归测试。

#include <QApplication>
#include <QDir>
#include <QSet>
#include <QTreeWidget>
#include <QtTest>

#include "ui/widgets/ArchiveFolderPane.h"

class TestFolderPane : public QObject {
    Q_OBJECT

private slots:
    void localPlacesDoNotRepeatPaths();
    void subdirectoryKeepsAncestorSelected();
    void outsideHomeSelectsFileSystemPlace();
};

namespace {
QTreeWidget *placesTree(ArchiveFolderPane &pane)
{
    pane.setLocalMode(true);
    return pane.findChild<QTreeWidget *>();
}

QString itemPath(const QTreeWidgetItem *item)
{
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

QTreeWidgetItem *findPath(QTreeWidget *tree, const QString &path)
{
    const QString wanted = QDir::cleanPath(path);
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = tree->topLevelItem(i);
        if (QDir::cleanPath(itemPath(item)) == wanted) {
            return item;
        }
    }
    return nullptr;
}
} // namespace

void TestFolderPane::localPlacesDoNotRepeatPaths()
{
    ArchiveFolderPane pane;
    QTreeWidget *tree = placesTree(pane);
    QVERIFY(tree);
    QVERIFY(tree->topLevelItemCount() > 0);

    QSet<QString> seen;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        const QString path = itemPath(tree->topLevelItem(i));
        QVERIFY2(!path.isEmpty(), "every place must carry an absolute path");
        QVERIFY2(!seen.contains(path),
                 qPrintable(QStringLiteral("duplicate place path: %1").arg(path)));
        seen.insert(path);
    }
}

void TestFolderPane::subdirectoryKeepsAncestorSelected()
{
    ArchiveFolderPane pane;
    QTreeWidget *tree = placesTree(pane);
    QVERIFY(tree);

    QTreeWidgetItem *home = findPath(tree, QDir::homePath());
    QVERIFY2(home, "the Home place should always be available");

    pane.setCurrentLocalPath(
        QDir::home().filePath(QStringLiteral("some/sub/directory")));
    QCOMPARE(tree->currentItem(), home);

    // Selecting another place and then entering a directory below Home must
    // move the highlight back instead of keeping the stale previous place.
    // 选中另一个位置后再进入 Home 下的目录，必须切回 Home，而不是残留旧高亮。
    QTreeWidgetItem *rootPlace = findPath(tree, QDir::rootPath());
    QVERIFY(rootPlace);
    pane.setCurrentLocalPath(QDir::rootPath());
    QCOMPARE(tree->currentItem(), rootPlace);

    pane.setCurrentLocalPath(QDir::home().filePath(QStringLiteral("Documents")));
    QCOMPARE(tree->currentItem(), home);
}

void TestFolderPane::outsideHomeSelectsFileSystemPlace()
{
    ArchiveFolderPane pane;
    QTreeWidget *tree = placesTree(pane);
    QVERIFY(tree);

    QTreeWidgetItem *rootPlace = findPath(tree, QDir::rootPath());
    QVERIFY(rootPlace);

    const QString outside =
        QDir(QDir::rootPath())
            .filePath(QStringLiteral("qarchiver-test-outside-home"));
    pane.setCurrentLocalPath(outside);
    QCOMPARE(tree->currentItem(), rootPlace);
}

QTEST_MAIN(TestFolderPane)
#include "tst_folderpane.moc"
