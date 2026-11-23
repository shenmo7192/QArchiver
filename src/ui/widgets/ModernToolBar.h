#ifndef MODERNTOOLBAR_H
#define MODERNTOOLBAR_H

// QArchiver / 小Q解压
// HaoZip-style flat header toolbar with large colourful buttons.
// 好压风格的大型彩色按钮扁平头部工具栏。

#include <QWidget>

class QAction;
class QHBoxLayout;
class QLabel;

// Large icon + label buttons on the left, application brand on the right.
// 左侧为大图标加文字按钮，右侧为应用品牌区。
class ModernToolBar : public QWidget {
    Q_OBJECT
public:
    explicit ModernToolBar(QWidget *parent = nullptr);

    void addToolAction(QAction *action);
    void addSmallAction(QAction *action);
    void addSeparator();

private:
    QHBoxLayout *m_actionLayout = nullptr;
    QLabel *m_brandTitle = nullptr;
    QLabel *m_brandTagline = nullptr;
};

#endif // MODERNTOOLBAR_H
