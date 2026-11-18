#ifndef BACKGROUNDIMAGE_H
#define BACKGROUNDIMAGE_H

// QArchiver / 小Q解压
// Background image options and painting for Aero-style content areas.
// Aero 风格内容区的背景图片选项与绘制辅助。

#include <QRect>
#include <QSize>
#include <QString>
#include <Qt>

class QPainter;
class QPixmap;

// Options for a semi-transparent background image drawn behind content.
// 绘制在内容之后、可半透明显示的背景图片选项。
struct BackgroundImageOptions {
    // Image file path; empty disables the background image.
    // 图片文件路径；为空表示不显示背景图片。
    QString path;

    // Alignment inside the target widget. Use combinations such as
    // Qt::AlignRight | Qt::AlignBottom; the four edges and the four corners
    // are all expressed with these flags.
    // 在目标控件内的对齐方式。使用 Qt::AlignRight | Qt::AlignBottom 等组合；
    // 四条边和四个角都用这些标志表达。
    Qt::Alignment alignment = Qt::AlignRight | Qt::AlignBottom;

    // Requested logical image size. An invalid/empty size uses the image's
    // natural size; a zero width or height preserves the aspect ratio.
    // 请求的逻辑尺寸。无效/空尺寸使用图片原始大小；宽或高为 0 时保持宽高比。
    QSize size;

    // Opacity in [0.0, 1.0]. / 透明度，范围 [0.0, 1.0]。
    qreal opacity = 0.25;

    // When false, paint() does nothing. / 为 false 时 paint() 不绘制。
    bool enabled = true;

    bool isValid() const
    {
        return enabled && !path.isEmpty() && opacity > 0.0;
    }
};

// Helpers shared by the settings layer, the preferences dialog and the widget
// that paints the background. The functions are pure and do not touch the
// filesystem except loadPixmap().
// 设置层、偏好设置对话框和绘制控件共用的辅助函数。
// 除 loadPixmap() 外，这些函数都是纯函数，不访问文件系统。
class BackgroundImage {
public:
    static QString alignmentToString(Qt::Alignment alignment);
    static Qt::Alignment alignmentFromString(const QString &value);

    // Scale sourceSize according to requested. Both zero => natural size,
    // one zero => preserve aspect ratio, both positive => exact size.
    // 根据 requested 缩放 sourceSize。两者都为 0 时使用原始大小，
    // 一个为 0 时保持宽高比，两个都为正数时使用精确尺寸。
    static QSize scaledSize(const QSize &sourceSize, const QSize &requested);

    // Target rectangle for an already scaled image inside target.
    // 已缩放图片在 target 内的目标矩形。
    static QRect targetRect(const QSize &imageSize, const QRect &target,
                            Qt::Alignment alignment);

    static QPixmap loadPixmap(const QString &path);

    // Draw the image into target with the configured opacity/alignment.
    // 按配置的透明度与对齐方式把图片绘制到 target。
    static void paint(QPainter *painter, const QRect &target,
                      const BackgroundImageOptions &options,
                      const QPixmap &pixmap);
};

#endif // BACKGROUNDIMAGE_H
