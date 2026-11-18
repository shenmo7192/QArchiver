// QArchiver / 小Q解压
// BackgroundWidget painting and settings loading.
// BackgroundWidget 的绘制与设置加载。

#include "BackgroundWidget.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include "core/AppSettings.h"
#include "utils/Logger.h"

BackgroundWidget::BackgroundWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("MainBackground"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
}

void BackgroundWidget::setBackgroundImage(const BackgroundImageOptions &options)
{
    m_options = options;
    m_pixmap = options.isValid()
        ? BackgroundImage::loadPixmap(options.path)
        : QPixmap();

    if (options.isValid() && m_pixmap.isNull()) {
        LOG_WARN_S("BackgroundWidget",
                   QStringLiteral("Failed to load background image: %1")
                       .arg(options.path));
    }
    update();
}

void BackgroundWidget::reloadFromSettings()
{
    AppSettings &settings = AppSettings::instance();

    BackgroundImageOptions options;
    options.path = settings.backgroundImagePath();
    options.alignment = BackgroundImage::alignmentFromString(
        settings.backgroundImageAlignment());
    options.size = settings.backgroundImageSize();
    options.opacity = settings.backgroundImageOpacity();
    setBackgroundImage(options);
}

void BackgroundWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    // Let the style sheet paint the base background (usually the Aero content
    // color), then draw the optional image on top of it and below children.
    // 先让样式表绘制基础背景（通常是 Aero 内容区颜色），
    // 再在其上、子控件之下绘制可选图片。
    QStyleOption option;
    option.initFrom(this);
    if (testAttribute(Qt::WA_StyledBackground)) {
        style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
    } else {
        painter.fillRect(rect(), palette().brush(QPalette::Base));
    }

    BackgroundImage::paint(&painter, rect(), m_options, m_pixmap);
}
