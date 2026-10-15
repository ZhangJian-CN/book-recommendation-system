#ifndef LIBRARYSTORE_H
#define LIBRARYSTORE_H

#include "bookrepository.h"

//这两个文件构成系统的业务管理层：统一管理图书、用户、评分和借阅数据，检查业务规则，并负责将操作结果保存到文件。
//头文件说明业务层管理哪些数据，以及界面可以调用哪些操作。

// 四类实体分别对应四份业务 CSV；用户信息当前通过文件加载。
struct User { QString id; QString name; };
// 每个用户和图书组合最多一条评分，重复提交会更新原评分。
struct Rating { QString userId; QString bookId; int score; };
struct BorrowRecord {
    QString id;
    QString userId;
    QString bookId;
    QString borrowDate;
    QString returnDate;
    QString status;
};

// 业务层统一维护引用约束、内存修改和文件保存，界面通过此接口操作数据。
class LibraryStore {
public:
    // dataDir 含四份约定 CSV；成功后整体替换数据，失败原因写入 error。
    bool load(const QString &dataDir, QString *error);
    const BookRepository &books() const { return books_; }
    const QVector<User> &users() const { return users_; }
    const QVector<Rating> &ratings() const { return ratings_; }
    const QVector<BorrowRecord> &borrowRecords() const { return borrowRecords_; }
    bool hasUser(const QString &id) const;
    bool addBook(const Book &book, QString *error);
    bool updateBook(const Book &book, QString *error);
    bool deleteBook(const QString &id, QString *error);
    // userId/bookId 必须存在，score 为 1～5；成功表示评分已保存到文件。
    bool rate(const QString &userId, const QString &bookId, int score, QString *error);
    bool borrowBook(const QString &userId, const QString &bookId,
                    const QString &borrowDate, QString *recordId, QString *error);
    bool returnBook(const QString &recordId, const QString &returnDate, QString *error);

private:
    bool saveRatings(QString *error) const;
    QString dataDir_;
    BookRepository books_;
    QVector<User> users_;
    QVector<Rating> ratings_;
    QVector<BorrowRecord> borrowRecords_;
};

#endif // LIBRARYSTORE_H
