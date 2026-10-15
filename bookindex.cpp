// 中检讲解：哈希索引。沿 bucketForId -> insert -> rowForId -> erase 查看核心操作。
#include "bookindex.h"
#include "bookrepository.h"

#include <algorithm>

// 桶数至少为 1，确保 bucketForId 的取模操作不会除以零。
BookIndex::BookIndex(int bucketCount)
    : buckets_(std::max(1, bucketCount))
{
}

BookIndex::BookIndex(const BookIndex &other)
    : buckets_(other.buckets_.size())
{
    // 深拷贝：重新创建链表结点，业务层保存失败回滚时不会与原索引共享结点。
    for (const auto &bucket : other.buckets_) {
        for (const Node *node = bucket.get(); node; node = node->next.get())
            insert(node->id, node->row);
    }
}

BookIndex &BookIndex::operator=(const BookIndex &other)
{
    // 避免自赋值；先用拷贝构造生成独立索引，再转移桶数组的所有权。
    if (this != &other) {
        BookIndex copy(other);
        buckets_ = std::move(copy.buckets_);
    }
    return *this;
}

int BookIndex::bucketForId(const QString &id) const
{
    // 字符串多项式哈希：依次乘 31 并加上字符编码，再取模得到桶号。
    // 不同编号可能落入同一桶，所以查询时仍要比较链表结点中的完整编号。
    // 例：B001 的字符编码为 66、48、48、49，累积得到 2013871，%101 得到桶 32。
    // 31 是本实现选用的乘数；取模确定桶的位置，不能保证不同编号不冲突。
    quint64 hash = 0;
    for (const QChar ch : id)
        hash = hash * 31 + ch.unicode();
    return int(hash % quint64(buckets_.size()));
}

int BookIndex::bucketSize(int bucket) const
{
    // 调试入口：非法桶号返回 0；合法桶号通过遍历链表计数，时间与该链长度有关。
    if (bucket < 0 || bucket >= int(buckets_.size()))
        return 0;
    int count = 0;
    for (const Node *node = buckets_[bucket].get(); node; node = node->next.get())
        ++count;
    return count;
}

void BookIndex::build(const QVector<Book> &books)
{
    // 先清空旧链，再以图书在动态数组中的当前位置重建索引。
    // reset 释放每条旧链；遍历 books 时 row=0,1,2...，与实体实际位置一致。
    for (auto &bucket : buckets_)
        bucket.reset();
    for (int row = 0; row < books.size(); ++row)
        insert(books[row].id, row);
}

void BookIndex::insert(const QString &id, int row)
{
    // 链地址法处理冲突：新结点头插到对应桶中，原来的链接到新结点后面。
    // 编号唯一性由调用方 BookRepository 检查，这里负责建立索引。
    const int bucket = bucketForId(id);
    // 第 1 步：分配新结点并写入“编号、数组下标”。
    auto node = std::make_unique<Node>();
    node->id = id;
    node->row = row;
    // 第 2 步：原桶头转交给新结点的 next；move 转移所有权，不复制链表。
    node->next = std::move(buckets_[bucket]);
    // 第 3 步：让桶数组拥有新头结点，形成“新结点 -> 原链表”。
    buckets_[bucket] = std::move(node);
}

void BookIndex::erase(const QString &id)
{
    // link 指向“拥有当前结点的指针”，统一处理删除头结点和中间结点。
    // 命中后让前面的链接接管 next，其余冲突结点仍然保留。
    // 测试把桶数设为 1，四本书形成 B004 -> B003 -> B002 -> B001。
    // 删除 B002 后应为 B004 -> B003 -> B001，可验证其余冲突结点未丢失。
    auto *link = &buckets_[bucketForId(id)];
    while (link->get()) {
        if ((*link)->id == id) {
            *link = std::move((*link)->next);
            return;
        }
        // 未命中则把 link 移到当前结点的 next 所在位置，检查下一结点。
        link = &(*link)->next;
    }
}

int BookIndex::rowForId(const QString &id) const
{
    // 先定位桶，再只遍历这个桶的链；找到返回数组下标，未找到返回 -1。
    // get() 取得观察用指针；沿 next 前进时不转移所有权，也不删除结点。
    for (const Node *node = buckets_[bucketForId(id)].get(); node;
         node = node->next.get()) {
        if (node->id == id)
            return node->row;
    }
    return -1;
}
