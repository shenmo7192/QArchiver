#ifndef ARCHIVEFOLDERPANE_H
#define ARCHIVEFOLDERPANE_H

// QArchiver / 小Q解压
// HaoZip-style left folder tree and details pane.
// 好压风格的左侧文件夹树与详细信息面板。

#include <QList>
#include <QWidget>

#include "common/ArchiveEntry.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QStackedWidget;
class QWidget;

// Left pane of the modern layout: archive folder tree + selected-entry details.
// 现代布局的左侧面板：压缩包文件夹树 + 选中条目详细信息。
class ArchiveFolderPane : public QWidget {
    Q_OBJECT
public:
    explicit ArchiveFolderPane(QWidget *parent = nullptr);

    void setEntries(const QList<ArchiveEntry> &entries);
    void clear();
    void setCurrentDirectory(const QString &directory);
    void setSelection(const QList<ArchiveEntry> &entries);

signals:
    void directoryActivated(const QString &path);

private:
    void rebuildTree();
    QStringList directoryPaths() const;

    QTreeWidget *m_tree = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QStackedWidget *m_detailsStack = nullptr;
    QLabel *m_nameValue = nullptr;
    QLabel *m_typeValue = nullptr;
    QLabel *m_sizeValue = nullptr;
    QLabel *m_packedValue = nullptr;
    QLabel *m_ratioValue = nullptr;
    QLabel *m_modifiedValue = nullptr;
    QLabel *m_pathValue = nullptr;
    QList<ArchiveEntry> m_entries;
};

#endif // ARCHIVEFOLDERPANE_H
