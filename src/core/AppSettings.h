#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "ArchiveConstants.h"

// Singleton wrapping QSettings for global preferences (design doc 4.6)
class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings &instance();

    // Getters
    ArchiveFormat defaultCompressFormat() const;
    int defaultCompressionLevel() const;
    OverwritePolicy overwritePolicy() const;
    bool preservePaths() const;
    bool rememberPasswords() const;
    bool openDestAfterFinish() const;
    qint64 maxPreviewSize() const;
    bool followSymlinks() const;
    QStringList recentFiles() const;
    QStringList bookmarks() const;
    QString theme() const;

    // Setters (persist immediately)
    void setDefaultCompressFormat(ArchiveFormat fmt);
    void setDefaultCompressionLevel(int level);
    void setOverwritePolicy(OverwritePolicy policy);
    void setPreservePaths(bool preserve);
    void setRememberPasswords(bool remember);
    void setOpenDestAfterFinish(bool open);
    void setMaxPreviewSize(qint64 size);
    void setFollowSymlinks(bool follow);
    void setRecentFiles(const QStringList &files);
    void setBookmarks(const QStringList &bms);
    void setTheme(const QString &theme);

private:
    AppSettings();
    Q_DISABLE_COPY(AppSettings)
};

#endif // APPSETTINGS_H
