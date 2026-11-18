#ifndef BACKGROUNDWIDGET_H
#define BACKGROUNDWIDGET_H

// QArchiver / 小Q解压
// Widget that paints an optional semi-transparent background image.
// 绘制可选半透明背景图片的控件。

#include <QPixmap>
#include <QWidget>

#include "utils/BackgroundImage.h"

// A container widget that paints a theme background and, optionally, a
// semi-transparent image behind its children. Set the image with
// setBackgroundImage() or load the persisted options with reloadFromSettings().
// 一个容器控件：绘制主题背景，并可选择在子控件之后绘制半透明图片。
// 使用 setBackgroundImage() 设置图片，或使用 reloadFromSettings() 读取持久化选项。
class BackgroundWidget : public QWidget {
    Q_OBJECT
public:
    explicit BackgroundWidget(QWidget *parent = nullptr);

    // Set and paint the given background image. An invalid path or an image
    // that cannot be loaded simply results in no image being drawn.
    // 设置并绘制给定背景图片。路径无效或图片加载失败时不绘制图片。
    void setBackgroundImage(const BackgroundImageOptions &options);
    const BackgroundImageOptions &backgroundImage() const { return m_options; }

public slots:
    // Read the background image options from AppSettings and apply them.
    // 从 AppSettings 读取背景图片选项并应用。
    void reloadFromSettings();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    BackgroundImageOptions m_options;
    QPixmap m_pixmap;
};

#endif // BACKGROUNDWIDGET_H
