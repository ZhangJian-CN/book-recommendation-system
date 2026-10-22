#ifndef LIBRARYSTORE_H
#define LIBRARYSTORE_H

#include "bookrepository.h"
#include <QLockFile>
#include <memory>
//动态数组：保存完整图书、用户、评分和借阅记录，便于遍历、显示和保存
// 界面调用本层而不直接改仓库，这样“条件检查、内存变更、文件保存”才能作为一次业务处理。
// addBook/updateBook/deleteBook 连接图书仓库；rate 与借还操作是另外两页的业务入口。

// 四类实体分别对应四份业务 CSV；用户信息当前通过文件加载。
struct User { QString id; QString name; }; // struct 打包用户编号和显示名，对应 users.csv 的一条记录。
// 每个用户和图书组合最多一条评分，重复提交会更新原评分。
struct Rating { QString userId; QString bookId; int score; }; // 一条用户对图书的评分，合法分数为 1～5。
struct BorrowRecord { // 一条借阅历史；同一本书可有多次历史，每次由不同记录编号区分。
    QString id; // 借阅记录编号，如 R003。
    QString userId; // 引用用户编号。
    QString bookId; // 引用图书编号。
    QString borrowDate; // 借出日期。
    QString returnDate; // 归还日期；未归还时为空。
    QString status; // borrowed 或 returned，决定记录当前状态。
};

// 业务层统一维护引用约束、内存修改和文件保存，界面通过此接口操作数据。
class LibraryStore { // class 封装跨文件业务规则：统一组织图书、用户、评分、借阅及保存恢复。
public: // 公有接口供窗口调用；外部通过它们操作，避免绕过引用约束或持久化。
    // 首次复制样例也遵循同目录互斥；已有非空/部分数据交给 load 校验，不覆盖。
    static bool initializeDataDirectory(const QString &dataDir, QString *error);
    // dataDir 含四份约定 CSV；成功后整体替换数据，失败原因写入 error。
    bool load(const QString &dataDir, QString *error); // 按依赖顺序加载四份 CSV，检查字段、唯一性与引用，全部通过才更新成员。
    const BookRepository &books() const { return books_; } // 返回图书仓库只读引用，用于显示、检索和推荐输入。
    const QVector<User> &users() const { return users_; } // 返回当前用户数组的只读引用。
    const QVector<Rating> &ratings() const { return ratings_; } // 返回评分记录；推荐矩阵由服务根据这些记录另建。
    const QVector<BorrowRecord> &borrowRecords() const { return borrowRecords_; } // 返回借阅历史数组的只读引用。
    bool hasUser(const QString &id) const; // 判断用户编号是否存在，供评分与借还等接口校验。
    // 下列三个图书接口：返回 true 才表示操作和保存均成功；error 向界面解释拒绝或失败原因。
    // 删除还会检查评分和所有借阅历史，不能直接绕过这里调用仓库 removeById。
    bool addBook(const Book &book, QString *error); // 新增实体和索引并保存 books.csv；保存失败恢复原仓库。
    bool updateBook(const Book &book, QString *error); // 按已有编号更新并保存；true 表示内存和文件均处理成功。
    bool deleteBook(const QString &id, QString *error); // 拒绝删除有评分或任何借阅历史的图书，合法时删除、重建索引并保存。
    // userId/bookId 必须存在，score 为 1～5；成功表示评分已保存到文件。
    bool rate(const QString &userId, const QString &bookId, int score, QString *error); // 新增或更新一条评分，并持久化。
    bool borrowBook(const QString &userId, const QString &bookId,
                    const QString &borrowDate, QString *recordId, QString *error); // 借出并联动库存，recordId 输出新记录编号。
    bool returnBook(const QString &recordId, const QString &returnDate, QString *error); // 归还指定借阅记录并恢复库存，拒绝重复归还。

private: // 私有成员保留实际业务状态，外部不能直接改动后跳过校验和保存。
    bool requireWriteAccess(QString *error) const;
    std::unique_ptr<QLockFile> directoryLock_; // 读取前取得，整个快照生命周期内持有。
    bool saveRatings(QString *error) const; // 将当前评分记录写入 ratings.csv，供 rate 内部调用。
    QString dataDir_; // 成功加载的数据目录；后续文件写回此处，不一定是源码中的 data。
    BookRepository books_; // 图书实体及自己实现的编号哈希索引。
    QVector<User> users_; // 当前用户记录。
    QVector<Rating> ratings_; // 当前评分记录。
    QVector<BorrowRecord> borrowRecords_; // 当前借阅历史，包含未归还和已归还。
};

#endif // LIBRARYSTORE_H
