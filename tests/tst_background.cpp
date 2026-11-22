// QArchiver / 小Q解压
// Unit tests for background image alignment, scaling and painting.
// 背景图片对齐、缩放与绘制辅助函数的单元测试。

#include <QtTest>

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryDir>

#include "utils/BackgroundImage.h"

class TestBackground : public QObject {
    Q_OBJECT

private slots:
    void alignmentRoundTrip_data();
    void alignmentRoundTrip();
    void alignmentDefaultsToBottomRight();
    void scaledSize_data();
    void scaledSize();
    void targetRect_data();
    void targetRect();
    void paintHonorsOpacityAndAlignment();
    void loadPixmapIsCachedAndBounded();
};

// All supported alignment tokens must survive a string round-trip.
// 所有支持的对齐标记都必须能完成字符串往返。
void TestBackground::alignmentRoundTrip_data()
{
    QTest::addColumn<QString>("token");

    for (const char *token : {"topLeft", "top", "topRight", "left", "center",
                              "right", "bottomLeft", "bottom", "bottomRight"}) {
        QTest::newRow(token) << QString::fromLatin1(token);
    }
}

void TestBackground::alignmentRoundTrip()
{
    QFETCH(QString, token);

    const Qt::Alignment alignment = BackgroundImage::alignmentFromString(token);
    QCOMPARE(BackgroundImage::alignmentToString(alignment), token);
}

// Unknown tokens fall back to the bottom-right default.
// 未知标记回退到右下角默认值。
void TestBackground::alignmentDefaultsToBottomRight()
{
    const Qt::Alignment expected = Qt::AlignRight | Qt::AlignBottom;
    QCOMPARE(BackgroundImage::alignmentFromString(QStringLiteral("nonsense")),
             expected);
    QCOMPARE(BackgroundImage::alignmentToString(expected),
             QStringLiteral("bottomRight"));
}

// Zero dimensions mean natural size or aspect-preserving scaling.
// 零尺寸表示原始大小或保持宽高比缩放。
void TestBackground::scaledSize_data()
{
    QTest::addColumn<QSize>("source");
    QTest::addColumn<QSize>("requested");
    QTest::addColumn<QSize>("expected");

    QTest::newRow("natural") << QSize(200, 100) << QSize(0, 0)
                             << QSize(200, 100);
    QTest::newRow("width only") << QSize(200, 100) << QSize(100, 0)
                                << QSize(100, 50);
    QTest::newRow("height only") << QSize(200, 100) << QSize(0, 50)
                                 << QSize(100, 50);
    QTest::newRow("exact") << QSize(200, 100) << QSize(80, 40)
                           << QSize(80, 40);
    QTest::newRow("empty source") << QSize(0, 0) << QSize(80, 40)
                                  << QSize();
}

void TestBackground::scaledSize()
{
    QFETCH(QSize, source);
    QFETCH(QSize, requested);
    QFETCH(QSize, expected);

    QCOMPARE(BackgroundImage::scaledSize(source, requested), expected);
}

// Each alignment places the image at the expected corner/edge/center.
// 每种对齐方式都会把图片放到预期的角、边或中心。
void TestBackground::targetRect_data()
{
    QTest::addColumn<QString>("token");
    QTest::addColumn<QPoint>("expected");

    QTest::newRow("topLeft") << QStringLiteral("topLeft") << QPoint(10, 20);
    QTest::newRow("top") << QStringLiteral("top") << QPoint(110, 20);
    QTest::newRow("topRight") << QStringLiteral("topRight") << QPoint(210, 20);
    QTest::newRow("left") << QStringLiteral("left") << QPoint(10, 95);
    QTest::newRow("center") << QStringLiteral("center") << QPoint(110, 95);
    QTest::newRow("right") << QStringLiteral("right") << QPoint(210, 95);
    QTest::newRow("bottomLeft") << QStringLiteral("bottomLeft") << QPoint(10, 170);
    QTest::newRow("bottom") << QStringLiteral("bottom") << QPoint(110, 170);
    QTest::newRow("bottomRight") << QStringLiteral("bottomRight")
                                 << QPoint(210, 170);
}

void TestBackground::targetRect()
{
    QFETCH(QString, token);
    QFETCH(QPoint, expected);

    const QRect target(10, 20, 300, 200);
    const QRect result = BackgroundImage::targetRect(
        QSize(100, 50), target,
        BackgroundImage::alignmentFromString(token));
    QCOMPARE(result.topLeft(), expected);
    QCOMPARE(result.size(), QSize(100, 50));
}

// The painter applies opacity and the configured alignment.
// 绘制函数会应用透明度与配置的对齐方式。
void TestBackground::paintHonorsOpacityAndAlignment()
{
    QPixmap source(10, 10);
    source.fill(Qt::red);

    BackgroundImageOptions options;
    options.path = QStringLiteral("unused.png");
    options.size = QSize(10, 10);
    options.alignment = Qt::AlignCenter;
    options.opacity = 0.5;
    options.enabled = true;
    QVERIFY(options.isValid());

    QImage target(20, 20, QImage::Format_ARGB32_Premultiplied);
    target.fill(Qt::transparent);
    {
        QPainter painter(&target);
        BackgroundImage::paint(&painter, target.rect(), options, source);
    }

    // Center 10x10 at 5,5; outside remains transparent.
    // 10x10 的图片居中于 5,5；外部保持透明。
    const QColor inside = target.pixelColor(10, 10);
    QCOMPARE(inside.red(), 255);
    QCOMPARE(inside.green(), 0);
    QCOMPARE(inside.blue(), 0);
    QVERIFY(qAbs(inside.alpha() - 128) <= 1);
    QCOMPARE(target.pixelColor(0, 0).alpha(), 0);
}

// The shared background cache returns the same decoded pixmap for an
// unchanged file and can be cleared.
// 共享背景缓存对未变化的文件返回同一 pixmap，并支持清空。
void TestBackground::loadPixmapIsCachedAndBounded()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath(QStringLiteral("background.png"));

    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(QColor(12, 34, 56, 128));
    QVERIFY(image.save(path));

    BackgroundImage::clearPixmapCache();
    const QPixmap first = BackgroundImage::loadPixmap(path);
    const QPixmap second = BackgroundImage::loadPixmap(path);
    QVERIFY(!first.isNull());
    QVERIFY(!second.isNull());
    QCOMPARE(first.cacheKey(), second.cacheKey());

    BackgroundImage::clearPixmapCache();
    QVERIFY(BackgroundImage::loadPixmap(QString()).isNull());
    QVERIFY(BackgroundImage::loadPixmap(temp.filePath(QStringLiteral("missing.png"))).isNull());
}

QTEST_MAIN(TestBackground)
#include "tst_background.moc"
