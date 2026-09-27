#ifndef BOOKREPOSITORY_H
#define BOOKREPOSITORY_H

#include <QString>
#include <QVector>
#include "bookindex.h"

struct Book {
    QString id;
    QString title;
    QString author;
    QString category;
    int stock = 0;
};

class BookRepository {
public:
    explicit BookRepository(int bucketCount = 101) : index_(bucketCount) {}
    bool load(const QString &path, QString *error);
    bool save(const QString &path, QString *error) const;
    const QVector<Book> &all() const { return books_; }
    const Book *findById(const QString &id) const;
    bool insert(const Book &book, QString *error);
    bool update(const Book &book, QString *error);
    bool removeById(const QString &id, QString *error);

private:
    QVector<Book> books_;
    BookIndex index_;
};

#endif // BOOKREPOSITORY_H
