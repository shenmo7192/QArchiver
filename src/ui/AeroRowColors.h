#ifndef AEROROWCOLORS_H
#define AEROROWCOLORS_H

// QArchiver / 小Q解压
// Shared Aero row colors for the file list delegate and tree branches.
// 文件列表 delegate 与树分支共用的 Aero 行配色。

#include <QColor>
#include <QLinearGradient>
#include <QRect>

struct AeroRowColors {
    QColor top;
    QColor bottom;
    QColor border;
};

inline AeroRowColors aeroSelectedRowColors(bool dark, bool active)
{
    if (dark) {
        return active
            ? AeroRowColors{QColor(QStringLiteral("#1f4a6e")),
                            QColor(QStringLiteral("#173a57")),
                            QColor(QStringLiteral("#2a82da"))}
            : AeroRowColors{QColor(QStringLiteral("#333333")),
                            QColor(QStringLiteral("#2a2a2a")),
                            QColor(QStringLiteral("#3a3a3a"))};
    }
    return active
        ? AeroRowColors{QColor(QStringLiteral("#dceeff")),
                        QColor(QStringLiteral("#b9dcf7")),
                        QColor(QStringLiteral("#7fb2d9"))}
        : AeroRowColors{QColor(QStringLiteral("#e8e8e8")),
                        QColor(QStringLiteral("#d8d8d8")),
                        QColor(QStringLiteral("#c0c0c0"))};
}

inline AeroRowColors aeroHoverRowColors(bool dark)
{
    if (dark) {
        return AeroRowColors{QColor(QStringLiteral("#2f3d4b")),
                             QColor(QStringLiteral("#243443")),
                             QColor(QStringLiteral("#2a82da"))};
    }
    return AeroRowColors{QColor(QStringLiteral("#f2f9ff")),
                         QColor(QStringLiteral("#e3f2fd")),
                         QColor(QStringLiteral("#cfe8fb"))};
}

// Flat HaoZip-style row colours used by the modern layout.
// 现代好压布局使用的扁平行配色。
inline AeroRowColors modernSelectedRowColors(bool dark, bool active)
{
    if (dark) {
        return active
            ? AeroRowColors{QColor(QStringLiteral("#24425f")),
                            QColor(QStringLiteral("#24425f")),
                            QColor(QStringLiteral("#3b6f9e"))}
            : AeroRowColors{QColor(QStringLiteral("#2a3340")),
                            QColor(QStringLiteral("#2a3340")),
                            QColor(QStringLiteral("#3a4552"))};
    }
    return active
        ? AeroRowColors{QColor(QStringLiteral("#dcebff")),
                        QColor(QStringLiteral("#dcebff")),
                        QColor(QStringLiteral("#b9d7fb"))}
        : AeroRowColors{QColor(QStringLiteral("#eef1f5")),
                        QColor(QStringLiteral("#eef1f5")),
                        QColor(QStringLiteral("#e1e6ec"))};
}

inline AeroRowColors modernHoverRowColors(bool dark)
{
    if (dark) {
        return AeroRowColors{QColor(QStringLiteral("#26313e")),
                             QColor(QStringLiteral("#26313e")),
                             QColor(QStringLiteral("#33404e"))};
    }
    return AeroRowColors{QColor(QStringLiteral("#f2f7ff")),
                         QColor(QStringLiteral("#f2f7ff")),
                         QColor(QStringLiteral("#e3edf8"))};
}

inline QLinearGradient aeroRowGradient(const QRect &rect,
                                       const AeroRowColors &colors)
{
    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    gradient.setColorAt(0.0, colors.top);
    gradient.setColorAt(1.0, colors.bottom);
    return gradient;
}

#endif // AEROROWCOLORS_H
