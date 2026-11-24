#ifndef WELCOMEPAGE_H
#define WELCOMEPAGE_H

// QArchiver / 小Q解压
// Modern start page: drop zone, quick actions and recent archives.
// 现代起始页：拖放区、快捷操作与最近打开。

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QLabel;

// Shown when no archive is open in the modern layout.
// 现代布局中未打开压缩包时显示。
class WelcomePage : public QWidget {
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);

public slots:
    void refreshRecent();

signals:
    void openRequested();
    void oneClickExtractRequested();
    void fileManagerRequested();
    void recentFileActivated(const QString &path);

private:
    void activateItem(QListWidgetItem *item);

    QListWidget *m_recentList = nullptr;
    QLabel *m_recentEmpty = nullptr;
};

#endif // WELCOMEPAGE_H
