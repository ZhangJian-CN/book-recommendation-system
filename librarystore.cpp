#include "librarystore.h"
#include "csvutil.h"

#include <QDate>
#include <QDir>
#include <QSet>

namespace {
bool validId(const QString &id)
{
    return !id.isEmpty() && id == id.trimmed();
}

}

bool LibraryStore::load(const QString &dataDir, QString *error)
{
    const QDir dir(dataDir);
    BookRepository books;
    if (!books.load(dir.filePath(QStringLiteral("books.csv")), error))
        return false;

    QVector<QStringList> rows;
    if (!Csv::read(dir.filePath(QStringLiteral("users.csv")),
                   QStringLiteral("user_id,name"), 2, &rows, error))
        return false;
    QVector<User> users;
    QSet<QString> userIds;
    for (int i = 0; i < rows.size(); ++i) {
        const User user{rows[i][0], rows[i][1]};
        if (!validId(user.id) || user.name.trimmed().isEmpty()
            || userIds.contains(user.id)) {
            *error = QStringLiteral("users.csv 第 %1 行数据无效或编号重复").arg(i + 2);
            return false;
        }
        userIds.insert(user.id);
        users.append(user);
    }

    if (!Csv::read(dir.filePath(QStringLiteral("ratings.csv")),
                   QStringLiteral("user_id,book_id,score"), 3, &rows, error))
        return false;
    QVector<Rating> ratings;
    QSet<QString> ratingKeys;
    for (int i = 0; i < rows.size(); ++i) {
        bool scoreOk = false;
        const int score = rows[i][2].toInt(&scoreOk);
        const Rating rating{rows[i][0], rows[i][1], score};
        const QString key = rating.userId + QChar(0x1f) + rating.bookId;
        if (!userIds.contains(rating.userId) || !books.findById(rating.bookId)
            || !scoreOk || score < 1 || score > 5 || ratingKeys.contains(key)) {
            *error = QStringLiteral("ratings.csv 第 %1 行引用、评分或唯一性错误").arg(i + 2);
            return false;
        }
        ratingKeys.insert(key);
        ratings.append(rating);
    }

    if (!Csv::read(dir.filePath(QStringLiteral("borrow_records.csv")),
                   QStringLiteral("record_id,user_id,book_id,borrow_date,return_date,status"),
                   6, &rows, error))
        return false;
    QVector<BorrowRecord> records;
    QSet<QString> recordIds;
    for (int i = 0; i < rows.size(); ++i) {
        const BorrowRecord record{rows[i][0], rows[i][1], rows[i][2],
                                  rows[i][3], rows[i][4], rows[i][5]};
        const QDate borrowed = QDate::fromString(record.borrowDate, Qt::ISODate);
        const QDate returned = QDate::fromString(record.returnDate, Qt::ISODate);
        const bool datesOk = record.status == QStringLiteral("borrowed")
                                 ? record.returnDate.isEmpty()
                                 : record.status == QStringLiteral("returned")
                                       && returned.isValid() && returned >= borrowed;
        if (!validId(record.id) || recordIds.contains(record.id)
            || !userIds.contains(record.userId) || !books.findById(record.bookId)
            || !borrowed.isValid() || !datesOk) {
            *error = QStringLiteral("borrow_records.csv 第 %1 行引用、日期或状态错误").arg(i + 2);
            return false;
        }
        recordIds.insert(record.id);
        records.append(record);
    }

    dataDir_ = dir.absolutePath();
    books_ = books;
    users_ = users;
    ratings_ = ratings;
    borrowRecords_ = records;
    return true;
}
bool LibraryStore::addBook(const Book &book, QString *error)
{
    const BookRepository before = books_;
    if (!books_.insert(book, error))
        return false;
    if (!books_.save(QDir(dataDir_).filePath(QStringLiteral("books.csv")), error)) {
        books_ = before;
        return false;
    }
    return true;
}
bool LibraryStore::updateBook(const Book &book, QString *error)
{
    const BookRepository before = books_;
    if (!books_.update(book, error))
        return false;
    if (!books_.save(QDir(dataDir_).filePath(QStringLiteral("books.csv")), error)) {
        books_ = before;
        return false;
    }
    return true;
}

bool LibraryStore::deleteBook(const QString &id, QString *error)
{
    for (const Rating &rating : ratings_) {
        if (rating.bookId == id) {
            *error = QStringLiteral("图书 %1 有评分记录，不能删除").arg(id);
            return false;
        }
    }
    for (const BorrowRecord &record : borrowRecords_) {
        if (record.bookId == id) {
            *error = QStringLiteral("图书 %1 有借阅记录，不能删除（含已归还记录）").arg(id);
            return false;
        }
    }
    const BookRepository before = books_;
    if (!books_.removeById(id, error))
        return false;
    if (!books_.save(QDir(dataDir_).filePath(QStringLiteral("books.csv")), error)) {
        books_ = before;
        return false;
    }
    return true;
}
