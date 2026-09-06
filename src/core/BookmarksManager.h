#ifndef BOOKMARKSMANAGER_H
#define BOOKMARKSMANAGER_H

#include <QObject>
#include <QStringList>

// Manages the bookmarks (favorites) list (design doc 5.9.2)
class BookmarksManager : public QObject {
    Q_OBJECT
public:
    static BookmarksManager &instance();

    void add(const QString &filePath);
    void remove(const QString &filePath);
    QStringList list() const;
    bool contains(const QString &filePath) const;

signals:
    void changed();

private:
    BookmarksManager();
    Q_DISABLE_COPY(BookmarksManager)
};

#endif // BOOKMARKSMANAGER_H
