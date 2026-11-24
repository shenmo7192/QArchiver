// QArchiver / 小Q解压
// Shared elided path display implementation.
// 共用路径显示控件实现。

#include "ui/widgets/PathLabel.h"

#include <QFontMetrics>
#include <QResizeEvent>

PathLabel::PathLabel(QWidget *parent)
    : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setMinimumWidth(80);
}

void PathLabel::setLocation(const QString &archiveName,
                            const QString &internalDir)
{
    QString text = archiveName;
    if (!internalDir.isEmpty()) {
        text += QStringLiteral("  /  ") + internalDir;
    }
    m_fullText = text;
    setToolTip(text);
    updateElision();
}

void PathLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    updateElision();
}

void PathLabel::updateElision()
{
    if (m_fullText.isEmpty()) {
        QLabel::setText(QString());
        return;
    }
    const QFontMetrics metrics(font());
    QLabel::setText(metrics.elidedText(m_fullText, Qt::ElideMiddle,
                                       qMax(40, width() - 4)));
}
