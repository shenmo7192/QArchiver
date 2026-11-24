#!/usr/bin/env python3
"""Generate the modern (HaoZip-style) icon set for QArchiver.

The script renders SVG sources with rsvg-convert and writes the PNGs into the
folders used by the resource bundle:

    resources/icons/modern/              toolbar and navigation icons, app logo
    resources/icons/file-types/modern/   modern file-type icons
    resources/icons/file-types/classic/  classic file-type icons
    resources/themes/modern/light/       glyphs for light backgrounds
    resources/themes/modern/dark/        glyphs for dark backgrounds
    resources/svg/modern/                committed SVG sources
    resources/app-icons/                 installed application icons (.png/.ico)

It is intentionally dependency-light so packagers can regenerate the assets
after changing a colour or shape:

    python3 tools/gen_modern_icons.py

The generated files are committed; running the script is only needed when the
icons themselves change.  The application icon is additionally converted to a
multi-resolution Windows .ico.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "resources"
ICONS = RES / "icons" / "modern"
FILE_TYPES = RES / "icons" / "file-types" / "modern"
GLYPHS_ROOT = RES / "themes" / "modern"
GLYPHS_LIGHT = GLYPHS_ROOT / "light"
GLYPHS_DARK = RES / "themes" / "modern" / "dark"
SVG_OUT = RES / "svg" / "modern"
APP_ICONS = RES / "app-icons"

# Monochrome glyphs with a light/ and a dark/ variant, referenced by
# modern.qss and the custom delegates.
# 具有 light/ 与 dark/ 两套变体的单色字形，由 modern.qss 与自绘委托引用。
GLYPH_NAMES = {
    "arrow-down", "arrow-up", "arrow-left", "arrow-right",
    "branch-closed", "branch-open", "chevron-down",
    "chevron-right", "chevron-up", "close",
    "search", "sizegrip", "sort-down", "sort-up",
}

# Glyphs that are identical in both themes and live at the theme root.
# 深浅主题共用、位于主题根目录的字形。
NEUTRAL_GLYPHS = {"check", "minus", "radio-dot"}

# Icons that are also used as themed glyphs (written to both places).
# 同时作为主题字形使用的图标（两处都写）。
DUAL_ICONS = {"search.png", "close.png"}


def output_path(name: str) -> Path:
    """Return the destination file for a generated icon."""
    stem = name[:-4]
    if stem.endswith("-light"):
        # Dark-theme variants share the light glyph names in dark/.
        # 深色主题变体在 dark/ 下使用与浅色字形相同的名称。
        return GLYPHS_DARK / (stem[: -len("-light")] + ".png")
    if stem in NEUTRAL_GLYPHS:
        return GLYPHS_ROOT / name
    if name in DUAL_ICONS:
        # These are toolbar icons and themed glyphs at the same time.  Write
        # the toolbar copy first, then duplicate it into light/ below.
        # 这些图标既用于工具栏又作为主题字形；先写工具栏副本，再复制到 light/。
        return ICONS / name
    if stem in GLYPH_NAMES:
        return GLYPHS_LIGHT / name
    if stem.startswith("file-") or stem in ("file", "folder"):
        return FILE_TYPES / name
    return ICONS / name

BLUE = "#2f8cf6"
BLUE_DARK = "#1b6fd6"
BLUE_LIGHT = "#7cc0ff"
CYAN = "#41c7f5"
GREEN = "#37c76a"
GREEN_DARK = "#1e9e4c"
ORANGE = "#ff9f1a"
ORANGE_DARK = "#e77c00"
RED = "#f25555"
PURPLE = "#8b6cf0"
YELLOW = "#ffd54a"
GRAY = "#8a98a8"
INK = "#1e2a39"

# Exact pixel sizes for glyphs that are drawn by Qt style sheets: QSS does not
# scale these images reliably, so each one is rendered at its final size.
SIZE_OVERRIDES = {
    "arrow-down.png": 12, "arrow-up.png": 12, "arrow-left.png": 12,
    "arrow-right.png": 12, "branch-closed.png": 12, "branch-open.png": 12,
    "check.png": 16, "minus.png": 16, "radio-dot.png": 12,
    "sort-up.png": 12, "sort-down.png": 12, "sizegrip.png": 12,
}
SMALL_GLYPHS = {
    "back.png", "forward.png", "up.png",
    "refresh.png", "search.png", "close.png", "chevron-down.png",
    "chevron-right.png", "chevron-up.png", "sort-up.png", "sort-down.png",
    "sizegrip.png",
}
FILE_ICONS = {"folder.png", "file.png"}


def svg(body: str, size: int = 64) -> str:
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" '
        f'height="{size}" viewBox="0 0 64 64">'
        f"{body}</svg>"
    )


def grad(gid: str, top: str, bottom: str, x1: float = 0, y1: float = 0,
         x2: float = 0, y2: float = 1) -> str:
    return (
        f'<linearGradient id="{gid}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}">'
        f'<stop offset="0" stop-color="{top}"/>'
        f'<stop offset="1" stop-color="{bottom}"/></linearGradient>'
    )


DEFS = (
    "<defs>"
    + grad("blue", "#79c3ff", "#2380e8")
    + grad("blueDeep", "#4aa6ff", "#1666c9")
    + grad("green", "#6ee899", "#20b455")
    + grad("orange", "#ffc14d", "#f28a00")
    + grad("red", "#ff8585", "#e03a3a")
    + grad("purple", "#b19bff", "#7050e0")
    + grad("yellow", "#ffe785", "#ffc21c")
    + grad("gray", "#c4ceda", "#8c9bab")
    + grad("teal", "#5fe3d2", "#14b8a6")
    + "</defs>"
)


def box(x: float, y: float, w: float, h: float, fill: str = "url(#blue)",
        r: float = 4, stroke: str | None = None) -> str:
    border = f' stroke="{stroke}" stroke-width="1"' if stroke else ""
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" '
            f'ry="{r}" fill="{fill}"{border}/>')


def archive_box(body_fill: str = "url(#blue)", lid_fill: str = "#9fd2ff",
                band_fill: str = "url(#orange)") -> str:
    """A friendly 3D archive box, the visual family base."""
    return (
        # shadow
        '<ellipse cx="32" cy="56" rx="21" ry="3.4" fill="#1e2a39" opacity=".10"/>'
        # body
        + box(11, 22, 42, 29, body_fill, 4)
        # lid
        + box(8, 15, 48, 12, lid_fill, 4)
        + '<rect x="8" y="23" width="48" height="4" fill="#ffffff" opacity=".22"/>'
        # zipper band
        + f'<rect x="28" y="15" width="11" height="36" rx="2" fill="{band_fill}"/>'
        + '<rect x="30.5" y="18" width="6" height="2.4" rx="1.2" fill="#fff" opacity=".75"/>'
        + '<rect x="30.5" y="23.5" width="6" height="2.4" rx="1.2" fill="#fff" opacity=".75"/>'
        + '<rect x="30.5" y="29" width="6" height="2.4" rx="1.2" fill="#fff" opacity=".75"/>'
        + '<rect x="30.5" y="34.5" width="6" height="2.4" rx="1.2" fill="#fff" opacity=".75"/>'
        + '<rect x="30.5" y="40" width="6" height="2.4" rx="1.2" fill="#fff" opacity=".75"/>'
        # handle
        + '<path d="M24 15c0-4.5 3.6-8 8-8s8 3.5 8 8" fill="none" '
          'stroke="#f5b53c" stroke-width="3" stroke-linecap="round"/>'
        # highlights
        + '<circle cx="16" cy="27" r="1.5" fill="#fff" opacity=".65"/>'
    )


def badge_plus(cx: float, cy: float, r: float = 10) -> str:
    return (
        f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#green)" '
        f'stroke="#ffffff" stroke-width="2"/>'
        f'<path d="M{cx - 4} {cy}h8M{cx} {cy - 4}v8" stroke="#fff" '
        'stroke-width="2.6" stroke-linecap="round"/>'
    )


def arrow_down(cx: float, cy: float, size: float = 10) -> str:
    return (f'<path d="M{cx - size} {cy - size * 0.6} L{cx} {cy + size * 0.7} '
            f'L{cx + size} {cy - size * 0.6}" fill="none" stroke="#fff" '
            'stroke-width="3.4" stroke-linecap="round" stroke-linejoin="round"/>')


def arrow_up(cx: float, cy: float, size: float = 10) -> str:
    return (f'<path d="M{cx - size} {cy + size * 0.6} L{cx} {cy - size * 0.7} '
            f'L{cx + size} {cy + size * 0.6}" fill="none" stroke="#fff" '
            'stroke-width="3.4" stroke-linecap="round" stroke-linejoin="round"/>')


def doc_icon(fill: str, glyph: str = "", accent: str = "#ffffff",
             size: int = 64) -> str:
    """A clean 64-unit document with a folded corner and an optional glyph."""
    body = (
        '<path d="M14 6h24l12 12v40H14z" fill="#ffffff" stroke="#d6e2ef" stroke-width="1.2"/>'
        '<path d="M38 6l12 12H40a2 2 0 0 1-2-2z" fill="#e8f2fc"/>'
        f'<rect x="14" y="6" width="36" height="10" rx="2" fill="{fill}"/>'
        f"{glyph}"
    )
    return svg(body, size)


def toolbar_icons() -> dict[str, str]:
    icons: dict[str, str] = {}

    # Open: blue folder with a document.
    icons["open.png"] = svg(
        DEFS
        + '<ellipse cx="32" cy="55" rx="22" ry="3" fill="#1e2a39" opacity=".08"/>'
        + '<path d="M7 18a4 4 0 0 1 4-4h13l5 6h24a4 4 0 0 1 4 4v27H7z" fill="url(#blueDeep)"/>'
        + '<path d="M7 25h50v22a4 4 0 0 1-4 4H11a4 4 0 0 1-4-4z" fill="url(#blue)"/>'
        + '<path d="M7 25h50v5H7z" fill="#ffffff" opacity=".25"/>'
        + '<path d="M24 30h13l6 6v15H24z" fill="#ffffff"/>'
        + '<path d="M37 30l6 6h-6z" fill="#cfe6ff"/>'
        + '<path d="M28 38h11M28 43h11M28 48h7" stroke="#7ea9d6" stroke-width="1.6" '
          'stroke-linecap="round"/>'
    )

    # Add files: archive box + green plus.
    icons["add.png"] = svg(DEFS + archive_box() + badge_plus(49, 48, 11))

    # Extract: archive box with a down arrow badge.
    icons["extract.png"] = svg(
        DEFS + archive_box()
        + '<circle cx="49" cy="48" r="11" fill="url(#blueDeep)" stroke="#fff" stroke-width="2"/>'
        + arrow_down(49, 48, 5)
    )

    # One-click extract: archive box + lightning.
    icons["oneclick.png"] = svg(
        DEFS + archive_box()
        + '<circle cx="48" cy="47" r="13" fill="url(#yellow)" stroke="#fff" stroke-width="2"/>'
        + '<path d="M50 38l-8 11h6l-2 9 9-12h-6z" fill="#f28a00"/>'
    )

    # Compress: orange box + up arrow badge.
    icons["compress.png"] = svg(
        DEFS
        + archive_box("url(#orange)", "#ffd98a", "url(#blue)")
        + '<circle cx="49" cy="48" r="11" fill="url(#green)" stroke="#fff" stroke-width="2"/>'
        + arrow_up(49, 48, 5)
    )

    # Delete: green trash can.
    icons["delete.png"] = svg(
        DEFS
        + '<ellipse cx="32" cy="56" rx="17" ry="3" fill="#1e2a39" opacity=".10"/>'
        + '<path d="M21 14h22l-2 5H23z" fill="#2fb85a"/>'
        + '<path d="M26 9h12l2 5H24z" fill="url(#green)"/>'
        + '<path d="M18 20h28l-3 33a4 4 0 0 1-4 3.6H25a4 4 0 0 1-4-3.6z" fill="url(#green)"/>'
        + '<path d="M22 24h20l-2.4 27H24.4z" fill="#ffffff" opacity=".18"/>'
        + '<path d="M28 27v20M36 27v20" stroke="#ffffff" stroke-width="2.4" '
          'stroke-linecap="round" opacity=".8"/>'
    )

    # Test: teal shield + check.
    icons["test.png"] = svg(
        DEFS
        + '<path d="M32 7l20 7v17c0 12-8 20-20 26-12-6-20-14-20-26V14z" fill="url(#teal)"/>'
        + '<path d="M32 12l15 5.4V31c0 9-5.8 15.4-15 20.4C22.8 46.4 17 40 17 31V17.4z" '
          'fill="#ffffff" opacity=".16"/>'
        + '<path d="M22 32l7 7 13-15" fill="none" stroke="#fff" stroke-width="4" '
          'stroke-linecap="round" stroke-linejoin="round"/>'
    )

    # Toolbox.
    icons["toolbox.png"] = svg(
        DEFS
        + '<ellipse cx="32" cy="56" rx="21" ry="3" fill="#1e2a39" opacity=".08"/>'
        + '<path d="M24 18v-4a4 4 0 0 1 4-4h8a4 4 0 0 1 4 4v4" fill="none" '
          'stroke="#7d8fa3" stroke-width="3.4" stroke-linecap="round"/>'
        + box(10, 20, 44, 30, "url(#blue)", 5)
        + '<rect x="10" y="20" width="44" height="9" rx="4" fill="#ffffff" opacity=".22"/>'
        + '<rect x="27" y="29" width="10" height="7" rx="2" fill="#ffffff"/>'
        + '<path d="M10 38h44" stroke="#ffffff" stroke-width="2" opacity=".35"/>'
    )

    # Settings gear: a round cog body with evenly spaced teeth, so the outer
    # silhouette reads as a circle rather than an eight-point star.
    # 设置齿轮：圆形齿轮主体 + 均匀分布的齿，外轮廓保持圆形而不是八角星。
    gear_teeth = "".join(
        f'<rect x="28.5" y="8" width="7" height="9" rx="2.2" '
        f'transform="rotate({angle} 32 32)"/>'
        for angle in range(0, 360, 45)
    )
    icons["settings.png"] = svg(
        DEFS
        + '<circle cx="32" cy="32" r="24" fill="url(#blue)"/>'
        + '<g fill="#ffffff">'
        + gear_teeth
        + '<circle cx="32" cy="32" r="17"/>'
        + '<circle cx="32" cy="32" r="7.2" fill="url(#blue)"/>'
        + '</g>'
    )

    # Help.
    icons["help.png"] = svg(
        DEFS
        + '<circle cx="32" cy="32" r="24" fill="url(#blue)"/>'
        + '<path d="M25 25a7.5 7.5 0 1 1 10.5 7c-2.3 1.2-3.5 2.6-3.5 5.2v1.2" fill="none" '
          'stroke="#fff" stroke-width="4" stroke-linecap="round"/>'
        + '<circle cx="32" cy="45.5" r="2.6" fill="#fff"/>'
    )

    # Small UI glyphs.
    icons["back.png"] = svg(
        '<path d="M40 14L22 32l18 18" fill="none" stroke="#6b7a8c" stroke-width="4" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 32)
    icons["forward.png"] = svg(
        '<path d="M24 14l18 18-18 18" fill="none" stroke="#6b7a8c" stroke-width="4" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 32)
    icons["up.png"] = svg(
        '<path d="M14 30l18-18 18 18M32 14v32" fill="none" stroke="#6b7a8c" '
        'stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>', 32)
    icons["refresh.png"] = svg(
        '<path d="M46 32a14 14 0 1 1-4.1-9.9" fill="none" stroke="#6b7a8c" '
        'stroke-width="4" stroke-linecap="round"/>'
        '<path d="M46 12v12H34" fill="none" stroke="#6b7a8c" stroke-width="4" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 32)
    icons["search.png"] = svg(
        '<circle cx="28" cy="27" r="12" fill="none" stroke="#8a98a8" stroke-width="3.6"/>'
        '<path d="M37 36l10 10" stroke="#8a98a8" stroke-width="4" stroke-linecap="round"/>', 32)
    icons["close.png"] = svg(
        '<path d="M12 12l20 20M32 12L12 32" stroke="#6b7a8c" stroke-width="4" '
        'stroke-linecap="round"/>', 44)
    icons["chevron-down.png"] = svg(
        '<path d="M8 12l8 8 8-8" fill="none" stroke="#7a8796" stroke-width="3" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 32)
    icons["chevron-right.png"] = svg(
        '<path d="M12 8l8 8-8 8" fill="none" stroke="#7a8796" stroke-width="3" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 32)
    icons["chevron-up.png"] = svg(
        '<path d="M8 20l8-8 8 8" fill="none" stroke="#7a8796" stroke-width="3" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 32)
    icons["check.png"] = svg(
        '<path d="M4 8.5l2.6 2.6L12 4.6" fill="none" stroke="#ffffff" stroke-width="2" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 16)
    icons["radio-dot.png"] = svg('<circle cx="6" cy="6" r="2.6" fill="#ffffff"/>', 12)
    icons["minus.png"] = svg('<path d="M4 8h8" stroke="#ffffff" stroke-width="2" '
                             'stroke-linecap="round"/>', 16)
    icons["sort-up.png"] = svg(
        '<path d="M6 1l4 5H7v5H5V6H2z" fill="#7a8796"/>', 12)
    icons["sort-down.png"] = svg(
        '<path d="M6 11L2 6h3V1h2v5h3z" fill="#7a8796"/>', 12)
    icons["sizegrip.png"] = svg(
        '<path d="M2 10L10 2M5 10l5-5M8 10l2-2" stroke="#c4ceda" stroke-width="1.4"/>', 12)
    icons["arrow-down.png"] = svg(
        '<path d="M2 3l4 4 4-4" fill="none" stroke="#7a8796" stroke-width="1.6" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 12)
    icons["arrow-up.png"] = svg(
        '<path d="M2 8l4-4 4 4" fill="none" stroke="#7a8796" stroke-width="1.6" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 12)
    icons["arrow-left.png"] = svg(
        '<path d="M8 2L4 6l4 4" fill="none" stroke="#7a8796" stroke-width="1.6" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 12)
    icons["arrow-right.png"] = svg(
        '<path d="M4 2l4 4-4 4" fill="none" stroke="#7a8796" stroke-width="1.6" '
        'stroke-linecap="round" stroke-linejoin="round"/>', 12)
    icons["branch-closed.png"] = icons["arrow-right.png"]
    icons["branch-open.png"] = icons["arrow-down.png"]

    # Folder / file rows.
    # Keep the folder artwork centred in the 64-unit viewBox; the list and
    # grid views both render this pixmap at different sizes.
    # 让文件夹图形在 64 单位 viewBox 中居中；列表和网格会以不同尺寸绘制。
    icons["folder.png"] = svg(
        '<rect x="5" y="8" width="24" height="12" rx="3" fill="#2d7fe0"/>'
        '<rect x="5" y="13" width="54" height="47" rx="6" fill="#2d7fe0"/>'
        '<rect x="5" y="23" width="54" height="35" rx="6" fill="#4aa6ff"/>'
        '<rect x="5" y="23" width="54" height="5" rx="2" fill="#ffffff" opacity=".22"/>',
        32)
    icons["file.png"] = doc_icon("#7ea9d6", size=64)

    # Additional common file kinds / 更多常见文件类型
    icons["file-ppt.png"] = doc_icon(
        "#e2574c",
        '<rect x="20" y="28" width="24" height="16" rx="2" fill="#ffe1dc" '
        'stroke="#e2574c" stroke-width="1.6"/>'
        '<path d="M25 44v4M39 44v4M22 48h20" stroke="#e2574c" stroke-width="1.8" '
        'stroke-linecap="round"/>'
        '<path d="M27 34h10M27 38h7" stroke="#e2574c" stroke-width="1.6" '
        'stroke-linecap="round"/>')
    icons["file-font.png"] = doc_icon(
        "#7b61d1",
        '<text x="32" y="46" font-size="22" text-anchor="middle" '
        'font-family="Georgia,DejaVu Serif,serif" font-weight="700" '
        'fill="#7b61d1">Aa</text>')
    icons["file-iso.png"] = doc_icon(
        "#14b8a6",
        '<circle cx="32" cy="37" r="11" fill="none" stroke="#14b8a6" stroke-width="2.5"/>'
        '<circle cx="32" cy="37" r="4" fill="#14b8a6"/>'
        '<path d="M32 26a11 11 0 0 1 9.5 5.5" fill="none" stroke="#8ff0e5" '
        'stroke-width="2" stroke-linecap="round"/>')
    icons["file-ebook.png"] = doc_icon(
        "#e77c00",
        '<path d="M21 30h9a4 4 0 0 1 4 4v14a4 4 0 0 0-4-4h-9z" fill="#ffe3bf" '
        'stroke="#e77c00" stroke-width="1.6"/>'
        '<path d="M43 30h-9a4 4 0 0 0-4 4v14a4 4 0 0 1 4-4h9z" fill="#fff2dd" '
        'stroke="#e77c00" stroke-width="1.6"/>'
        '<path d="M32 34v14" stroke="#e77c00" stroke-width="1.6"/>')
    icons["file-database.png"] = doc_icon(
        "#2f8cf6",
        '<ellipse cx="32" cy="30" rx="10" ry="4" fill="#cfe6ff" stroke="#2f8cf6" stroke-width="1.6"/>'
        '<path d="M22 30v14c0 2.2 4.5 4 10 4s10-1.8 10-4V30" fill="#e8f3ff" '
        'stroke="#2f8cf6" stroke-width="1.6"/>'
        '<path d="M22 37c0 2.2 4.5 4 10 4s10-1.8 10-4" fill="none" stroke="#2f8cf6" stroke-width="1.4"/>')
    icons["file-torrent.png"] = doc_icon(
        "#37c76a",
        '<path d="M32 27v15" stroke="#37c76a" stroke-width="3" stroke-linecap="round"/>'
        '<path d="M26 36l6 7 6-7" fill="none" stroke="#37c76a" stroke-width="3" '
        'stroke-linecap="round" stroke-linejoin="round"/>'
        '<path d="M23 48h18" stroke="#8ee7ab" stroke-width="2" stroke-linecap="round"/>')
    icons["file-design.png"] = doc_icon(
        "#e05fa8",
        '<path d="M22 43a11 11 0 1 1 14-14c2.5 2.5 1.2 5-1.2 5h-2.3c-1.6 0-2.5 1.3-1.8 2.7 1.3 2.6-1 5.3-3.9 5.3H22z" '
        'fill="#ffe3f1" stroke="#e05fa8" stroke-width="1.6"/>'
        '<circle cx="27" cy="33" r="1.8" fill="#ffc21c"/>'
        '<circle cx="33" cy="30" r="1.8" fill="#2f8cf6"/>'
        '<circle cx="38" cy="34" r="1.8" fill="#37c76a"/>')
    icons["file-subtitle.png"] = doc_icon(
        "#6b7a8c",
        '<rect x="19" y="29" width="26" height="15" rx="3" fill="#eef2f6" stroke="#6b7a8c" stroke-width="1.6"/>'
        '<text x="32" y="41" font-size="11" text-anchor="middle" '
        'font-family="Arial,DejaVu Sans,sans-serif" font-weight="700" fill="#6b7a8c">CC</text>')
    icons["file-3d.png"] = doc_icon(
        "#8b6cf0",
        '<path d="M32 25l11 6v12l-11 6-11-6V31z" fill="#eee9ff" stroke="#8b6cf0" stroke-width="1.8"/>'
        '<path d="M21 31l11 6 11-6M32 37v12" fill="none" stroke="#8b6cf0" stroke-width="1.8"/>')
    icons["file-package.png"] = doc_icon(
        "#b07a4a",
        '<path d="M21 33l11-6 11 6v12l-11 6-11-6z" fill="#f2e3d2" stroke="#b07a4a" stroke-width="1.8"/>'
        '<path d="M21 33l11 6 11-6M32 39v12" fill="none" stroke="#b07a4a" stroke-width="1.8"/>'
        '<path d="M32 27v6" stroke="#e77c00" stroke-width="3"/>')

    # Archive type icons.
    icons["file-zip.png"] = doc_icon(
        "#2f8cf6",
        '<path d="M30 20v4M30 28v4M30 36v8" stroke="#cfe6ff" stroke-width="2" '
        'stroke-dasharray="2 2"/>'
        '<rect x="35" y="40" width="4" height="8" rx="2" fill="#2f8cf6"/>')
    icons["file-rar.png"] = doc_icon("#8b6cf0", '<path d="M30 22v20" stroke="#e6ddff" '
                                     'stroke-width="3" stroke-dasharray="2 3"/>')
    icons["file-7z.png"] = doc_icon("#14b8a6", '<text x="31" y="45" font-size="13" '
                                    'font-family="Arial,DejaVu Sans,sans-serif" '
                                    'font-weight="700" fill="#0e8f83">7z</text>')
    icons["file-tar.png"] = doc_icon("#b07a4a", '<text x="31" y="45" font-size="11" '
                                     'font-family="Arial,DejaVu Sans,sans-serif" '
                                     'font-weight="700" fill="#8b5a2b">TAR</text>')
    icons["file-image.png"] = doc_icon(
        "#37c76a",
        '<rect x="20" y="25" width="24" height="17" rx="2" fill="#e9fbf0"/>'
        '<circle cx="26" cy="30" r="2.4" fill="#ffc21c"/>'
        '<path d="M21 41l7-8 4 4 4-5 7 9z" fill="#37c76a"/>')
    icons["file-text.png"] = doc_icon(
        "#8a98a8",
        '<path d="M22 27h20M22 32h20M22 37h20M22 42h12" stroke="#9aa9b9" '
        'stroke-width="2" stroke-linecap="round"/>')
    icons["file-audio.png"] = doc_icon(
        "#e05fa8",
        '<path d="M34 24v16a4 4 0 1 1-3-3.9V28l10-2v-4z" fill="#e05fa8"/>')
    icons["file-video.png"] = doc_icon(
        "#7b61d1",
        '<rect x="21" y="27" width="22" height="15" rx="2" fill="#efeaff"/>'
        '<path d="M29 31l8 4-8 4z" fill="#7b61d1"/>')
    icons["file-pdf.png"] = doc_icon(
        "#f25555",
        '<rect x="19" y="30" width="26" height="13" rx="2" fill="#f25555"/>'
        '<text x="32" y="40" font-size="8" text-anchor="middle" '
        'font-family="Arial,DejaVu Sans,sans-serif" font-weight="700" '
        'fill="#ffffff">PDF</text>')
    icons["file-doc.png"] = doc_icon(
        "#2f8cf6",
        '<path d="M24 27h6l14 14H24z" fill="#cfe6ff" opacity=".7"/>'
        '<path d="M34 24l10 10h-8a2 2 0 0 1-2-2z" fill="#2f8cf6"/>'
        '<path d="M23 44h18" stroke="#2f8cf6" stroke-width="2.4" stroke-linecap="round"/>')
    icons["file-sheet.png"] = doc_icon(
        "#37c76a",
        '<rect x="20" y="26" width="24" height="18" rx="1.5" fill="#eefbf2" stroke="#37c76a"/>'
        '<path d="M20 32h24M20 38h24M28 26v18M36 26v18" stroke="#37c76a" stroke-width="1.2"/>')
    icons["file-code.png"] = doc_icon(
        "#243447",
        '<text x="31" y="44" font-size="12" text-anchor="middle" '
        'font-family="monospace" font-weight="700" fill="#9fd2ff">&lt;/&gt;</text>')
    icons["file-exe.png"] = doc_icon(
        "#6b7a8c",
        '<circle cx="32" cy="35" r="7" fill="none" stroke="#6b7a8c" stroke-width="2.4"/>'
        '<path d="M32 25v4M32 41v4M22 35h4M38 35h4" stroke="#6b7a8c" stroke-width="2"/>')

    # Archive view-mode glyphs / 归档内容视图切换图标
    icons["view-grid.png"] = svg(
        '<rect x="8" y="8" width="22" height="22" rx="4" fill="#6b7a8c"/>'
        '<rect x="34" y="8" width="22" height="22" rx="4" fill="#6b7a8c"/>'
        '<rect x="8" y="34" width="22" height="22" rx="4" fill="#6b7a8c"/>'
        '<rect x="34" y="34" width="22" height="22" rx="4" fill="#6b7a8c"/>', 32)
    icons["view-list.png"] = svg(
        '<circle cx="12" cy="18" r="4" fill="#6b7a8c"/>'
        '<circle cx="12" cy="32" r="4" fill="#6b7a8c"/>'
        '<circle cx="12" cy="46" r="4" fill="#6b7a8c"/>'
        '<path d="M24 18h32M24 32h32M24 46h32" stroke="#6b7a8c" '
        'stroke-width="5" stroke-linecap="round"/>', 32)
    icons["view-tree.png"] = svg(
        '<path d="M22 14h8v18h8M30 32h8M30 50h8" fill="none" stroke="#6b7a8c" '
        'stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>'
        '<rect x="8" y="8" width="14" height="12" rx="3" fill="#6b7a8c"/>'
        '<rect x="44" y="23" width="14" height="11" rx="3" fill="#6b7a8c"/>'
        '<rect x="44" y="34" width="14" height="11" rx="3" fill="#6b7a8c"/>'
        '<rect x="44" y="45" width="14" height="11" rx="3" fill="#6b7a8c"/>', 32)
    # Welcome page / app icon.
    icons["app.png"] = svg(DEFS + archive_box() + badge_plus(48, 49, 10))

    # Light variants of the small monochrome glyphs for the dark theme.
    light_names = [
        "back.png", "forward.png", "up.png", "refresh.png", "search.png",
        "close.png", "chevron-down.png", "chevron-right.png", "chevron-up.png",
        "sort-up.png", "sort-down.png", "arrow-down.png", "arrow-up.png",
        "arrow-left.png", "arrow-right.png", "branch-closed.png", "branch-open.png",
        "sizegrip.png",
    ]
    for name in light_names:
        body = icons[name]
        body = body.replace("#6b7a8c", "#d7e0ea")
        body = body.replace("#8a98a8", "#c4ceda")
        body = body.replace("#7a8796", "#c4ceda")
        icons[name.replace(".png", "-light.png")] = body

    return icons


APP_ICON = svg(
    DEFS
    + '<rect width="64" height="64" rx="14" fill="url(#blue)"/>'
    + '<rect x="1" y="1" width="62" height="62" rx="13" fill="none" stroke="#ffffff" '
      'stroke-opacity=".35"/>'
    + archive_box(),
    256,
)


def main() -> int:
    if shutil.which("rsvg-convert") is None:
        print("rsvg-convert is required (librsvg2-bin)", file=sys.stderr)
        return 1

    for directory in (ICONS, FILE_TYPES, GLYPHS_LIGHT, GLYPHS_DARK, APP_ICONS):
        directory.mkdir(parents=True, exist_ok=True)
    SVG_OUT.mkdir(parents=True, exist_ok=True)

    tmp = SVG_OUT / ".tmp"
    tmp.mkdir(exist_ok=True)

    for name, body in toolbar_icons().items():
        if name in SIZE_OVERRIDES:
            size = SIZE_OVERRIDES[name]
        elif name in SMALL_GLYPHS:
            size = 32
        else:
            size = 64
        src = tmp / f"{name}.svg"
        src.write_text(body, encoding="utf-8")
        # The committed SVG sources are the reproducible originals.
        # 提交的 SVG 源文件是可复现的原始资源。
        (SVG_OUT / f"{name}.svg").write_text(body, encoding="utf-8")
        destination = output_path(name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ["rsvg-convert", "-w", str(size), "-h", str(size), str(src),
             "-o", str(destination)],
            check=True,
        )
        if name in DUAL_ICONS:
            # e.g. the search glyph is used both as a toolbar icon and as a
            # themed glyph, so the light-background copy lives in light/.
            # 例如 search 既是工具栏图标又是主题字形，浅色背景版本另存一份。
            subprocess.run(["cp", str(destination),
                            str(GLYPHS_LIGHT / name)], check=True)

    # The app icon uses a larger canvas; rsvg preserves the 64-unit viewBox.
    app_svg = tmp / "app-icon.svg"
    app_svg.write_text(APP_ICON, encoding="utf-8")
    app_png = ICONS / "app-icon.png"
    subprocess.run(
        ["rsvg-convert", "-w", "256", "-h", "256", str(app_svg), "-o", str(app_png)],
        check=True,
    )
    # Linux icon themes look for several nominal sizes.  Provide the common
    # hicolor sizes so launchers do not have to scale a single bitmap.
    # Linux 图标主题按常见尺寸查找；提供多种 hicolor 尺寸，避免启动器缩放模糊。
    for size in (512, 256, 128, 64, 48, 32):
        subprocess.run(
            ["rsvg-convert", "-w", str(size), "-h", str(size), str(app_svg),
             "-o", str(APP_ICONS / f"qarchiver-{size}.png")],
            check=True,
        )
    if shutil.which("magick"):
        subprocess.run(
            ["magick", str(app_png), "-define",
             "icon:auto-resize=256,128,64,48,32,16",
             str(APP_ICONS / "qarchiver.ico")],
            check=True,
        )

    # Remove the intermediate rendering directory; resources/svg/modern keeps
    # the committed SVG sources.
    # 删除渲染中间目录；resources/svg/modern 保留提交的 SVG 源文件。
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"Generated icons under {ICONS} and {RES / 'themes' / 'modern'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
