#ifndef BOOKREPOSITORY_H
#define BOOKREPOSITORY_H

#include <QString>
#include <QVector>

struct Book {
    QString id;
    QString title;
    QString author;
    QString category;
    int stock = 0;
};

class BookRepository {
public:
    bool load(const QString &path, QString *error);
    bool save(const QString &path, QString *error) const;
    const QVector<Book> &all() const { return books_; }
    const Book *findById(const QString &id) const;
    bool update(const Book &book, QString *error);

private:
    QVector<Book> books_;
};

#endif // BOOKREPOSITORY_H
