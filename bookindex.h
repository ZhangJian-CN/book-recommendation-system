#ifndef BOOKINDEX_H
#define BOOKINDEX_H

#include <QString>
#include <QVector>
#include <memory>
#include <vector>

//链地址哈希表：桶数组+链表：用于图书编号的快速查询。

struct Book;

// 链地址哈希索引：只保存“图书编号 -> 图书数组下标”，图书完整信息由 BookRepository 保存。
// 默认 101 个桶且不自动扩容；编号长度视为常数、负载因子受控时平均查找 O(1)，最坏 O(n)。
class BookIndex {
public:
    // bucketCount 是桶数；构造后各桶为空，完整图书数据不复制到索引中。
    explicit BookIndex(int bucketCount = 101);
    BookIndex(const BookIndex &other);
    BookIndex &operator=(const BookIndex &other);
    BookIndex(BookIndex &&) = default;
    BookIndex &operator=(BookIndex &&) = default;
    // 输入完整图书数组，重建每个编号对应的下标；加载和数组删除后调用。
    void build(const QVector<Book> &books);
    // id 是业务编号，row 是图书数组的零起始下标；以头插法加入对应桶。
    void insert(const QString &id, int row);
    // 仅移除索引结点，不删除图书实体，也不调整其他结点保存的数组下标。
    void erase(const QString &id);
    // 找到返回数组下标，未找到返回 -1；const 表示查询不修改索引。
    int rowForId(const QString &id) const;
    // 返回编号对应的桶号，可配合 bucketSize 观察冲突和链长。
    int bucketForId(const QString &id) const;
    int bucketSize(int bucket) const;

//出现冲突时，eg：两个编号落在同一个位置，通过链表把它们链接起来。

private:
    // 同一个桶中的编号用单链表连接；unique_ptr 管理结点所有权和释放。
    struct Node {
        // 冲突时逐个比较完整 id，不能仅凭桶号判断是同一本书。
        QString id;
        // row 指向 BookRepository::books_ 的元素，不是界面筛选表格的行号。
        int row;
        // next 独占后继结点；销毁整条链时随所有权关系释放结点。
        std::unique_ptr<Node> next;
    };
    // 桶数组的每个元素指向该桶链表的头结点，空指针表示空桶。
    std::vector<std::unique_ptr<Node>> buckets_;
};

#endif // BOOKINDEX_H
