// QArchiver / 小Q解压
// ThemeManager implementation: Aero palettes, fonts and the Qt style sheet.
// ThemeManager 实现：Aero 调色板、字体与 Qt 样式表。

#include "ui/icons/IconUtils.h"
#include "ui/theme/ThemeManager.h"

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

// Style sheets are loaded from the embedded resource bundle.  Keep the paths in
// one place so a resource reorganization only touches these constants.
// 样式表从内嵌资源包加载。把路径集中在此，资源重组时只需修改这两个常量。
constexpr const char *kAeroStyleSheet = ":/themes/aero/aero.qss";
constexpr const char *kModernStyleSheet = ":/themes/modern/modern.qss";

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
// Modern palette
// ---------------------------------------------------------------------------

QPalette buildModernPalette(bool dark)
{
    QPalette p;

    if (dark) {
        const QColor window(23, 27, 34);
        const QColor windowText(230, 235, 242);
        const QColor base(31, 36, 45);
        const QColor alternateBase(35, 42, 52);
        const QColor text(230, 235, 242);
        const QColor button(36, 43, 53);
        const QColor buttonText(230, 235, 242);
        const QColor highlight(52, 90, 128);
        const QColor highlightedText(234, 243, 255);
        const QColor placeholder(122, 135, 150);

        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, windowText);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternateBase);
        p.setColor(QPalette::ToolTipBase, QColor(36, 43, 53));
        p.setColor(QPalette::ToolTipText, windowText);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, buttonText);
        p.setColor(QPalette::BrightText, QColor(255, 255, 255));
        p.setColor(QPalette::Link, QColor(108, 186, 255));
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, highlightedText);
        p.setColor(QPalette::PlaceholderText, placeholder);
        p.setColor(QPalette::Light, base.lighter(150));
        p.setColor(QPalette::Midlight, base.lighter(125));
        p.setColor(QPalette::Mid, base.darker(125));
        p.setColor(QPalette::Dark, base.darker(160));
        p.setColor(QPalette::Shadow, base.darker(200));

        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(101, 113, 127));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(101, 113, 127));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(101, 113, 127));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(45, 56, 70));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(120, 132, 146));
    } else {
        const QColor window(243, 246, 251);
        const QColor windowText(30, 42, 57);
        const QColor base(255, 255, 255);
        const QColor alternateBase(248, 250, 253);
        const QColor text(30, 42, 57);
        const QColor button(255, 255, 255);
        const QColor buttonText(30, 42, 57);
        const QColor highlight(220, 235, 255);
        const QColor highlightedText(30, 42, 57);
        const QColor placeholder(152, 162, 179);

        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, windowText);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternateBase);
        p.setColor(QPalette::ToolTipBase, base);
        p.setColor(QPalette::ToolTipText, windowText);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, buttonText);
        p.setColor(QPalette::BrightText, QColor(255, 255, 255));
        p.setColor(QPalette::Link, QColor(27, 126, 255));
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, highlightedText);
        p.setColor(QPalette::PlaceholderText, placeholder);
        p.setColor(QPalette::Light, base.lighter(150));
        p.setColor(QPalette::Midlight, base.lighter(125));
        p.setColor(QPalette::Mid, base.darker(125));
        p.setColor(QPalette::Dark, base.darker(160));
        p.setColor(QPalette::Shadow, base.darker(200));

        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(168, 179, 192));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(168, 179, 192));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(168, 179, 192));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(236, 241, 247));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(168, 179, 192));
    }

    return p;
}

// ---------------------------------------------------------------------------
// Aero style sheet colors
// ---------------------------------------------------------------------------

void addAssetTokens(QHash<QString, QString> &tokens, bool dark)
{
    // The classic theme ships the same glyph names in light/ and dark/.
    // 经典主题在 light/ 与 dark/ 下提供同名图形。
    const QString root = QString::fromLatin1(Icons::aeroGlyphRoot())
        + QLatin1String(dark ? "dark/" : "light/");
    const auto add = [&tokens, &root](const char *key, const char *file) {
        tokens.insert(QString::fromLatin1(key),
                      root + QStringLiteral("%1.png").arg(QString::fromLatin1(file)));
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
    addAssetTokens(tokens, false);
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
    addAssetTokens(tokens, true);
    return tokens;
}

// ---------------------------------------------------------------------------
// Modern theme tokens
// ---------------------------------------------------------------------------

QHash<QString, QString> modernTokens(bool dark)
{
    QHash<QString, QString> tokens;
    // The modern theme ships the same glyph names in light/ and dark/.
    // 现代主题在 light/ 与 dark/ 下提供同名图形。
    const QString glyphRoot = QString::fromLatin1(Icons::modernGlyphRoot())
        + QLatin1String(dark ? "dark/" : "light/");

    const auto addAsset = [&tokens, &glyphRoot](const char *key, const char *file) {
        tokens.insert(QString::fromLatin1(key),
                      glyphRoot + QStringLiteral("%1.png").arg(QString::fromLatin1(file)));
    };
    addAsset("branchClosed", "branch-closed");
    addAsset("branchOpen", "branch-open");
    addAsset("chevronDown", "arrow-down");
    addAsset("chevronUp", "arrow-up");
    addAsset("close", "close");
    addAsset("sizeGrip", "sizegrip");
    addAsset("sortUp", "sort-up");
    addAsset("sortDown", "sort-down");

    // These glyphs live at the theme root: they are identical in both themes
    // and have no light/dark variant.
    // 这些字形位于主题根目录：深浅主题共用，没有 light/dark 变体。
    const QString neutralRoot = QString::fromLatin1(Icons::modernGlyphRoot());
    const auto addFixed = [&tokens, &neutralRoot](const char *key, const char *file) {
        tokens.insert(QString::fromLatin1(key),
                      neutralRoot + QStringLiteral("%1.png").arg(QString::fromLatin1(file)));
    };
    addFixed("check", "check");
    addFixed("minus", "minus");
    addFixed("radioDot", "radio-dot");

    if (dark) {
        tokens.insert(QStringLiteral("window"), QStringLiteral("#171b22"));
        tokens.insert(QStringLiteral("text"), QStringLiteral("#e6ebf2"));
        tokens.insert(QStringLiteral("mutedText"), QStringLiteral("#9aa8b8"));
        tokens.insert(QStringLiteral("base"), QStringLiteral("#1f242d"));
        tokens.insert(QStringLiteral("alternateBase"), QStringLiteral("#232a34"));
        tokens.insert(QStringLiteral("border"), QStringLiteral("#3b4858"));
        tokens.insert(QStringLiteral("borderLight"), QStringLiteral("#303a46"));
        tokens.insert(QStringLiteral("inputBorder"), QStringLiteral("#3d4856"));
        tokens.insert(QStringLiteral("focusBorder"), QStringLiteral("#4aa6ff"));
        tokens.insert(QStringLiteral("accent"), QStringLiteral("#4aa6ff"));
        tokens.insert(QStringLiteral("accentHover"), QStringLiteral("#6cbaff"));
        tokens.insert(QStringLiteral("accentDark"), QStringLiteral("#6cbaff"));
        tokens.insert(QStringLiteral("accentSoft"), QStringLiteral("#223449"));
        tokens.insert(QStringLiteral("selection"), QStringLiteral("#24425f"));
        tokens.insert(QStringLiteral("selectionText"), QStringLiteral("#eaf3ff"));
        tokens.insert(QStringLiteral("hover"), QStringLiteral("#26313e"));
        tokens.insert(QStringLiteral("pressed"), QStringLiteral("#2d3b4c"));
        tokens.insert(QStringLiteral("disabledText"), QStringLiteral("#65717f"));
        tokens.insert(QStringLiteral("disabledBg"), QStringLiteral("#232830"));
        tokens.insert(QStringLiteral("headerBg"), QStringLiteral("#232b35"));
        tokens.insert(QStringLiteral("headerText"), QStringLiteral("#9aa8b8"));
        tokens.insert(QStringLiteral("separator"), QStringLiteral("#303a46"));
        tokens.insert(QStringLiteral("headerTop"), QStringLiteral("#223447"));
        tokens.insert(QStringLiteral("headerMid"), QStringLiteral("#1d252f"));
        tokens.insert(QStringLiteral("headerBottom"), QStringLiteral("#1a212a"));
        tokens.insert(QStringLiteral("brand"), QStringLiteral("#6cbaff"));
        tokens.insert(QStringLiteral("brandTag"), QStringLiteral("#8fa3b8"));
        tokens.insert(QStringLiteral("toolbarText"), QStringLiteral("#d7e0ea"));
        tokens.insert(QStringLiteral("toolbarHover"), QStringLiteral("#26384c"));
        tokens.insert(QStringLiteral("toolbarHoverBorder"), QStringLiteral("#354b62"));
        tokens.insert(QStringLiteral("toolbarPressed"), QStringLiteral("#2c435c"));
        tokens.insert(QStringLiteral("toolbarPressedBorder"), QStringLiteral("#4a6a8a"));
        tokens.insert(QStringLiteral("menuBg"), QStringLiteral("#242b35"));
        tokens.insert(QStringLiteral("menuBorder"), QStringLiteral("#3a4552"));
        tokens.insert(QStringLiteral("menuHover"), QStringLiteral("#2b3948"));
        tokens.insert(QStringLiteral("scrollHandle"), QStringLiteral("#4a5868"));
        tokens.insert(QStringLiteral("scrollHandleHover"), QStringLiteral("#63758a"));
        tokens.insert(QStringLiteral("progressBg"), QStringLiteral("#222a34"));
        tokens.insert(QStringLiteral("progressTop"), QStringLiteral("#4aa6ff"));
        tokens.insert(QStringLiteral("progressBottom"), QStringLiteral("#2f8cf6"));
        tokens.insert(QStringLiteral("statusBg"), QStringLiteral("#151b22"));
        tokens.insert(QStringLiteral("statusText"), QStringLiteral("#9aa8b8"));
        tokens.insert(QStringLiteral("rowSelected"), QStringLiteral("#24425f"));
        tokens.insert(QStringLiteral("rowSelectedBorder"), QStringLiteral("#3b6f9e"));
        tokens.insert(QStringLiteral("rowHover"), QStringLiteral("#26313e"));
        tokens.insert(QStringLiteral("rowHoverBorder"), QStringLiteral("#33404e"));
        tokens.insert(QStringLiteral("detailsBg"), QStringLiteral("#171d25"));
        tokens.insert(QStringLiteral("welcomeBg"), QStringLiteral("#1b2330"));
        tokens.insert(QStringLiteral("welcomeBorder"), QStringLiteral("#31465e"));
        tokens.insert(QStringLiteral("buttonBg"), QStringLiteral("#242b35"));
        tokens.insert(QStringLiteral("tooltipBg"), QStringLiteral("#2d2d30"));
        tokens.insert(QStringLiteral("outputBg"), QStringLiteral("#14181f"));
        tokens.insert(QStringLiteral("outputText"), QStringLiteral("#c6d3e1"));
    } else {
        tokens.insert(QStringLiteral("window"), QStringLiteral("#e9eef6"));
        tokens.insert(QStringLiteral("text"), QStringLiteral("#1e2a39"));
        tokens.insert(QStringLiteral("mutedText"), QStringLiteral("#7a8796"));
        tokens.insert(QStringLiteral("base"), QStringLiteral("#ffffff"));
        tokens.insert(QStringLiteral("alternateBase"), QStringLiteral("#f6f9fd"));
        tokens.insert(QStringLiteral("border"), QStringLiteral("#cfdbe8"));
        tokens.insert(QStringLiteral("borderLight"), QStringLiteral("#e2eaf3"));
        tokens.insert(QStringLiteral("inputBorder"), QStringLiteral("#c6d4e3"));
        tokens.insert(QStringLiteral("focusBorder"), QStringLiteral("#2f8cf6"));
        tokens.insert(QStringLiteral("accent"), QStringLiteral("#2f8cf6"));
        tokens.insert(QStringLiteral("accentHover"), QStringLiteral("#4aa6ff"));
        tokens.insert(QStringLiteral("accentDark"), QStringLiteral("#1b6fd6"));
        tokens.insert(QStringLiteral("accentSoft"), QStringLiteral("#e8f3ff"));
        tokens.insert(QStringLiteral("selection"), QStringLiteral("#dcebff"));
        tokens.insert(QStringLiteral("selectionText"), QStringLiteral("#1e2a39"));
        tokens.insert(QStringLiteral("hover"), QStringLiteral("#f2f7ff"));
        tokens.insert(QStringLiteral("pressed"), QStringLiteral("#e3f0ff"));
        tokens.insert(QStringLiteral("disabledText"), QStringLiteral("#a8b3c0"));
        tokens.insert(QStringLiteral("disabledBg"), QStringLiteral("#f2f5f8"));
        tokens.insert(QStringLiteral("headerBg"), QStringLiteral("#edf3fa"));
        tokens.insert(QStringLiteral("headerText"), QStringLiteral("#5c6e82"));
        tokens.insert(QStringLiteral("separator"), QStringLiteral("#d9e3ee"));
        tokens.insert(QStringLiteral("headerTop"), QStringLiteral("#e1efff"));
        tokens.insert(QStringLiteral("headerMid"), QStringLiteral("#f2f8ff"));
        tokens.insert(QStringLiteral("headerBottom"), QStringLiteral("#fbfdff"));
        tokens.insert(QStringLiteral("brand"), QStringLiteral("#1b7eff"));
        tokens.insert(QStringLiteral("brandTag"), QStringLiteral("#8fa3b8"));
        tokens.insert(QStringLiteral("toolbarText"), QStringLiteral("#2b3a4a"));
        tokens.insert(QStringLiteral("toolbarHover"), QStringLiteral("#e8f3ff"));
        tokens.insert(QStringLiteral("toolbarHoverBorder"), QStringLiteral("#cfe4fb"));
        tokens.insert(QStringLiteral("toolbarPressed"), QStringLiteral("#d8eaff"));
        tokens.insert(QStringLiteral("toolbarPressedBorder"), QStringLiteral("#b5d6f8"));
        tokens.insert(QStringLiteral("menuBg"), QStringLiteral("#ffffff"));
        tokens.insert(QStringLiteral("menuBorder"), QStringLiteral("#e3e8ef"));
        tokens.insert(QStringLiteral("menuHover"), QStringLiteral("#eef6ff"));
        tokens.insert(QStringLiteral("scrollHandle"), QStringLiteral("#c9d5e2"));
        tokens.insert(QStringLiteral("scrollHandleHover"), QStringLiteral("#aabacb"));
        tokens.insert(QStringLiteral("progressBg"), QStringLiteral("#eef2f7"));
        tokens.insert(QStringLiteral("progressTop"), QStringLiteral("#62b6ff"));
        tokens.insert(QStringLiteral("progressBottom"), QStringLiteral("#2f8cf6"));
        tokens.insert(QStringLiteral("statusBg"), QStringLiteral("#eaf0f7"));
        tokens.insert(QStringLiteral("statusText"), QStringLiteral("#7a8796"));
        tokens.insert(QStringLiteral("rowSelected"), QStringLiteral("#dcebff"));
        tokens.insert(QStringLiteral("rowSelectedBorder"), QStringLiteral("#b9d7fb"));
        tokens.insert(QStringLiteral("rowHover"), QStringLiteral("#f2f7ff"));
        tokens.insert(QStringLiteral("rowHoverBorder"), QStringLiteral("#e3edf8"));
        tokens.insert(QStringLiteral("detailsBg"), QStringLiteral("#ecf2f9"));
        tokens.insert(QStringLiteral("welcomeBg"), QStringLiteral("#f3f9ff"));
        tokens.insert(QStringLiteral("welcomeBorder"), QStringLiteral("#bfd5ea"));
        tokens.insert(QStringLiteral("buttonBg"), QStringLiteral("#ffffff"));
        tokens.insert(QStringLiteral("tooltipBg"), QStringLiteral("#ffffff"));
        tokens.insert(QStringLiteral("outputBg"), QStringLiteral("#fbfcfe"));
        tokens.insert(QStringLiteral("outputText"), QStringLiteral("#3b4a5a"));
    }
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

QString loadStyleSheet(bool dark, bool modern)
{
    QFile file(modern ? QString::fromLatin1(kModernStyleSheet)
                      : QString::fromLatin1(kAeroStyleSheet));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // The test binaries link ThemeManager without the application
        // resources; an empty style sheet is the expected fallback there.
        // 测试程序链接了 ThemeManager 但没有应用资源；
        // 在那里空样式表是预期的回退结果。
        LOG_DEBUG_S("ThemeManager", "Theme style sheet resource is not available");
        return {};
    }

    QString qss = QString::fromUtf8(file.readAll());
    const QHash<QString, QString> tokens = modern
        ? modernTokens(dark)
        : (dark ? darkTokens() : lightTokens());
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
    const bool modern = AppSettings::instance().uiLayoutMode()
        == QStringLiteral("modern");

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
    app.setPalette(modern ? buildModernPalette(dark) : buildPalette(dark));

    // Expose the effective theme to custom-painted widgets (the file list
    // delegate, row colors and the modern/classic distinction).
    // 将实际主题暴露给自绘控件（文件列表 delegate、行配色以及现代/经典布局）。
    app.setProperty("aeroDark", dark);
    app.setProperty("modernUi", modern);
    app.setProperty("aeroAccent",
                    QColor(dark ? QStringLiteral("#2a82da")
                                : (modern ? QStringLiteral("#2f8cf6")
                                          : QStringLiteral("#3399ff"))));
    app.setStyleSheet(loadStyleSheet(dark, modern));

    LOG_INFO("ThemeManager", "Applied %s %s theme",
             modern ? "modern" : "Aero", dark ? "dark" : "light");
}

bool ThemeManager::isModernLayout()
{
    const QVariant value = QCoreApplication::instance()->property("modernUi");
    return value.isValid() ? value.toBool() : false;
}

bool ThemeManager::isDarkTheme()
{
    const QVariant value = QCoreApplication::instance()->property("aeroDark");
    return value.isValid() ? value.toBool() : false;
}
