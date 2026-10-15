#include "librarystore.h"
#include "csvutil.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <limits>

namespace {
bool validId(const QString &id)
{
    return !id.isEmpty() && id == id.trimmed();
}

QString ratingKey(const QString &userId, const QString &bookId)
{
    return userId + QChar(0x1f) + bookId;
}

QString markerPath(const QDir &dir)
{
    return dir.filePath(QStringLiteral(".borrow_transaction"));
}

QString backupPath(const QDir &dir, const QString &name)
{
    return dir.filePath(QStringLiteral(".") + name + QStringLiteral(".borrow_backup"));
}

bool writeBytes(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
        || !file.commit()) {
        *error = QStringLiteral("无法写入 %1：%2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool restoreBackup(const QDir &dir, const QString &name, QString *error)
{
    QFile backup(backupPath(dir, name));
    if (!backup.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("借阅事务备份缺失或无法读取：%1").arg(backup.fileName());
        return false;
    }
    const QByteArray original = backup.readAll();
    QFile current(dir.filePath(name));
    if (current.open(QIODevice::ReadOnly) && current.readAll() == original)
        return true;
    current.close();
    return writeBytes(dir.filePath(name), original, error);
}

void clearBorrowTransaction(const QDir &dir)
{
    const QString booksBackup = backupPath(dir, QStringLiteral("books.csv"));
    const QString recordsBackup = backupPath(dir, QStringLiteral("borrow_records.csv"));
    const bool booksRemoved = !QFile::exists(booksBackup) || QFile::remove(booksBackup);
    const bool recordsRemoved = !QFile::exists(recordsBackup) || QFile::remove(recordsBackup);
    if (booksRemoved && recordsRemoved)
        QFile::remove(markerPath(dir));
}

bool recoverBorrowTransaction(const QDir &dir, QString *error)
{
    QFile marker(markerPath(dir));
    if (!marker.exists())
        return true;
    if (!marker.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("无法读取借阅事务标记：%1").arg(marker.errorString());
        return false;
    }
    const QByteArray state = marker.readAll();
    marker.close();
    if (state == "pending") {
        if (!restoreBackup(dir, QStringLiteral("books.csv"), error)
            || !restoreBackup(dir, QStringLiteral("borrow_records.csv"), error))
            return false;
    } else if (state != "committed") {
        *error = QStringLiteral("借阅事务标记内容无效，请检查数据目录");
        return false;
    }
    clearBorrowTransaction(dir);
    return true;
}

bool saveBorrowRecords(const QDir &dir, const QVector<BorrowRecord> &records,
                       QString *error)
{
    QVector<QStringList> rows;
    for (const BorrowRecord &record : records) {
        rows.append({record.id, record.userId, record.bookId, record.borrowDate,
                     record.returnDate, record.status});
    }
    return Csv::write(dir.filePath(QStringLiteral("borrow_records.csv")),
                      QStringLiteral("record_id,user_id,book_id,borrow_date,return_date,status"),
                      rows, error);
}

bool saveBorrowAndBooks(const QDir &dir, const BookRepository &books,
                        const QVector<BorrowRecord> &records, QString *error)
{
    if (!recoverBorrowTransaction(dir, error))
        return false;
    const QString booksName = QStringLiteral("books.csv");
    const QString recordsName = QStringLiteral("borrow_records.csv");
    QFile::remove(backupPath(dir, booksName));
    QFile::remove(backupPath(dir, recordsName));
    if (!QFile::copy(dir.filePath(booksName), backupPath(dir, booksName))
        || !QFile::copy(dir.filePath(recordsName), backupPath(dir, recordsName))) {
        *error = QStringLiteral("无法备份图书或借阅记录，未写入更改");
        return false;
    }
    if (!writeBytes(markerPath(dir), "pending", error))
        return false;
    if (!books.save(dir.filePath(booksName), error)
        || !saveBorrowRecords(dir, records, error)
        || !writeBytes(markerPath(dir), "committed", error)) {
        const QString failure = *error;
        QString recoveryError;
        if (!recoverBorrowTransaction(dir, &recoveryError)) {
            *error = failure + QStringLiteral("；恢复未完成：") + recoveryError;
        } else {
            *error = failure + QStringLiteral("；原数据已恢复");
        }
        return false;
    }
    clearBorrowTransaction(dir);
    return true;
}
}

bool LibraryStore::load(const QString &dataDir, QString *error)
{
    const QDir dir(dataDir);
    if (!recoverBorrowTransaction(dir, error))
        return false;
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
        const QString key = ratingKey(rating.userId, rating.bookId);
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
    QSet<QString> activeLoans;
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
            || !borrowed.isValid() || !datesOk
            || (record.status == QStringLiteral("borrowed")
                && activeLoans.contains(ratingKey(record.userId, record.bookId)))) {
            *error = QStringLiteral("borrow_records.csv 第 %1 行引用、日期、状态或重复借阅错误").arg(i + 2);
            return false;
        }
        recordIds.insert(record.id);
        if (record.status == QStringLiteral("borrowed"))
            activeLoans.insert(ratingKey(record.userId, record.bookId));
        records.append(record);
    }

    dataDir_ = dir.absolutePath();
    books_ = books;
    users_ = users;
    ratings_ = ratings;
    borrowRecords_ = records;
    return true;
}

bool LibraryStore::hasUser(const QString &id) const
{
    for (const User &user : users_) {
        if (user.id == id)
            return true;
    }
    return false;
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

bool LibraryStore::saveRatings(QString *error) const
{
    QVector<QStringList> rows;
    for (const Rating &rating : ratings_)
        rows.append({rating.userId, rating.bookId, QString::number(rating.score)});
    return Csv::write(QDir(dataDir_).filePath(QStringLiteral("ratings.csv")),
                      QStringLiteral("user_id,book_id,score"), rows, error);
}

bool LibraryStore::rate(const QString &userId, const QString &bookId,
                        int score, QString *error)
{
    if (!hasUser(userId) || !books_.findById(bookId) || score < 1 || score > 5) {
        *error = QStringLiteral("用户、图书不存在或评分不在 1～5 范围内");
        return false;
    }
    const QVector<Rating> before = ratings_;
    bool found = false;
    for (Rating &rating : ratings_) {
        if (rating.userId == userId && rating.bookId == bookId) {
            rating.score = score;
            found = true;
            break;
        }
    }
    if (!found)
        ratings_.append({userId, bookId, score});
    if (!saveRatings(error)) {
        ratings_ = before;
        return false;
    }
    return true;
}

bool LibraryStore::borrowBook(const QString &userId, const QString &bookId,
                              const QString &borrowDate, QString *recordId, QString *error)
{
    const QDate date = QDate::fromString(borrowDate, Qt::ISODate);
    const Book *current = books_.findById(bookId);
    if (!hasUser(userId) || !current) {
        *error = QStringLiteral("用户或图书编号不存在");
        return false;
    }
    if (!date.isValid() || date.toString(Qt::ISODate) != borrowDate) {
        *error = QStringLiteral("借出日期必须是有效的 YYYY-MM-DD");
        return false;
    }
    if (current->stock <= 0) {
        *error = QStringLiteral("图书 %1 库存为零，不能借出").arg(bookId);
        return false;
    }
    quint64 highest = 0;
    int width = 3;
    for (const BorrowRecord &record : borrowRecords_) {
        if (record.userId == userId && record.bookId == bookId
            && record.status == QStringLiteral("borrowed")) {
            *error = QStringLiteral("该用户已有这本书的未归还记录");
            return false;
        }
        if (record.id.startsWith('R') && record.id.size() > 1) {
            const QString digits = record.id.mid(1);
            bool ok = false;
            const quint64 number = digits.toULongLong(&ok);
            if (ok) {
                highest = qMax(highest, number);
                width = qMax(width, int(digits.size()));
            }
        }
    }
    if (highest == std::numeric_limits<quint64>::max()) {
        *error = QStringLiteral("借阅编号已用尽");
        return false;
    }
    const QString newId = QStringLiteral("R")
                          + QString::number(highest + 1).rightJustified(width, '0');
    const BookRepository booksBefore = books_;
    const QVector<BorrowRecord> recordsBefore = borrowRecords_;
    Book updated = *current;
    --updated.stock;
    books_.update(updated, error);
    borrowRecords_.append({newId, userId, bookId, borrowDate, {}, QStringLiteral("borrowed")});
    if (!saveBorrowAndBooks(QDir(dataDir_), books_, borrowRecords_, error)) {
        books_ = booksBefore;
        borrowRecords_ = recordsBefore;
        return false;
    }
    if (recordId)
        *recordId = newId;
    return true;
}

bool LibraryStore::returnBook(const QString &recordId, const QString &returnDate,
                              QString *error)
{
    const QDate date = QDate::fromString(returnDate, Qt::ISODate);
    if (!date.isValid() || date.toString(Qt::ISODate) != returnDate) {
        *error = QStringLiteral("归还日期必须是有效的 YYYY-MM-DD");
        return false;
    }
    for (int i = 0; i < borrowRecords_.size(); ++i) {
        const BorrowRecord &record = borrowRecords_[i];
        if (record.id != recordId)
            continue;
        if (record.status != QStringLiteral("borrowed")) {
            *error = QStringLiteral("借阅记录 %1 已归还，不能重复归还").arg(recordId);
            return false;
        }
        if (date < QDate::fromString(record.borrowDate, Qt::ISODate)) {
            *error = QStringLiteral("归还日期不能早于借出日期");
            return false;
        }
        const Book *current = books_.findById(record.bookId);
        if (!current || current->stock == std::numeric_limits<int>::max()) {
            *error = QStringLiteral("图书不存在或库存无法增加");
            return false;
        }
        const BookRepository booksBefore = books_;
        const QVector<BorrowRecord> recordsBefore = borrowRecords_;
        Book updated = *current;
        ++updated.stock;
        books_.update(updated, error);
        borrowRecords_[i].returnDate = returnDate;
        borrowRecords_[i].status = QStringLiteral("returned");
        if (!saveBorrowAndBooks(QDir(dataDir_), books_, borrowRecords_, error)) {
            books_ = booksBefore;
            borrowRecords_ = recordsBefore;
            return false;
        }
        return true;
    }
    *error = QStringLiteral("借阅记录 %1 不存在").arg(recordId);
    return false;
}
