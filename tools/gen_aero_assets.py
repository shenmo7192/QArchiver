#!/usr/bin/env python3
"""Generate the small Aero UI glyphs used by resources/themes/aero/aero.qss.

The generated PNGs are committed to the repository, so this script is only
needed when the glyphs or their colors change. It uses PyQt6 because the
project already targets Qt; run it from the repository root:

    QT_QPA_PLATFORM=offscreen python3 tools/gen_aero_assets.py
"""

from __future__ import annotations

import os
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt6.QtCore import QPointF, QRectF, Qt  # noqa: E402
from PyQt6.QtGui import (  # noqa: E402
    QColor,
    QGuiApplication,
    QImage,
    QLinearGradient,
    QPainter,
    QPen,
    QPolygonF,
)

# Classic (Aero) glyphs are written into the theme's light/ and dark/ folders.
# The style sheet lives in the same theme folder and refers to these files via
# the /themes/aero resource prefix.
# 经典（Aero）字形写入主题的 light/ 与 dark/ 目录；样式表位于同一主题目录，
# 并通过 /themes/aero 资源前缀引用这些文件。
#
# Files are written to one of three destinations:
#   THEME_DIR/          glyphs shared by both themes (Win7 checkbox/radio art)
#   THEME_DIR/light/    glyphs for light backgrounds
#   THEME_DIR/dark/     the same glyph names, recolored for dark backgrounds
# 文件写入三个目标之一：主题根目录（深浅共用）、light/（浅色背景）、
# dark/（同名重着色版本，供深色背景）。
THEME_DIR = Path(__file__).resolve().parent.parent / "resources" / "themes" / "aero"
OUT_DIR = THEME_DIR / "light"
DARK_DIR = THEME_DIR / "dark"
SHARED_DIR = THEME_DIR

# Windows 7 Aero-ish control colors.
WHITE = QColor("#ffffff")
BOX_TOP = QColor("#fdfdfd")
BOX_BOTTOM = QColor("#e4e4e4")
BOX_HOVER_TOP = QColor("#f3faff")
BOX_HOVER_BOTTOM = QColor("#d7ecfd")
BOX_BORDER = QColor("#8e9eae")
BOX_HOVER_BORDER = QColor("#3c7fb1")
BOX_DISABLED_TOP = QColor("#f2f2f2")
BOX_DISABLED_BOTTOM = QColor("#e2e2e2")
BOX_DISABLED_BORDER = QColor("#c0c0c0")
CHECK_BLUE = QColor("#1a5fb4")
CHECK_BLUE_LIGHT = QColor("#5fb3ff")
DISABLED_MARK = QColor("#a8a8a8")
ARROW = QColor("#3a3a3a")
ARROW_DISABLED = QColor("#b0b0b0")
TREE_ARROW = QColor("#4a4a4a")
SEARCH = QColor("#7f8fa0")
DOCK_GLYPH = QColor("#4a4a4a")
GRIP = QColor("#8a8a8a")
DARK_GLYPH = QColor("#c8d8e8")
DARK_GLYPH_DISABLED = QColor("#5b6875")


def image(size: int) -> tuple[QImage, QPainter]:
    img = QImage(size, size, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(Qt.GlobalColor.transparent)
    painter = QPainter(img)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
    return img, painter


def save(img: QImage, name: str, target: str = "light") -> None:
    directory = {"light": OUT_DIR, "dark": DARK_DIR, "shared": SHARED_DIR}[target]
    path = directory / name
    path.parent.mkdir(parents=True, exist_ok=True)
    if not img.save(str(path), "PNG"):
        raise RuntimeError(f"failed to save {path}")
    print(f"wrote {path.relative_to(THEME_DIR.parent.parent.parent)}")


def recolor(src_name: str, dst_name: str, color: QColor) -> None:
    """Create the dark-theme variant of a monochrome glyph (dark/ folder)."""
    img = QImage(str(OUT_DIR / src_name))
    if img.isNull():
        raise RuntimeError(f"missing source glyph: {src_name}")
    img = img.convertToFormat(QImage.Format.Format_ARGB32)
    for y in range(img.height()):
        for x in range(img.width()):
            alpha = img.pixelColor(x, y).alpha()
            if alpha == 0:
                continue
            tinted = QColor(color)
            tinted.setAlpha(alpha)
            img.setPixelColor(x, y, tinted)
    save(img, dst_name, "dark")


def box_gradient(rect: QRectF, top: QColor, bottom: QColor) -> QLinearGradient:
    grad = QLinearGradient(rect.topLeft(), rect.bottomLeft())
    grad.setColorAt(0.0, top)
    grad.setColorAt(1.0, bottom)
    return grad


def draw_box(
    painter: QPainter,
    rect: QRectF,
    top: QColor,
    bottom: QColor,
    border: QColor,
    radius: float,
) -> None:
    painter.setPen(QPen(border, 1.0))
    painter.setBrush(box_gradient(rect, top, bottom))
    painter.drawRoundedRect(rect, radius, radius)
    # Glass highlight along the top edge.
    painter.setPen(QPen(QColor(255, 255, 255, 190), 1.0))
    painter.drawLine(
        QPointF(rect.left() + 2.0, rect.top() + 1.5),
        QPointF(rect.right() - 2.0, rect.top() + 1.5),
    )


def draw_checkbox(variant: str) -> None:
    img, p = image(13)
    if variant.endswith("disabled"):
        top, bottom, border = BOX_DISABLED_TOP, BOX_DISABLED_BOTTOM, BOX_DISABLED_BORDER
    elif variant.endswith("hover"):
        top, bottom, border = BOX_HOVER_TOP, BOX_HOVER_BOTTOM, BOX_HOVER_BORDER
    elif variant.startswith("checked"):
        top, bottom, border = BOX_HOVER_TOP, BOX_HOVER_BOTTOM, BOX_HOVER_BORDER
    else:
        top, bottom, border = BOX_TOP, BOX_BOTTOM, BOX_BORDER

    draw_box(p, QRectF(0.5, 0.5, 12.0, 12.0), top, bottom, border, 2.5)

    if variant.startswith("checked"):
        mark = DISABLED_MARK if variant == "checked-disabled" else CHECK_BLUE
        # White shadow first, then the blue check.
        p.setPen(QPen(QColor(255, 255, 255, 220), 3.2, Qt.PenStyle.SolidLine,
                      Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
        p.drawPolyline(QPolygonF([
            QPointF(3.2, 6.8),
            QPointF(5.6, 9.2),
            QPointF(9.8, 4.0),
        ]))
        p.setPen(QPen(mark, 2.0, Qt.PenStyle.SolidLine,
                      Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
        p.drawPolyline(QPolygonF([
            QPointF(3.2, 6.8),
            QPointF(5.6, 9.2),
            QPointF(9.8, 4.0),
        ]))
    elif variant == "indeterminate":
        p.setPen(QPen(CHECK_BLUE, 2.0, Qt.PenStyle.SolidLine,
                      Qt.PenCapStyle.RoundCap))
        p.drawLine(QPointF(4.0, 6.5), QPointF(9.0, 6.5))

    p.end()
    save(img, f"checkbox-{variant}.png", "shared")


def draw_radio(variant: str) -> None:
    img, p = image(13)
    if variant.endswith("disabled"):
        top, bottom, border = BOX_DISABLED_TOP, BOX_DISABLED_BOTTOM, BOX_DISABLED_BORDER
    elif variant.endswith("hover"):
        top, bottom, border = BOX_HOVER_TOP, BOX_HOVER_BOTTOM, BOX_HOVER_BORDER
    elif variant.startswith("checked"):
        top, bottom, border = BOX_HOVER_TOP, BOX_HOVER_BOTTOM, BOX_HOVER_BORDER
    else:
        top, bottom, border = BOX_TOP, BOX_BOTTOM, BOX_BORDER

    outer = QRectF(0.5, 0.5, 12.0, 12.0)
    p.setPen(QPen(border, 1.0))
    p.setBrush(box_gradient(outer, top, bottom))
    p.drawEllipse(outer)

    # Glass highlight arc at the top of the circle.
    p.setPen(QPen(QColor(255, 255, 255, 190), 1.0))
    p.drawArc(QRectF(2.0, 1.4, 9.0, 9.0), 30 * 16, 120 * 16)

    if variant.startswith("checked"):
        inner = QRectF(3.8, 3.8, 5.4, 5.4)
        if variant == "checked-disabled":
            p.setPen(QPen(DISABLED_MARK, 1.0))
            p.setBrush(DISABLED_MARK)
        else:
            grad = QLinearGradient(inner.topLeft(), inner.bottomLeft())
            grad.setColorAt(0.0, CHECK_BLUE_LIGHT)
            grad.setColorAt(1.0, CHECK_BLUE)
            p.setPen(QPen(CHECK_BLUE, 1.0))
            p.setBrush(grad)
        p.drawEllipse(inner)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(255, 255, 255, 170))
        p.drawEllipse(QRectF(5.0, 4.6, 1.6, 1.2))

    p.end()
    save(img, f"radio-{variant}.png", "shared")


def arrow_polygon(direction: str, size: float = 9.0) -> QPolygonF:
    if direction == "down":
        return QPolygonF([
            QPointF(1.5, 3.0), QPointF(7.5, 3.0), QPointF(4.5, 6.5),
        ])
    if direction == "up":
        return QPolygonF([
            QPointF(1.5, 6.0), QPointF(7.5, 6.0), QPointF(4.5, 2.5),
        ])
    if direction == "left":
        return QPolygonF([
            QPointF(6.0, 1.5), QPointF(6.0, 7.5), QPointF(2.5, 4.5),
        ])
    if direction == "right":
        return QPolygonF([
            QPointF(3.0, 1.5), QPointF(3.0, 7.5), QPointF(6.5, 4.5),
        ])
    raise ValueError(direction)


def draw_arrow(name: str, direction: str, disabled: bool = False) -> None:
    img, p = image(9)
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(ARROW_DISABLED if disabled else ARROW)
    p.drawPolygon(arrow_polygon(direction))
    p.end()
    save(img, name)


def draw_branch(open_state: bool) -> None:
    img, p = image(9)
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(TREE_ARROW)
    if open_state:
        p.drawPolygon(QPolygonF([
            QPointF(1.5, 3.0), QPointF(7.5, 3.0), QPointF(4.5, 7.0),
        ]))
    else:
        p.drawPolygon(QPolygonF([
            QPointF(3.0, 1.5), QPointF(3.0, 7.5), QPointF(7.0, 4.5),
        ]))
    p.end()
    save(img, "branch-open.png" if open_state else "branch-closed.png")


def draw_search() -> None:
    img, p = image(16)
    p.setPen(QPen(SEARCH, 1.6))
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawEllipse(QRectF(3.0, 3.0, 7.0, 7.0))
    p.setPen(QPen(SEARCH, 2.2, Qt.PenStyle.SolidLine,
                  Qt.PenCapStyle.RoundCap))
    p.drawLine(QPointF(9.6, 9.6), QPointF(13.2, 13.2))
    p.end()
    save(img, "search.png")


def draw_dock_close() -> None:
    img, p = image(12)
    p.setPen(QPen(DOCK_GLYPH, 1.5, Qt.PenStyle.SolidLine,
                  Qt.PenCapStyle.RoundCap))
    p.drawLine(QPointF(3.0, 3.0), QPointF(9.0, 9.0))
    p.drawLine(QPointF(9.0, 3.0), QPointF(3.0, 9.0))
    p.end()
    save(img, "dock-close.png")


def draw_dock_float() -> None:
    img, p = image(12)
    p.setPen(QPen(DOCK_GLYPH, 1.2))
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawRect(QRectF(2.5, 4.5, 5.5, 5.5))
    p.drawRect(QRectF(4.5, 2.5, 5.5, 5.5))
    p.end()
    save(img, "dock-float.png")


def draw_sizegrip() -> None:
    img, p = image(16)
    p.setPen(QPen(GRIP, 1.0))
    for offset in (0, 3, 6):
        p.drawLine(QPointF(15.0 - offset, 15.0), QPointF(15.0, 15.0 - offset))
    p.end()
    save(img, "sizegrip.png")


def main() -> None:
    for directory in (SHARED_DIR, OUT_DIR, DARK_DIR):
        directory.mkdir(parents=True, exist_ok=True)
    app = QGuiApplication([])  # noqa: F841 - keeps QPainter happy

    for variant in (
        "unchecked",
        "unchecked-hover",
        "unchecked-disabled",
        "checked",
        "checked-hover",
        "checked-disabled",
        "indeterminate",
    ):
        draw_checkbox(variant)

    for variant in (
        "unchecked",
        "unchecked-hover",
        "unchecked-disabled",
        "checked",
        "checked-hover",
        "checked-disabled",
    ):
        draw_radio(variant)

    draw_arrow("combo-arrow.png", "down")
    draw_arrow("arrow-down.png", "down")
    draw_arrow("spin-up.png", "up")
    draw_arrow("spin-down.png", "down")
    draw_arrow("sort-up.png", "up")
    draw_arrow("sort-down.png", "down")

    for direction in ("up", "down", "left", "right"):
        draw_arrow(f"scroll-{direction}.png", direction)
        draw_arrow(f"scroll-{direction}-disabled.png", direction, disabled=True)

    draw_branch(False)
    draw_branch(True)
    draw_search()
    draw_dock_close()
    draw_dock_float()
    draw_sizegrip()

    # Light-on-dark variants for the Aero Dark theme. Only monochrome glyphs
    # are recolored; the Win7 checkbox/radio boxes keep their own artwork.
    # Aero 深色主题使用的浅色单色图标；Win7 复选框/单选按钮保留原图。
    for name in (
        "arrow-down.png",
        "branch-closed.png",
        "branch-open.png",
        "combo-arrow.png",
        "dock-close.png",
        "dock-float.png",
        "scroll-down.png",
        "scroll-left.png",
        "scroll-right.png",
        "scroll-up.png",
        "search.png",
        "sizegrip.png",
        "sort-down.png",
        "sort-up.png",
        "spin-down.png",
        "spin-up.png",
    ):
        recolor(name, name, DARK_GLYPH)

    for name in (
        "scroll-down-disabled.png",
        "scroll-left-disabled.png",
        "scroll-right-disabled.png",
        "scroll-up-disabled.png",
    ):
        recolor(name, name, DARK_GLYPH_DISABLED)


if __name__ == "__main__":
    main()
