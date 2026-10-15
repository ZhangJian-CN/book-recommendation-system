#ifndef BOOKREPOSITORY_H
#define BOOKREPOSITORY_H

#include <QString>
#include <QVector>
#include "bookindex.h"

//借阅、评分记录存放

// 图书实体；stock 表示当前可借数量，借出减一、归还加一。
struct Book {
    QString id;
    QString title;
    QString author;
    QString category;
    int stock = 0;
};

// 图书仓库：动态数组保存完整实体，BookIndex 保存编号到数组下标的索引。
// 本层负责图书字段校验及内存增删改查，评分/借阅引用限制在 LibraryStore 检查。
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
    // books_ 保存实体，index_ 辅助查询；索引保存的下标必须与数组实际位置一致。
    QVector<Book> books_;
    BookIndex index_;
};

#endif // BOOKREPOSITORY_H
