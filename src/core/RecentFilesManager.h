#ifndef RECENTFILESMANAGER_H
#define RECENTFILESMANAGER_H

#include <QObject>
#include <QStringList>

// Manages the recently-opened archives list (design doc 5.9.1)
class RecentFilesManager : public QObject {
    Q_OBJECT
public:
    static RecentFilesManager &instance();

    void add(const QString &filePath);
    void remove(const QString &filePath);
    QStringList list() const;
    void clear();
    // Remove entries that no longer exist on disk
    QStringList validatedList() const;

signals:
    void changed();

private:
    RecentFilesManager();
    Q_DISABLE_COPY(RecentFilesManager)
};

#endif // RECENTFILESMANAGER_H
