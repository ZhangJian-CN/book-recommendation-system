#ifndef BOOKREPOSITORY_H
#define BOOKREPOSITORY_H

#include <QString>
#include <QVector>
#include "bookindex.h"
//动态数组：保存完整图书、用户、评分和借阅记录，便于遍历、显示和保存
// Book 只保存图书；用户、评分和借阅结构在 librarystore.h，避免混淆四类 CSV。
// 本头文件回答“有哪些字段、界面能调用哪些操作”，具体算法和校验写在 .cpp。

// 图书实体；stock 表示当前可借数量，借出减一、归还加一。
struct Book { // struct 将五个字段组合成一本书；默认成员公开，便于传递实体，本身不负责保存文件。
    QString id; // 图书编号，作为唯一业务主键，供哈希索引、评分和借阅记录引用。
    QString title; // 图书书名，供列表显示及 KMP 检索；不能只有空白字符。
    QString author; // 作者，供展示及 KMP 检索；不能只有空白字符。
    QString category; // 分类文字；当前业务允许为空，不是编号索引的键。
    int stock = 0; // 当前可借数量，默认 0、不得为负；借出减一、归还加一。
};

// 图书仓库：动态数组保存完整实体，BookIndex 保存编号到数组下标的索引。
// 本层负责图书字段校验及内存增删改查，评分/借阅引用限制在 LibraryStore 检查。
class BookRepository { // class 将图书实体数组、编号索引及操作封装起来，给业务层提供一致的管理接口。
public: // 公有区：供 LibraryStore 和测试调用；bool 返回成功/失败，error 输出具体失败原因。
    explicit BookRepository(int bucketCount = 101) : index_(bucketCount) {} // 构造空仓库，同时按指定桶数初始化索引。
    bool load(const QString &path, QString *error); // 从图书 CSV 加载、校验实体，全部合法后替换内存并重建索引。
    bool save(const QString &path, QString *error) const; // 将当前图书按约定字段顺序保存到 CSV，不修改内存实体。
    const QVector<Book> &all() const { return books_; } // 返回只读数组引用，用于遍历/显示；不会复制整份图书数组。
    const Book *findById(const QString &id) const; // 按哈希索引返回实体指针，未找到为 nullptr；数组变动后应重新查询。
    bool insert(const Book &book, QString *error); // 校验字段及编号唯一性，在内存数组和索引中新增；本函数不写文件。
    bool update(const Book &book, QString *error); // 按原编号修改已有实体；编号及数组位置不变，不重建索引。
    bool removeById(const QString &id, QString *error); // 删除内存实体并重建索引，修正后续图书下标；引用限制由业务层先检查。

private: // 私有区：禁止外部绕过接口随意改数组或索引，避免二者指向关系失效。
    // 两份结构承担不同任务：books_ 便于遍历和显示，index_ 便于按编号快速定位。
    // 修改字段不改变位置；追加建立一个索引；删除后位置改变，所以重建索引。
    // books_ 保存实体，index_ 辅助查询；索引保存的下标必须与数组实际位置一致。
    QVector<Book> books_; // 动态数组拥有完整图书实体；显示全部、保存和关键词检索需要遍历它。
    BookIndex index_; // 自写哈希表只保存编号到 books_ 下标的映射，供精确编号查询。
};

#endif // BOOKREPOSITORY_H
