// QArchiver / 小Q解压
// Background image loading, scaling, alignment and painting.
// 背景图片的加载、缩放、对齐与绘制。

#include "BackgroundImage.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <algorithm>

namespace {

int alignedOffset(int targetStart, int targetLength, int imageLength,
                  bool first, bool center)
{
    if (first) {
        return targetStart;
    }
    if (center) {
        return targetStart + (targetLength - imageLength) / 2;
    }
    // Align to the far edge (right or bottom).
    return targetStart + targetLength - imageLength;
}

} // namespace

QString BackgroundImage::alignmentToString(Qt::Alignment alignment)
{
    const bool left = alignment.testFlag(Qt::AlignLeft);
    const bool right = alignment.testFlag(Qt::AlignRight);
    const bool top = alignment.testFlag(Qt::AlignTop);
    const bool bottom = alignment.testFlag(Qt::AlignBottom);

    QString horizontal;
    if (left && !right) {
        horizontal = QStringLiteral("left");
    } else if (right && !left) {
        horizontal = QStringLiteral("right");
    } else {
        horizontal = QStringLiteral("center");
    }

    QString vertical;
    if (top && !bottom) {
        vertical = QStringLiteral("top");
    } else if (bottom && !top) {
        vertical = QStringLiteral("bottom");
    } else {
        vertical = QStringLiteral("center");
    }

    if (horizontal == QStringLiteral("center")
        && vertical == QStringLiteral("center")) {
        return QStringLiteral("center");
    }
    if (horizontal == QStringLiteral("center")) {
        return vertical; // top / bottom
    }
    if (vertical == QStringLiteral("center")) {
        return horizontal; // left / right
    }

    // Corners: topLeft, topRight, bottomLeft, bottomRight.
    // 四角：topLeft、topRight、bottomLeft、bottomRight。
    const QString verticalPart =
        vertical == QStringLiteral("top") ? QStringLiteral("top")
                                          : QStringLiteral("bottom");
    const QString horizontalPart =
        horizontal == QStringLiteral("left") ? QStringLiteral("Left")
                                             : QStringLiteral("Right");
    return verticalPart + horizontalPart;
}

Qt::Alignment BackgroundImage::alignmentFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("topleft")) {
        return Qt::AlignLeft | Qt::AlignTop;
    }
    if (normalized == QStringLiteral("top")) {
        return Qt::AlignHCenter | Qt::AlignTop;
    }
    if (normalized == QStringLiteral("topright")) {
        return Qt::AlignRight | Qt::AlignTop;
    }
    if (normalized == QStringLiteral("left")) {
        return Qt::AlignLeft | Qt::AlignVCenter;
    }
    if (normalized == QStringLiteral("center")) {
        return Qt::AlignCenter;
    }
    if (normalized == QStringLiteral("right")) {
        return Qt::AlignRight | Qt::AlignVCenter;
    }
    if (normalized == QStringLiteral("bottomleft")) {
        return Qt::AlignLeft | Qt::AlignBottom;
    }
    if (normalized == QStringLiteral("bottom")) {
        return Qt::AlignHCenter | Qt::AlignBottom;
    }
    if (normalized == QStringLiteral("bottomright")) {
        return Qt::AlignRight | Qt::AlignBottom;
    }
    // Unknown values use the default position.
    // 未知值使用默认位置。
    return Qt::AlignRight | Qt::AlignBottom;
}

QSize BackgroundImage::scaledSize(const QSize &sourceSize,
                                  const QSize &requested)
{
    if (sourceSize.isEmpty()) {
        return {};
    }

    const int requestedWidth = requested.width();
    const int requestedHeight = requested.height();

    if (requestedWidth <= 0 && requestedHeight <= 0) {
        return sourceSize;
    }
    if (requestedWidth <= 0) {
        const qreal scale = qreal(requestedHeight) / sourceSize.height();
        return QSize(std::max(1, qRound(sourceSize.width() * scale)),
                     requestedHeight);
    }
    if (requestedHeight <= 0) {
        const qreal scale = qreal(requestedWidth) / sourceSize.width();
        return QSize(requestedWidth,
                     std::max(1, qRound(sourceSize.height() * scale)));
    }
    return QSize(requestedWidth, requestedHeight);
}

QRect BackgroundImage::targetRect(const QSize &imageSize, const QRect &target,
                                  Qt::Alignment alignment)
{
    if (imageSize.isEmpty() || target.isEmpty()) {
        return {};
    }

    const bool left = alignment.testFlag(Qt::AlignLeft);
    const bool right = alignment.testFlag(Qt::AlignRight);
    const bool top = alignment.testFlag(Qt::AlignTop);
    const bool bottom = alignment.testFlag(Qt::AlignBottom);
    const bool hcenter = alignment.testFlag(Qt::AlignHCenter) || (!left && !right);
    const bool vcenter = alignment.testFlag(Qt::AlignVCenter) || (!top && !bottom);

    const int x = alignedOffset(target.x(), target.width(), imageSize.width(),
                                left && !right, hcenter);
    const int y = alignedOffset(target.y(), target.height(), imageSize.height(),
                                top && !bottom, vcenter);
    return QRect(QPoint(x, y), imageSize);
}

namespace {
struct CachedPixmap {
    QPixmap pixmap;
    qint64 mtimeMs = 0;
    qint64 size = 0;
    quint64 lastUsed = 0;
};

QHash<QString, CachedPixmap> &pixmapCache()
{
    static QHash<QString, CachedPixmap> cache;
    return cache;
}

quint64 g_cacheClock = 0;
constexpr int kMaxCachedPixmaps = 8;
} // namespace

QPixmap BackgroundImage::loadPixmap(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    const QString key = info.absoluteFilePath();
    const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
    const qint64 size = info.size();
    const quint64 used = ++g_cacheClock;

    auto &cache = pixmapCache();
    auto it = cache.find(key);
    if (it != cache.end() && it->mtimeMs == mtimeMs && it->size == size
        && !it->pixmap.isNull()) {
        it->lastUsed = used;
        return it->pixmap;
    }

    // QImageReader respects the PNG alpha channel and also supports the other
    // image formats Qt was built with.
    // QImageReader 会保留 PNG 透明通道，并支持 Qt 构建时启用的其他图片格式。
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        cache.remove(key);
        return {};
    }

    if (cache.size() >= kMaxCachedPixmaps && !cache.contains(key)) {
        auto oldest = cache.begin();
        for (auto candidate = cache.begin(); candidate != cache.end(); ++candidate) {
            if (candidate->lastUsed < oldest->lastUsed) {
                oldest = candidate;
            }
        }
        cache.erase(oldest);
    }

    const QPixmap pixmap = QPixmap::fromImage(image);
    cache.insert(key, CachedPixmap{pixmap, mtimeMs, size, used});
    return pixmap;
}

void BackgroundImage::clearPixmapCache()
{
    pixmapCache().clear();
}

void BackgroundImage::paint(QPainter *painter, const QRect &target,
                           const BackgroundImageOptions &options,
                           const QPixmap &pixmap)
{
    if (!painter || !options.isValid() || pixmap.isNull() || target.isEmpty()) {
        return;
    }

    const qreal dpr = pixmap.devicePixelRatio() > 0.0
        ? pixmap.devicePixelRatio()
        : 1.0;
    const QSize naturalSize(qRound(pixmap.width() / dpr),
                            qRound(pixmap.height() / dpr));
    const QSize drawnSize = scaledSize(naturalSize, options.size);
    const QRect destination = targetRect(drawnSize, target, options.alignment);
    if (destination.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setOpacity(std::clamp(options.opacity, 0.0, 1.0));
    painter->drawPixmap(destination, pixmap);
    painter->restore();
}
