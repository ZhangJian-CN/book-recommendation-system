#ifndef LIBRARYSTORE_H
#define LIBRARYSTORE_H

#include "bookrepository.h"

struct User { QString id; QString name; };
struct Rating { QString userId; QString bookId; int score; };
struct BorrowRecord {
    QString id;
    QString userId;
    QString bookId;
    QString borrowDate;
    QString returnDate;
    QString status;
};

class LibraryStore {
public:
    bool load(const QString &dataDir, QString *error);
    const BookRepository &books() const { return books_; }
    const QVector<User> &users() const { return users_; }
    const QVector<Rating> &ratings() const { return ratings_; }
    const QVector<BorrowRecord> &borrowRecords() const { return borrowRecords_; }
    bool hasUser(const QString &id) const;
    bool addBook(const Book &book, QString *error);
    bool updateBook(const Book &book, QString *error);
    bool deleteBook(const QString &id, QString *error);
    bool rate(const QString &userId, const QString &bookId, int score, QString *error);

private:
    bool saveRatings(QString *error) const;
    QString dataDir_;
    BookRepository books_;
    QVector<User> users_;
    QVector<Rating> ratings_;
    QVector<BorrowRecord> borrowRecords_;
};

#endif // LIBRARYSTORE_H
