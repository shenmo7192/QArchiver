#!/usr/bin/env python3
"""Normalise the classic (Aero) icon sizes.

The classic set mixes toolbar glyphs and larger list icons.  Keeping one rule
avoids the 32/48/256 mix that crept in over time:

* toolbar / dialog glyphs -> 48x48 (the classic toolbar icon size)
* list icons (file, folder, home) -> 64x64 (crisp at 32 px on 2x displays)

``.ico`` files are multi-image containers and are left untouched, ``QIcon``
picks the closest size from them.  The script is idempotent: re-running it on
already normalised files rewrites identical pixels.

    python3 tools/normalize_classic_icons.py
"""

from __future__ import annotations

import sys
from pathlib import Path

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QImage

ROOT = Path(__file__).resolve().parent.parent
CLASSIC = ROOT / "resources" / "icons" / "classic"

# Toolbar and dialog glyphs.
GLYPH_SIZE = 48
GLYPHS = {
    "About.png", "Close.png", "Extract.png", "New.png", "Open.png",
    "Preferences.png", "Quit.png", "add.png", "test.png",
}
# Icons shown at list/grid size.
LIST_SIZE = 64
LIST_ICONS = {"file.png", "folder.png", "home.png"}


def resize(path: Path, size: int) -> bool:
    image = QImage(str(path))
    if image.isNull():
        raise RuntimeError(f"cannot read {path}")
    if image.width() == size and image.height() == size:
        return False
    scaled = image.scaled(
        size, size,
        Qt.AspectRatioMode.KeepAspectRatio,
        Qt.TransformationMode.SmoothTransformation,
    )
    if not scaled.save(str(path), "PNG"):
        raise RuntimeError(f"cannot write {path}")
    print(f"resized {path.name}: {image.width()}x{image.height()} -> {size}x{size}")
    return True


def main() -> int:
    changed = 0
    for name in sorted(GLYPHS):
        changed += resize(CLASSIC / name, GLYPH_SIZE)
    for name in sorted(LIST_ICONS):
        changed += resize(CLASSIC / name, LIST_SIZE)
    print(f"{changed} classic icon(s) normalised in {CLASSIC}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
