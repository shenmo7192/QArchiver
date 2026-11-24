#!/usr/bin/env python3
"""Verify that every embedded resource is reachable.

Checks, in order:

1. every ``*.qrc`` entry points at an existing file;
2. every ``:/...`` literal in the C++/QSS sources resolves to a resource alias
   (or shares the prefix of a resource directory, for the paths that are built
   at runtime from a directory constant);
3. the expected resource prefixes exist.

Run it after adding assets or changing ``IconUtils``/``ThemeManager`` paths:

    python3 tools/check_resources.py
"""

from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RESOURCES = ROOT / "resources"

# Prefixes the code relies on (see src/ui/icons/IconUtils.h and ThemeManager).
# 代码依赖的资源前缀（见 src/ui/icons/IconUtils.h 与 ThemeManager）。
EXPECTED_PREFIXES = {
    "/icons/classic/",
    "/icons/modern/",
    "/icons/file-types/",
    "/icons/file-types/classic/",
    "/icons/file-types/modern/",
    "/themes/aero/",
    "/themes/modern/",
}


def load_aliases() -> dict[str, Path]:
    """Return {resource path: source file} for every manifest entry."""
    aliases: dict[str, Path] = {}
    for qrc in sorted(RESOURCES.glob("*.qrc")):
        tree = ET.parse(qrc)
        for resource in tree.getroot().findall("qresource"):
            prefix = resource.get("prefix", "")
            for entry in resource.findall("file"):
                alias = entry.get("alias") or entry.text
                path = "/" + prefix.strip("/") + "/" + alias.strip("/")
                source = (qrc.parent / (entry.text or "")).resolve()
                aliases[path] = source
    return aliases


# String literals passed to the Icons:: helpers or local aliases, e.g.
# Icons::aero("Open.png") or modern("close.png") / classic("add.png").
# 传给 Icons:: 辅助函数的字面量，例如 Icons::aero("Open.png")。
ICON_CALL = re.compile(
    r'(?:Icons::)?(aero|modern|fileType|classic)\(QStringLiteral\("([^"]+)"\)\)')

ICON_ROOTS = {
    "aero": ("/icons/classic/",),
    "classic": ("/icons/classic/",),
    "modern": ("/icons/modern/",),
    "fileType": (
        "/icons/file-types/classic/",
        "/icons/file-types/modern/",
    ),
}


def collect_icon_calls() -> list[tuple[str, str]]:
    """Return [(helper, file name)] for every literal icon call."""
    calls: list[tuple[str, str]] = []
    for path in (ROOT / "src").rglob("*"):
        if path.suffix not in {".cpp", ".h"}:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        calls.extend(ICON_CALL.findall(text))
    return calls


def collect_references() -> set[str]:
    """Collect ``:/...`` literals from the C++ and QSS sources."""
    # C++ uses ":/path", Qt style sheets use url(:/path).
    # C++ 使用 ":/path"，Qt 样式表使用 url(:/path)。
    patterns = (re.compile(r'":(/[^"\n]+)"'), re.compile(r"url\(:(/[^)\s]+)\)"))
    references: set[str] = set()
    for pattern_root in ("src", "resources"):
        for path in (ROOT / pattern_root).rglob("*"):
            if path.suffix not in {".cpp", ".h", ".qss"}:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            for pattern in patterns:
                for match in pattern.finditer(text):
                    references.add(match.group(1))
    return references


def main() -> int:
    aliases = load_aliases()
    problems: list[str] = []

    # 1. Manifest entries must exist on disk.
    for resource, source in sorted(aliases.items()):
        if not source.is_file():
            problems.append(f"missing file for {resource}: {source}")

    # 2. Prefixes the code depends on.  Every directory that contains an alias
    #    counts, including nested ones such as /icons/file-types/modern/.
    prefixes: set[str] = set()
    for path in aliases:
        for parent in Path(path).parents:
            prefixes.add(str(parent).rstrip("/") + "/")
    for expected in EXPECTED_PREFIXES:
        if expected not in prefixes:
            problems.append(f"missing resource prefix {expected}")

    # 3. Referenced paths must resolve to an alias or to a directory prefix.
    for reference in sorted(collect_references()):
        if reference in aliases:
            continue
        if reference == "/translations":
            # QTranslator::load(name, ":/translations"); the catalogs are added
            # by translations/CMakeLists.txt, not by a .qrc in resources/.
            # 翻译资源由 translations/CMakeLists.txt 生成，不在 resources/ 清单中。
            continue
        # Built at runtime from a prefix constant (e.g. Icons::aero()).
        probe = reference if reference.endswith("/") else reference + "/"
        if any(probe.startswith(prefix) for prefix in EXPECTED_PREFIXES):
            if not any(alias.startswith(probe) for alias in aliases):
                problems.append(f"unresolved resource reference: {reference}")
            continue
        problems.append(f"unknown resource path: {reference}")

    # 4. Literal icon calls (Icons:: and local aliases) must point at a real alias.
    #    Icons::fileType() selects the active layout's set at runtime, so each
    #    literal must exist in both the classic and modern file-type roots.
    #    字面量图标调用必须指向真实别名；Icons::fileType() 在运行时按布局选择，
    #    因此每个文件名都必须同时存在于经典与现代文件类型目录。
    for helper, file_name in sorted(collect_icon_calls()):
        for root in ICON_ROOTS[helper]:
            resource = root + file_name
            if resource not in aliases:
                problems.append(f"missing icon: Icons::{helper}(\"{file_name}\")"
                                f" -> {resource}")

    # 5. Both themes must ship the same glyph names in light/ and dark/, so a
    #    theme switch only changes the directory, never the file name.
    #    两套主题的 light/ 与 dark/ 必须同名，切换主题只换目录不换文件名。
    for theme in sorted((RESOURCES / "themes").iterdir()):
        if not theme.is_dir():
            continue
        light = {f.name for f in (theme / "light").glob("*.png")}
        dark = {f.name for f in (theme / "dark").glob("*.png")}
        for missing in sorted(light ^ dark):
            problems.append(f"{theme.name}: '{missing}' exists on one side only "
                            f"(light/dark must match)")

    if problems:
        print("resource check FAILED:", file=sys.stderr)
        for problem in problems:
            print("  -", problem, file=sys.stderr)
        return 1

    print(f"resource check ok: {len(aliases)} entries, "
          f"{len(collect_references())} referenced paths, "
          f"{len(collect_icon_calls())} icon calls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
