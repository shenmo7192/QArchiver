#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <memory>

#include "ArchiveConstants.h"

class QSettings;

// Singleton wrapping QSettings for global preferences (design doc 4.6)
class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings &instance();
    ~AppSettings() override;

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
    QString theme() const;
    QString contentViewMode() const;   // "tree" or "list"
    QString toolButtonStyle() const;  // textUnderIcon/textBesideIcon/textOnly/iconOnly

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
    void setTheme(const QString &theme);
    void setContentViewMode(const QString &mode);
    void setToolButtonStyle(const QString &style);

private:
    AppSettings();
    Q_DISABLE_COPY(AppSettings)

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    std::unique_ptr<QSettings> m_settings;
};

#endif // APPSETTINGS_H
