// QArchiver / 小Q解压
// Archive folder tree, details form and navigation signals.
// 压缩包文件夹树、详细信息表单与导航信号。

#include "ui/icons/IconUtils.h"
#include "ui/widgets/ArchiveFolderPane.h"

#include "ui/icons/FileTypeHelper.h"
#include "utils/FileUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QAbstractItemView>
#include <QEvent>
#include <QFrame>
#include <QIcon>
#include <QSet>
#include <QShowEvent>
#include <QSize>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QString normalizedPath(const QString &path)
{
    return FileUtils::normalizeEntryPath(path);
}

QString sizeText(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}

QString ratioText(const ArchiveEntry &entry)
{
    if (entry.size <= 0 || entry.packedSize <= 0) {
        return QStringLiteral("-");
    }
    const double ratio = 100.0
        - (static_cast<double>(entry.packedSize) * 100.0
           / static_cast<double>(entry.size));
    return QStringLiteral("%1%").arg(ratio, 0, 'f', 1);
}

QLabel *makeValueLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("ModernDetailsValue"));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

} // namespace

ArchiveFolderPane::ArchiveFolderPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ModernFolderPane"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_header = new QLabel(tr("Folders"), this);
    m_header->setObjectName(QStringLiteral("ModernPanelHeader"));
    layout->addWidget(m_header);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("ModernFolderTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(14);
    m_tree->setIconSize(QSize(18, 18));
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setMinimumWidth(170);
    layout->addWidget(m_tree, 1);

    // Details area / 详细信息区域
    auto *details = new QWidget(this);
    m_detailsPane = details;
    details->setObjectName(QStringLiteral("ModernDetailsPane"));
    auto *detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(0, 0, 0, 6);
    detailsLayout->setSpacing(0);

    auto *detailsTitle = new QLabel(tr("Details"), details);
    detailsTitle->setObjectName(QStringLiteral("ModernDetailsTitle"));
    detailsLayout->addWidget(detailsTitle);

    m_detailsStack = new QStackedWidget(details);
    m_emptyLabel = new QLabel(tr("Select a file to view its details."),
                              m_detailsStack);
    m_emptyLabel->setObjectName(QStringLiteral("ModernDetailsValue"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setContentsMargins(12, 0, 12, 0);

    auto *formPage = new QWidget(m_detailsStack);
    auto *form = new QFormLayout(formPage);
    form->setContentsMargins(12, 2, 12, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(4);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const auto addRow = [form, formPage](const QString &label) {
        auto *key = new QLabel(label, formPage);
        key->setObjectName(QStringLiteral("ModernDetailsKey"));
        auto *value = makeValueLabel(formPage);
        form->addRow(key, value);
        return value;
    };
    m_nameValue = addRow(tr("Name:"));
    m_typeValue = addRow(tr("Type:"));
    m_sizeValue = addRow(tr("Size:"));
    m_packedValue = addRow(tr("Packed:"));
    m_ratioValue = addRow(tr("Ratio:"));
    m_modifiedValue = addRow(tr("Modified:"));
    m_pathValue = addRow(tr("Path:"));

    m_detailsStack->addWidget(m_emptyLabel);
    m_detailsStack->addWidget(formPage);
    m_detailsStack->setCurrentWidget(m_emptyLabel);
    detailsLayout->addWidget(m_detailsStack);
    details->setMinimumHeight(190);
    details->setMaximumHeight(260);
    layout->addWidget(details);

    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item) {
        if (item) {
            emit directoryActivated(
                item->data(0, Qt::UserRole).toString());
        }
    });
    connect(m_tree, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem *item) {
        if (item) {
            emit directoryActivated(
                item->data(0, Qt::UserRole).toString());
        }
    });
}

void ArchiveFolderPane::setEntries(const QList<ArchiveEntry> &entries)
{
    setLocalMode(false);
    m_entries = entries;
    rebuildTree();
    setSelection({});
}

void ArchiveFolderPane::setLocalMode(bool local)
{
    if (m_localMode == local) {
        return;
    }
    m_localMode = local;
    if (m_header) {
        m_header->setText(local ? tr("Places") : tr("Folders"));
    }
    if (m_detailsPane) {
        m_detailsPane->setVisible(!local);
    }
    if (m_tree) {
        m_tree->setRootIsDecorated(!local);
        // rebuildLocalPlaces() already clears the tree when entering local
        // mode; only the archive tree needs an explicit clear here.
        // 进入本地模式时 rebuildLocalPlaces() 已清空树，仅归档树需要显式清空。
        if (!local) {
            m_tree->clear();
        }
    }
    if (local) {
        rebuildLocalPlaces();
    }
}

void ArchiveFolderPane::rebuildLocalPlaces()
{
    if (!m_tree) {
        return;
    }
    const bool wasBlocked = m_tree->blockSignals(true);
    m_tree->clear();

    // Deduplicate by cleaned absolute path: XDG user directories may point at
    // the same folder (for example all at $HOME on minimal systems).
    // 按清理后的绝对路径去重：XDG 用户目录可能指向同一文件夹（如精简系统上
    // 全部回退到 $HOME）。
    QSet<QString> seen;
    const auto addPlace = [this, &seen](const QString &label, const QString &path,
                                        const QString &icon = QStringLiteral("folder.png")) {
        const QFileInfo info(path);
        if (!info.exists() || !info.isDir()) {
            return;
        }
        const QString cleaned = QDir::cleanPath(info.absoluteFilePath());
        if (seen.contains(cleaned)) {
            return;
        }
        seen.insert(cleaned);
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, label);
        item->setIcon(0, Icons::fileType(icon));
        item->setData(0, Qt::UserRole, cleaned);
        item->setToolTip(0, info.absoluteFilePath());
    };

    addPlace(tr("Home"), QDir::homePath());
    addPlace(tr("Desktop"), QStandardPaths::writableLocation(
                                QStandardPaths::DesktopLocation));
    addPlace(tr("Documents"), QStandardPaths::writableLocation(
                                  QStandardPaths::DocumentsLocation));
    addPlace(tr("Downloads"), QStandardPaths::writableLocation(
                                  QStandardPaths::DownloadLocation));
    addPlace(tr("Pictures"), QStandardPaths::writableLocation(
                                 QStandardPaths::PicturesLocation));
    addPlace(tr("Music"), QStandardPaths::writableLocation(
                              QStandardPaths::MusicLocation));
    addPlace(tr("Videos"), QStandardPaths::writableLocation(
                               QStandardPaths::MoviesLocation));
    addPlace(tr("File System"), QDir::rootPath());

    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : volumes) {
        if (!volume.isValid() || !volume.isReady()) {
            continue;
        }
        const QString path = QDir::cleanPath(volume.rootPath());
        if (path.isEmpty() || seen.contains(path)) {
            continue;
        }
        // Hide pseudo filesystems and system-internal mounts; keep the root
        // and user-facing removable/extra volumes.
        // 隐藏伪文件系统与系统内部挂载，只保留根目录和用户可见的磁盘/外接卷。
#ifdef Q_OS_WIN
        if (path == QDir::rootPath()) {
            continue;
        }
#else
        const bool userVolume =
            path.startsWith(QStringLiteral("/media/"))
            || path.startsWith(QStringLiteral("/mnt/"))
            || path.startsWith(QStringLiteral("/run/media/"));
        if (!userVolume) {
            continue;
        }
#endif
        seen.insert(path);
        auto *item = new QTreeWidgetItem(m_tree);
        const QString name = volume.displayName().isEmpty()
            ? path : volume.displayName();
        item->setText(0, name);
        item->setIcon(0, Icons::fileType(QStringLiteral("folder.png")));
        item->setData(0, Qt::UserRole, path);
        item->setToolTip(0, path);
    }

    m_tree->setRootIsDecorated(false);
    m_tree->blockSignals(wasBlocked);
    m_placesRefreshTimer.start();
}

void ArchiveFolderPane::refreshLocalPlacesIfNeeded()
{
    if (!m_localMode || !m_tree) {
        return;
    }
    // QStorageInfo::mountedVolumes() can touch network mounts; never run it on
    // every directory switch.
    // mountedVolumes() 可能触碰网络挂载，不能在每次切换目录时都执行。
    constexpr qint64 kRefreshIntervalMs = 5000;
    if (m_placesRefreshTimer.isValid()
        && m_placesRefreshTimer.elapsed() < kRefreshIntervalMs) {
        return;
    }
    rebuildLocalPlaces();
}

void ArchiveFolderPane::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_localMode || !m_tree) {
        return;
    }
    const QString current = m_tree->currentItem()
        ? m_tree->currentItem()->data(0, Qt::UserRole).toString()
        : QString();
    refreshLocalPlacesIfNeeded();
    if (!current.isEmpty()) {
        setCurrentLocalPath(current);
    }
}

void ArchiveFolderPane::setCurrentLocalPath(const QString &path)
{
    if (!m_localMode || !m_tree) {
        return;
    }
    const QString target = QDir::cleanPath(path);

    // Prefer the deepest place that contains the target; navigating into a
    // subdirectory should still highlight its place instead of leaving the
    // previous selection behind.  Paths compare case-insensitively on Windows.
    // 选中最深的包含该目录的位置；进入子目录时仍高亮所属位置，而不是残留上一
    // 个选中项。Windows 下路径比较忽略大小写。
    const auto findPlace = [this](const QString &wanted) -> QTreeWidgetItem * {
#ifdef Q_OS_WIN
        const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
        QTreeWidgetItem *best = nullptr;
        int bestLength = -1;
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_tree->topLevelItem(i);
            const QString place = QDir::cleanPath(
                item->data(0, Qt::UserRole).toString());
            if (place.isEmpty()) {
                continue;
            }
            const bool equal = wanted.compare(place, cs) == 0;
            // A place that is itself a root ("C:/" or "/") already ends with
            // a separator, so any absolute path below it counts as inside.
            // 位置本身就是根目录（"C:/" 或 "/"）时已以分隔符结尾，其下任意
            // 绝对路径都算包含于该位置。
            const bool inside = wanted.size() > place.size()
                && wanted.startsWith(place, cs)
                && (place.endsWith(QLatin1Char('/'))
                    || wanted.at(place.size()) == QLatin1Char('/'));
            if ((equal || inside) && place.size() > bestLength) {
                best = item;
                bestLength = place.size();
            }
        }
        return best;
    };

    QTreeWidgetItem *match = findPlace(target);
    // Matching only the file-system root is not specific enough: the directory
    // may be on a volume mounted after the list was built.  Refresh on demand,
    // throttled.  Paths inside Home/Desktop/etc. skip the refresh entirely.
    // 仅匹配到文件系统根说明位置不够具体：目录可能位于列表构建后才挂载的卷上，
    // 按需限频刷新；位于主目录/桌面等具体位置下的路径则完全跳过刷新。
    const bool onlyRootMatch = match
        && QDir::cleanPath(match->data(0, Qt::UserRole).toString())
               == QDir::cleanPath(QDir::rootPath())
        && target != QDir::cleanPath(QDir::rootPath());
    if (!match || onlyRootMatch) {
        refreshLocalPlacesIfNeeded();
        match = findPlace(target);
    }

    const bool wasBlocked = m_tree->blockSignals(true);
    if (match) {
        m_tree->setCurrentItem(match);
        m_tree->scrollToItem(match);
    } else {
        m_tree->setCurrentItem(nullptr);
    }
    m_tree->blockSignals(wasBlocked);
}

void ArchiveFolderPane::clear()
{
    m_entries.clear();
    if (m_tree) {
        m_tree->clear();
    }
    setSelection({});
}

QStringList ArchiveFolderPane::directoryPaths() const
{
    QSet<QString> paths;
    for (const ArchiveEntry &entry : m_entries) {
        QString path = entry.path;
        while (path.endsWith(QLatin1Char('/'))) {
            path.chop(1);
        }
        path = normalizedPath(path);
        if (path.isEmpty() || path == QStringLiteral(".")) {
            continue;
        }
        if (entry.isDir) {
            paths.insert(path);
        }
        const QStringList components = path.split(QLatin1Char('/'),
                                                  Qt::SkipEmptyParts);
        QString ancestor;
        for (int i = 0; i + 1 < components.size(); ++i) {
            ancestor = ancestor.isEmpty()
                ? components.at(i)
                : ancestor + QLatin1Char('/') + components.at(i);
            paths.insert(ancestor);
        }
    }
    QStringList sorted = paths.values();
    sorted.sort(Qt::CaseInsensitive);
    return sorted;
}

void ArchiveFolderPane::rebuildTree()
{
    if (!m_tree) {
        return;
    }
    const bool wasBlocked = m_tree->blockSignals(true);
    m_tree->clear();

    auto *root = new QTreeWidgetItem(m_tree);
    root->setText(0, tr("Archive root"));
    root->setIcon(0, Icons::fileType(QStringLiteral("folder.png")));
    root->setData(0, Qt::UserRole, QString());

    QMap<QString, QTreeWidgetItem *> items;
    items.insert(QString(), root);

    const QStringList directories = directoryPaths();
    for (const QString &path : directories) {
        const int slash = path.lastIndexOf(QLatin1Char('/'));
        const QString parentPath = slash < 0 ? QString() : path.left(slash);
        const QString name = slash < 0 ? path : path.mid(slash + 1);
        QTreeWidgetItem *parent = items.value(parentPath, root);
        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, name);
        item->setIcon(0, Icons::fileType(QStringLiteral("folder.png")));
        item->setData(0, Qt::UserRole, path);
        item->setToolTip(0, path);
        items.insert(path, item);
    }

    root->setExpanded(true);
    // Expand the first level so the archive structure is visible without
    // forcing every large archive open.  展开第一层，避免大型压缩包一次性展开。
    for (int i = 0; i < root->childCount(); ++i) {
        root->child(i)->setExpanded(true);
    }
    m_tree->blockSignals(wasBlocked);
}

void ArchiveFolderPane::setCurrentDirectory(const QString &directory)
{
    if (!m_tree) {
        return;
    }
    if (m_localMode) {
        setCurrentLocalPath(directory);
        return;
    }
    const QString target = normalizedPath(directory);
    QTreeWidgetItem *root = m_tree->topLevelItem(0);
    if (!root) {
        return;
    }

    QTreeWidgetItem *match = root;
    if (!target.isEmpty()) {
        QList<QTreeWidgetItem *> found = m_tree->findItems(
            QFileInfo(target).fileName(), Qt::MatchExactly | Qt::MatchRecursive);
        for (QTreeWidgetItem *item : found) {
            if (item->data(0, Qt::UserRole).toString() == target) {
                match = item;
                break;
            }
        }
    }

    const bool wasBlocked = m_tree->blockSignals(true);
    for (QTreeWidgetItem *parent = match->parent(); parent;
         parent = parent->parent()) {
        parent->setExpanded(true);
    }
    m_tree->setCurrentItem(match);
    m_tree->scrollToItem(match);
    m_tree->blockSignals(wasBlocked);
}

void ArchiveFolderPane::setSelection(const QList<ArchiveEntry> &entries)
{
    if (!m_detailsStack) {
        return;
    }

    if (entries.isEmpty()) {
        m_detailsStack->setCurrentWidget(m_emptyLabel);
        return;
    }

    if (entries.size() > 1) {
        qint64 total = 0;
        qint64 packed = 0;
        for (const ArchiveEntry &entry : entries) {
            if (entry.isDir) {
                continue;
            }
            total += entry.size;
            packed += entry.packedSize;
        }
        m_nameValue->setText(tr("%1 items selected").arg(entries.size()));
        m_typeValue->setText(tr("Multiple"));
        m_sizeValue->setText(sizeText(total));
        m_packedValue->setText(sizeText(packed));
        m_ratioValue->setText(QStringLiteral("-"));
        m_modifiedValue->setText(QStringLiteral("-"));
        m_pathValue->setText(tr("Multiple locations"));
        m_detailsStack->setCurrentIndex(1);
        return;
    }

    const ArchiveEntry &entry = entries.first();
    QString path = entry.path;
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    const QString normalized = normalizedPath(path);
    const QString name = QFileInfo(normalized).fileName();

    m_nameValue->setText(name.isEmpty() ? normalized : name);
    m_typeValue->setText(FileTypeHelper::description(name, entry.isDir));
    m_sizeValue->setText(entry.isDir ? QStringLiteral("-") : sizeText(entry.size));
    m_packedValue->setText(entry.isDir ? QStringLiteral("-")
                                       : sizeText(entry.packedSize));
    m_ratioValue->setText(entry.isDir ? QStringLiteral("-") : ratioText(entry));
    m_modifiedValue->setText(entry.mtime.isValid()
        ? entry.mtime.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QStringLiteral("-"));
    m_pathValue->setText(normalized);
    m_detailsStack->setCurrentIndex(1);
}
