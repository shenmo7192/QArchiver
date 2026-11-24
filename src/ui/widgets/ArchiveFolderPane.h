#ifndef ARCHIVEFOLDERPANE_H
#define ARCHIVEFOLDERPANE_H

// QArchiver / 小Q解压
// HaoZip-style left folder tree and details pane.
// 好压风格的左侧文件夹树与详细信息面板。

#include <QElapsedTimer>
#include <QList>
#include <QWidget>

#include "common/ArchiveEntry.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QStackedWidget;
class QShowEvent;
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
    // Switch the pane between archive folders and system places.
    // 在压缩包文件夹树与系统位置侧栏之间切换。
    void setLocalMode(bool local);
    void setCurrentLocalPath(const QString &path);

signals:
    void directoryActivated(const QString &path);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void rebuildTree();
    void rebuildLocalPlaces();
    // Refresh the system-place list at most once per interval; called when the
    // pane is shown or a directory outside the known places is opened.
    // 限频刷新系统位置列表：仅在窗口重新显示或打开未知位置下的目录时调用。
    void refreshLocalPlacesIfNeeded();
    QStringList directoryPaths() const;

    QTreeWidget *m_tree = nullptr;
    QLabel *m_header = nullptr;
    QWidget *m_detailsPane = nullptr;
    bool m_localMode = false;
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
    QElapsedTimer m_placesRefreshTimer;
};

#endif // ARCHIVEFOLDERPANE_H
