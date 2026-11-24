# Resource assets / 资源文件

All files in this directory are compiled into the executable by `rcc` through
the `*.qrc` manifests, so QArchiver ships as a single binary.  Only
`app-icons/` is kept out of the resource bundle: those files are used by the
installer, the desktop entry and the Windows `.rc` file.

本目录下的文件通过 `*.qrc` 清单由 `rcc` 编译进可执行文件，因此 QArchiver
以单个二进制分发。只有 `app-icons/` 不进入资源包：它们供安装规则、桌面入口
与 Windows `.rc` 使用。

## Layout / 目录结构

```text
resources/
├── app-icons/                  installed application icons (.png + .ico)
├── icons/
│   ├── classic/                classic (Aero) toolbar/file icons
│   ├── modern/                 modern toolbar and navigation icons
│   │   ├── app.png             in-app logo (small)
│   │   └── app-icon.png        in-app logo (window icon)
│   └── file-types/             file-type icons for the list/grid views
│       ├── classic/            classic (Aero) set
│       └── modern/             modern set
├── themes/
│   ├── aero/
│   │   ├── aero.qss
│   │   ├── *.png               shared bitmaps (Win7 checkbox/radio art)
│   │   ├── light/              glyphs for light backgrounds
│   │   └── dark/               the same glyph names for dark backgrounds
│   └── modern/
│       ├── modern.qss
│       ├── *.png               shared glyphs (check, minus, radio-dot)
│       ├── light/
│       └── dark/
├── svg/modern/                 committed SVG sources for the modern set
├── icons.qrc                   -> /icons/classic, /icons/modern, /icons/file-types/{classic,modern}
├── themes-aero.qrc             -> /themes/aero
└── themes-modern.qrc           -> /themes/modern
```

Both themes ship the **same glyph names** in `light/` and `dark/`, so switching
between light and dark never needs a renamed asset.  Glyphs that look identical
in both themes live at the theme root instead of in `light/`/`dark/`.
`Icons::themed()` (see `src/ui/icons/IconUtils.h`) is the only place that builds
these paths, and `tools/check_resources.py` verifies that `light/` and `dark/`
hold the same file names.

The two file-type sets use the same file names and only differ by directory, so
`Icons::fileType()` can switch sets without any name mapping.

两套主题在 `light/` 与 `dark/` 下使用**相同的字形名称**，因此切换浅色/深色
主题不需要重命名资源。路径拼接只发生在 `src/ui/icons/IconUtils.h` 的
`Icons::themed()` 中。

两套文件类型图标使用相同文件名，仅目录不同，因此 `Icons::fileType()`
切换布局时不需要维护额外的文件名映射。

## Generated assets / 生成资源

| Directory | Generator | Notes |
|---|---|---|
| `themes/aero/light`, `themes/aero/dark` | `tools/gen_aero_assets.py` | PyQt6 painter script; `dark/` is recolored from `light/` |
| `icons/modern`, `icons/file-types/modern`, `themes/modern/light`, `themes/modern/dark`, `app-icons/*.png|*.ico` | `tools/gen_modern_icons.py` | needs `rsvg-convert`; also writes `svg/modern/` |
| `*.qrc` | `tools/update_qrc.py` | run after adding, renaming or deleting assets |

The generated PNGs are committed, so no generator has to run for a normal
build.  Run the scripts only when the artwork itself changes, then re-run
`tools/update_qrc.py` and commit both the assets and the manifests.

生成的 PNG 已提交，正常构建无需运行生成脚本。仅在图形本身变化时运行脚本，
随后重新运行 `tools/update_qrc.py`，并同时提交资源与清单。
