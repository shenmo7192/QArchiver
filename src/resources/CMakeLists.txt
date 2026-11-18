# Qt resource bundle (icons, toolbar images and embedded assets).
# Qt 资源包（图标、工具栏图片及嵌入资源）。
target_sources(QArchiver PRIVATE
    resources.qrc
)

# target_sources() does not enable AUTORCC by itself, so set it on the target.
# target_sources() 不会自动启用 AUTORCC，因此在目标上显式开启。
set_target_properties(QArchiver PROPERTIES AUTORCC ON)
