// QArchiver / 小Q解压
// ThemeManager implementation: Aero palettes, fonts and the Qt style sheet.
// ThemeManager 实现：Aero 调色板、字体与 Qt 样式表。

#include "ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QPalette>
#include <QProxyStyle>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOption>
#include <QDialogButtonBox>

#include "core/AppSettings.h"
#include "utils/Logger.h"

namespace {

// ---------------------------------------------------------------------------
// Palettes
// ---------------------------------------------------------------------------

QPalette buildPalette(bool dark)
{
    QPalette p;

    if (dark) {
        const QColor window(30, 30, 30);
        const QColor windowText(230, 230, 230);
        const QColor base(37, 37, 38);
        const QColor alternateBase(45, 45, 48);
        const QColor toolTipBase(45, 45, 48);
        const QColor toolTipText(230, 230, 230);
        const QColor text(230, 230, 230);
        const QColor button(45, 45, 48);
        const QColor buttonText(230, 230, 230);
        const QColor brightText(255, 255, 255);
        const QColor link(42, 130, 218);
        const QColor highlight(42, 130, 218);
        const QColor highlightedText(255, 255, 255);
        const QColor placeholder(150, 150, 150);

        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, windowText);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternateBase);
        p.setColor(QPalette::ToolTipBase, toolTipBase);
        p.setColor(QPalette::ToolTipText, toolTipText);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, buttonText);
        p.setColor(QPalette::BrightText, brightText);
        p.setColor(QPalette::Link, link);
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, highlightedText);
        p.setColor(QPalette::PlaceholderText, placeholder);
        p.setColor(QPalette::Light, base.lighter(150));
        p.setColor(QPalette::Midlight, base.lighter(125));
        p.setColor(QPalette::Mid, base.darker(125));
        p.setColor(QPalette::Dark, base.darker(160));
        p.setColor(QPalette::Shadow, base.darker(200));

        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(110, 110, 110));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 110, 110));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 110, 110));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(60, 60, 60));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(130, 130, 130));
    } else {
        const QColor window(240, 240, 240);
        const QColor windowText(26, 26, 26);
        const QColor base(255, 255, 255);
        const QColor alternateBase(247, 249, 252);
        const QColor toolTipBase(255, 255, 255);
        const QColor toolTipText(26, 26, 26);
        const QColor text(26, 26, 26);
        const QColor button(240, 240, 240);
        const QColor buttonText(26, 26, 26);
        const QColor brightText(255, 255, 255);
        const QColor link(0, 102, 204);
        const QColor highlight(51, 153, 255);
        const QColor highlightedText(255, 255, 255);
        const QColor placeholder(109, 109, 109);

        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, windowText);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternateBase);
        p.setColor(QPalette::ToolTipBase, toolTipBase);
        p.setColor(QPalette::ToolTipText, toolTipText);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, buttonText);
        p.setColor(QPalette::BrightText, brightText);
        p.setColor(QPalette::Link, link);
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, highlightedText);
        p.setColor(QPalette::PlaceholderText, placeholder);
        p.setColor(QPalette::Light, base.lighter(150));
        p.setColor(QPalette::Midlight, base.lighter(125));
        p.setColor(QPalette::Mid, base.darker(125));
        p.setColor(QPalette::Dark, base.darker(160));
        p.setColor(QPalette::Shadow, base.darker(200));

        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(160, 160, 160));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(160, 160, 160));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(160, 160, 160));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(200, 200, 200));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(160, 160, 160));
    }

    return p;
}

// ---------------------------------------------------------------------------
// Aero style sheet colors
// ---------------------------------------------------------------------------

void addAssetTokens(QHash<QString, QString> &tokens, const QString &suffix)
{
    const auto add = [&tokens, &suffix](const char *key, const char *file) {
        tokens.insert(QString::fromLatin1(key),
                      QStringLiteral(":/styles/%1%2.png")
                          .arg(QString::fromLatin1(file), suffix));
    };
    add("arrowDown", "arrow-down");
    add("branchClosed", "branch-closed");
    add("branchOpen", "branch-open");
    add("comboArrow", "combo-arrow");
    add("dockClose", "dock-close");
    add("dockFloat", "dock-float");
    add("scrollDown", "scroll-down");
    add("scrollLeft", "scroll-left");
    add("scrollRight", "scroll-right");
    add("scrollUp", "scroll-up");
    add("sortDown", "sort-down");
    add("sortUp", "sort-up");
    add("spinDown", "spin-down");
    add("spinUp", "spin-up");
}

QHash<QString, QString> lightTokens()
{
    QHash<QString, QString> tokens = {
        {QStringLiteral("window"), QStringLiteral("#f0f0f0")},
        {QStringLiteral("text"), QStringLiteral("#1a1a1a")},
        {QStringLiteral("mutedText"), QStringLiteral("#6d6d6d")},
        {QStringLiteral("base"), QStringLiteral("#ffffff")},
        {QStringLiteral("alternateBase"), QStringLiteral("#f7f9fc")},
        {QStringLiteral("border"), QStringLiteral("#a0b4c8")},
        {QStringLiteral("borderLight"), QStringLiteral("#c8d6e3")},
        {QStringLiteral("inputBorder"), QStringLiteral("#8e9aa5")},
        {QStringLiteral("focusBorder"), QStringLiteral("#3c7fb1")},
        {QStringLiteral("accent"), QStringLiteral("#3c7fb1")},
        {QStringLiteral("accentDark"), QStringLiteral("#2c6a99")},
        {QStringLiteral("selection"), QStringLiteral("#3399ff")},
        {QStringLiteral("selectionText"), QStringLiteral("#ffffff")},
        {QStringLiteral("hoverTop"), QStringLiteral("#eaf6ff")},
        {QStringLiteral("hoverBottom"), QStringLiteral("#cfe8fb")},
        {QStringLiteral("hoverBorder"), QStringLiteral("#7fb2d9")},
        {QStringLiteral("pressedTop"), QStringLiteral("#c4e5f6")},
        {QStringLiteral("pressedBottom"), QStringLiteral("#98d1ef")},
        {QStringLiteral("pressedBorder"), QStringLiteral("#5a8fb8")},
        {QStringLiteral("disabledText"), QStringLiteral("#a0a0a0")},
        {QStringLiteral("disabledBg"), QStringLiteral("#f0f0f0")},
        {QStringLiteral("disabledBorder"), QStringLiteral("#c8c8c8")},
        {QStringLiteral("tooltipBg"), QStringLiteral("#ffffff")},
        {QStringLiteral("tooltipBorder"), QStringLiteral("#a0a0a0")},
        {QStringLiteral("toolbarTop"), QStringLiteral("#f4fbff")},
        {QStringLiteral("toolbarMid"), QStringLiteral("#e3f2fd")},
        {QStringLiteral("toolbarMid2"), QStringLiteral("#d0e7f9")},
        {QStringLiteral("toolbarBottom"), QStringLiteral("#bcd9f0")},
        {QStringLiteral("headerTop"), QStringLiteral("#f8fcff")},
        {QStringLiteral("headerMid"), QStringLiteral("#e6f2fc")},
        {QStringLiteral("headerBottom"), QStringLiteral("#d2e6f7")},
        {QStringLiteral("headerSeparator"), QStringLiteral("#b8c9da")},
        {QStringLiteral("separator"), QStringLiteral("#b8c9da")},
        {QStringLiteral("buttonTop"), QStringLiteral("#fdfdfd")},
        {QStringLiteral("buttonMid"), QStringLiteral("#f4f4f4")},
        {QStringLiteral("buttonMid2"), QStringLiteral("#e8e8e8")},
        {QStringLiteral("buttonBottom"), QStringLiteral("#d4d4d4")},
        {QStringLiteral("buttonBorder"), QStringLiteral("#8e8e8e")},
        {QStringLiteral("buttonHoverTop"), QStringLiteral("#f3faff")},
        {QStringLiteral("buttonHoverBottom"), QStringLiteral("#d7ecfd")},
        {QStringLiteral("buttonPressedTop"), QStringLiteral("#c4e5f6")},
        {QStringLiteral("buttonPressedBottom"), QStringLiteral("#98d1ef")},
        {QStringLiteral("menuBg"), QStringLiteral("#ffffff")},
        {QStringLiteral("menuBorder"), QStringLiteral("#a0a0a0")},
        {QStringLiteral("menuHoverTop"), QStringLiteral("#eaf6ff")},
        {QStringLiteral("menuHoverBottom"), QStringLiteral("#cfe8fb")},
        {QStringLiteral("menuHoverBorder"), QStringLiteral("#7fb2d9")},
        {QStringLiteral("menuSeparator"), QStringLiteral("#d0d0d0")},
        {QStringLiteral("scrollTrack"), QStringLiteral("#eef3f8")},
        {QStringLiteral("scrollHandleTop"), QStringLiteral("#f4f9ff")},
        {QStringLiteral("scrollHandleMid"), QStringLiteral("#d7e8f7")},
        {QStringLiteral("scrollHandleBottom"), QStringLiteral("#b7d4ec")},
        {QStringLiteral("scrollHandleBorder"), QStringLiteral("#8fb4d1")},
        {QStringLiteral("scrollButtonTop"), QStringLiteral("#f7fbff")},
        {QStringLiteral("scrollButtonBottom"), QStringLiteral("#dce9f5")},
        {QStringLiteral("progressBg"), QStringLiteral("#e8e8e8")},
        {QStringLiteral("progressTop"), QStringLiteral("#b6f0a8")},
        {QStringLiteral("progressMid"), QStringLiteral("#5fd34a")},
        {QStringLiteral("progressMid2"), QStringLiteral("#2fbf20")},
        {QStringLiteral("progressBottom"), QStringLiteral("#1a9e12")},
        {QStringLiteral("progressBorder"), QStringLiteral("#1a7f12")},
        {QStringLiteral("statusTop"), QStringLiteral("#f4fbff")},
        {QStringLiteral("statusBottom"), QStringLiteral("#dce9f5")},
        {QStringLiteral("dockTitleTop"), QStringLiteral("#f4fbff")},
        {QStringLiteral("dockTitleBottom"), QStringLiteral("#d8e9f8")},
        {QStringLiteral("listHover"), QStringLiteral("#f2f9ff")},
        {QStringLiteral("listSelectedTop"), QStringLiteral("#dceeff")},
        {QStringLiteral("listSelectedBottom"), QStringLiteral("#b9dcf7")},
        {QStringLiteral("listSelectedBorder"), QStringLiteral("#7fb2d9")},
        {QStringLiteral("listInactiveTop"), QStringLiteral("#e8e8e8")},
        {QStringLiteral("listInactiveBottom"), QStringLiteral("#d8d8d8")},
        {QStringLiteral("footerBorder"), QStringLiteral("#d0d0d0")}
    };
    addAssetTokens(tokens, QString());
    return tokens;
}

QHash<QString, QString> darkTokens()
{
    QHash<QString, QString> tokens = {
        {QStringLiteral("window"), QStringLiteral("#1e1e1e")},
        {QStringLiteral("text"), QStringLiteral("#e6e6e6")},
        {QStringLiteral("mutedText"), QStringLiteral("#a0a0a0")},
        {QStringLiteral("base"), QStringLiteral("#252526")},
        {QStringLiteral("alternateBase"), QStringLiteral("#2d2d30")},
        {QStringLiteral("border"), QStringLiteral("#3f4b57")},
        {QStringLiteral("borderLight"), QStringLiteral("#333a42")},
        {QStringLiteral("inputBorder"), QStringLiteral("#5a6673")},
        {QStringLiteral("focusBorder"), QStringLiteral("#2a82da")},
        {QStringLiteral("accent"), QStringLiteral("#2a82da")},
        {QStringLiteral("accentDark"), QStringLiteral("#1f6fb2")},
        {QStringLiteral("selection"), QStringLiteral("#2a82da")},
        {QStringLiteral("selectionText"), QStringLiteral("#ffffff")},
        {QStringLiteral("hoverTop"), QStringLiteral("#2f3d4b")},
        {QStringLiteral("hoverBottom"), QStringLiteral("#243443")},
        {QStringLiteral("hoverBorder"), QStringLiteral("#2a82da")},
        {QStringLiteral("pressedTop"), QStringLiteral("#1f3346")},
        {QStringLiteral("pressedBottom"), QStringLiteral("#172838")},
        {QStringLiteral("pressedBorder"), QStringLiteral("#1f6fb2")},
        {QStringLiteral("disabledText"), QStringLiteral("#6f6f6f")},
        {QStringLiteral("disabledBg"), QStringLiteral("#2a2a2a")},
        {QStringLiteral("disabledBorder"), QStringLiteral("#3a3a3a")},
        {QStringLiteral("tooltipBg"), QStringLiteral("#2d2d30")},
        {QStringLiteral("tooltipBorder"), QStringLiteral("#4a4a4a")},
        {QStringLiteral("toolbarTop"), QStringLiteral("#2a3540")},
        {QStringLiteral("toolbarMid"), QStringLiteral("#222c36")},
        {QStringLiteral("toolbarMid2"), QStringLiteral("#1d2630")},
        {QStringLiteral("toolbarBottom"), QStringLiteral("#18202a")},
        {QStringLiteral("headerTop"), QStringLiteral("#2a3540")},
        {QStringLiteral("headerMid"), QStringLiteral("#222c36")},
        {QStringLiteral("headerBottom"), QStringLiteral("#1b242d")},
        {QStringLiteral("headerSeparator"), QStringLiteral("#3f4b57")},
        {QStringLiteral("separator"), QStringLiteral("#3f4b57")},
        {QStringLiteral("buttonTop"), QStringLiteral("#3a3a3a")},
        {QStringLiteral("buttonMid"), QStringLiteral("#333333")},
        {QStringLiteral("buttonMid2"), QStringLiteral("#2d2d2d")},
        {QStringLiteral("buttonBottom"), QStringLiteral("#252525")},
        {QStringLiteral("buttonBorder"), QStringLiteral("#5a5a5a")},
        {QStringLiteral("buttonHoverTop"), QStringLiteral("#3b4f63")},
        {QStringLiteral("buttonHoverBottom"), QStringLiteral("#25384a")},
        {QStringLiteral("buttonPressedTop"), QStringLiteral("#1f3346")},
        {QStringLiteral("buttonPressedBottom"), QStringLiteral("#172838")},
        {QStringLiteral("menuBg"), QStringLiteral("#2d2d30")},
        {QStringLiteral("menuBorder"), QStringLiteral("#4a4a4a")},
        {QStringLiteral("menuHoverTop"), QStringLiteral("#2f3d4b")},
        {QStringLiteral("menuHoverBottom"), QStringLiteral("#243443")},
        {QStringLiteral("menuHoverBorder"), QStringLiteral("#2a82da")},
        {QStringLiteral("menuSeparator"), QStringLiteral("#4a4a4a")},
        {QStringLiteral("scrollTrack"), QStringLiteral("#1b1b1b")},
        {QStringLiteral("scrollHandleTop"), QStringLiteral("#4a5a6a")},
        {QStringLiteral("scrollHandleMid"), QStringLiteral("#3a4a5a")},
        {QStringLiteral("scrollHandleBottom"), QStringLiteral("#2a3a4a")},
        {QStringLiteral("scrollHandleBorder"), QStringLiteral("#5a6a7a")},
        {QStringLiteral("scrollButtonTop"), QStringLiteral("#2a3540")},
        {QStringLiteral("scrollButtonBottom"), QStringLiteral("#1d2630")},
        {QStringLiteral("progressBg"), QStringLiteral("#1b1b1b")},
        {QStringLiteral("progressTop"), QStringLiteral("#4ecf3a")},
        {QStringLiteral("progressMid"), QStringLiteral("#2fbf20")},
        {QStringLiteral("progressMid2"), QStringLiteral("#1f9e12")},
        {QStringLiteral("progressBottom"), QStringLiteral("#167a0c")},
        {QStringLiteral("progressBorder"), QStringLiteral("#0f5a08")},
        {QStringLiteral("statusTop"), QStringLiteral("#2a3540")},
        {QStringLiteral("statusBottom"), QStringLiteral("#18202a")},
        {QStringLiteral("dockTitleTop"), QStringLiteral("#2a3540")},
        {QStringLiteral("dockTitleBottom"), QStringLiteral("#1b242d")},
        {QStringLiteral("listHover"), QStringLiteral("#2f3d4b")},
        {QStringLiteral("listSelectedTop"), QStringLiteral("#1f4a6e")},
        {QStringLiteral("listSelectedBottom"), QStringLiteral("#173a57")},
        {QStringLiteral("listSelectedBorder"), QStringLiteral("#2a82da")},
        {QStringLiteral("listInactiveTop"), QStringLiteral("#333333")},
        {QStringLiteral("listInactiveBottom"), QStringLiteral("#2a2a2a")},
        {QStringLiteral("footerBorder"), QStringLiteral("#3f4b57")}
    };
    addAssetTokens(tokens, QStringLiteral("-dark"));
    return tokens;
}

// ---------------------------------------------------------------------------
// Base style
// ---------------------------------------------------------------------------

// Fusion is the most predictable base for a complete Qt style sheet: it does
// not let GTK/Breeze/KDE draw its own controls underneath our theme. The proxy
// also forces the Windows button order in every QDialogButtonBox.
// Fusion 是完整 Qt 样式表最可预测的基础样式：
// 它不会让 GTK/Breeze/KDE 在我们的主题下方再绘制一套原生控件。
// 代理样式还强制所有 QDialogButtonBox 使用 Windows 按钮顺序。
class AeroProxyStyle : public QProxyStyle {
public:
    AeroProxyStyle()
        : QProxyStyle(QStringLiteral("Fusion"))
    {
    }

    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == SH_DialogButtonLayout) {
            return QDialogButtonBox::WinLayout;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

void applyBaseStyle(QApplication &app)
{
    if (!dynamic_cast<AeroProxyStyle *>(app.style())) {
        app.setStyle(new AeroProxyStyle);
    }
}

void applyFont(QApplication &app)
{
    static const QStringList preferredFamilies = {
        QStringLiteral("Segoe UI"),
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Noto Sans"),
        QStringLiteral("DejaVu Sans")
    };

    const QStringList available = QFontDatabase::families();
    QFont font = app.font();
    for (const QString &family : preferredFamilies) {
        if (available.contains(family, Qt::CaseInsensitive)) {
            font.setFamily(family);
            break;
        }
    }
    // Windows 7 uses 9 pt Segoe UI for standard dialogs.
    // Windows 7 标准对话框使用 9 pt Segoe UI。
    font.setPointSizeF(9.0);
    app.setFont(font);
}

QString loadAeroStyleSheet(bool dark)
{
    QFile file(QStringLiteral(":/styles/aero.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // The test binaries link ThemeManager without the application
        // resources; an empty style sheet is the expected fallback there.
        // 测试程序链接了 ThemeManager 但没有应用资源；
        // 在那里空样式表是预期的回退结果。
        LOG_DEBUG_S("ThemeManager", "Aero style sheet resource is not available");
        return {};
    }

    QString qss = QString::fromUtf8(file.readAll());
    const QHash<QString, QString> tokens = dark ? darkTokens() : lightTokens();
    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it) {
        qss.replace(QStringLiteral("@%1@").arg(it.key()), it.value());
    }

    // Catch typos in development builds instead of silently dropping rules.
    // 在开发构建中捕获拼写错误，避免规则被静默丢弃。
    static const QRegularExpression unresolvedToken(
        QStringLiteral("@[A-Za-z0-9_]+@"));
    const QRegularExpressionMatch match = unresolvedToken.match(qss);
    if (match.hasMatch()) {
        LOG_WARN_S("ThemeManager",
                   QStringLiteral("Unresolved style sheet token: %1")
                       .arg(match.captured(0)));
    }
    return qss;
}

} // namespace

QString ThemeManager::systemColorScheme()
{
    // Prefer Qt's platform hint.  It does not perform a blocking DBus call
    // and reflects the same desktop preference on modern Qt versions.
    // Prior to Qt 6.5 use the palette lightness, which is also available
    // without touching the session bus during startup.
    // 优先使用 Qt 的平台提示。它不会执行阻塞式 DBus 调用，在较新的 Qt 上
    // 反映相同的桌面偏好。Qt 6.5 之前使用调色板明度，同样无需在启动期间
    // 访问会话总线。
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints()) {
        const Qt::ColorScheme scheme = hints->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            return QStringLiteral("dark");
        }
        if (scheme == Qt::ColorScheme::Light) {
            return QStringLiteral("light");
        }
    }
#endif
    const QPalette palette = QGuiApplication::palette();
    return palette.color(QPalette::Window).lightness() < 128
        ? QStringLiteral("dark")
        : QStringLiteral("light");
}

QString ThemeManager::effectiveTheme(const QString &configuredTheme)
{
    const QString t = configuredTheme.trimmed().toLower();
    if (t == QStringLiteral("dark")) {
        return QStringLiteral("dark");
    }
    if (t == QStringLiteral("light")) {
        return QStringLiteral("light");
    }
    // "system" (or any unknown value): follow the desktop color-scheme.
    // "system"（或任何未知值）：跟随桌面配色方案。
    return systemColorScheme();
}

void ThemeManager::applyTheme(QApplication &app)
{
    const QString theme = effectiveTheme(AppSettings::instance().theme());
    const bool dark = (theme == QStringLiteral("dark"));

    // Install the base style first, then the font, palette and style sheet.
    // The style may reset palette/font state, so the order matters.
    // 先安装基础样式，再设置字体、调色板和样式表。
    // 基础样式可能重置调色板/字体状态，因此顺序很重要。
    applyBaseStyle(app);
    applyFont(app);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // setColorScheme() was added in Qt 6.5; older versions follow the palette.
    // setColorScheme() 自 Qt 6.5 起提供；更早版本直接使用调色板。
    if (auto *hints = QGuiApplication::styleHints()) {
        hints->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    }
#endif
    app.setPalette(buildPalette(dark));

    // Expose the effective theme to custom-painted widgets (the file list
    // delegate and any future marquee progress bar).
    // 将实际主题暴露给自绘控件（文件列表 delegate 和未来的跑马灯进度条）。
    app.setProperty("aeroDark", dark);
    app.setProperty("aeroAccent",
                    QColor(dark ? QStringLiteral("#2a82da")
                                : QStringLiteral("#3399ff")));
    app.setStyleSheet(loadAeroStyleSheet(dark));

    LOG_INFO_S("ThemeManager", dark ? "Applied Aero dark theme"
                                    : "Applied Aero light theme");
}
