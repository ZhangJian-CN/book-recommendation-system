//哈希计算与链地址查找：根据编号计算桶号，再沿链比较完整编号，取得图书下标
// 现场讲解顺序：桶数组与结点 -> 字符串哈希 -> 冲突链 -> 查询 -> 删除与重建。
// 这里保存的是“编号 -> 图书数组下标”；图书的书名、作者和库存不重复存入链表。
// 可以这样概括：先用哈希缩小查找范围，再在同一个桶里比较完整编号。
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
    // other 是只读来源；用 insert 新建结点，不能把它的 unique_ptr 所有权拿走。
    // 头插会改变复制后的链内顺序，但每个 id 对应的 row 不变，不影响查询结果。
    for (const auto &bucket : other.buckets_) { // 外层逐桶取出桶头；const 引用只观察，不复制或转移智能指针。
        for (const Node *node = bucket.get(); node; node = node->next.get()) // 内层从头走到链尾；node 为空即停止。
            insert(node->id, node->row); // 为每条旧记录新建结点，只复制编号和下标，避免备份共享结点。
    }
}

BookIndex &BookIndex::operator=(const BookIndex &other)
{
    // 避免自赋值；先用拷贝构造生成独立索引，再转移桶数组的所有权。
    if (this != &other) { // this 是当前对象的地址；排除 a=a，避免无意义的自赋值操作。
        BookIndex copy(other); // 先调用拷贝构造，得到独立的新链；此步完成前原桶仍保持原值。
        buckets_ = std::move(copy.buckets_); // 再转移新桶数组的所有权，替换本对象的旧索引。
    }
    return *this; // 解引用 this 得到当前对象，并按返回类型交出引用，可支持连续赋值。
}

int BookIndex::bucketForId(const QString &id) const
{
    // 输入：业务编号，如 B001；输出：[0, 桶数-1] 中的一个桶下标。
    // 注意“哈希值”和“桶号”不同：先累积 hash，再取模得到真正用于访问数组的数。
    // 字符串长度为 L 时，此函数扫描 L 个字符，耗时 O(L)，不是对任意长字符串都 O(1)。
    // 字符串多项式哈希：依次乘 31 并加上字符编码，再取模得到桶号。
    // 不同编号可能落入同一桶，所以查询时仍要比较链表结点中的完整编号。
    // 例：B001 的字符编码为 66、48、48、49，累积得到 2013871，%101 得到桶 32。
    // 31 是本实现选用的乘数；取模确定桶的位置，不能保证不同编号不冲突。
    quint64 hash = 0; // 保存尚未取模的无符号累积哈希值；不是最终数组下标。
    for (const QChar ch : id) // 按编号顺序逐个处理 QChar，直到扫描完全部字符。
        hash = hash * 31 + ch.unicode(); // 把旧值乘 31，再加入当前字符编码，使字符及顺序都影响结果。
    return int(hash % quint64(buckets_.size())); // 取余把结果限制在合法桶下标范围，然后转回 int。
}

int BookIndex::bucketSize(int bucket) const
{
    // 调试入口：非法桶号返回 0；合法桶号通过遍历链表计数，时间与该链长度有关。
    if (bucket < 0 || bucket >= int(buckets_.size()))
        return 0;
    int count = 0; // 当前桶已经访问的结点数，空桶最终返回 0。
    for (const Node *node = buckets_[bucket].get(); node; node = node->next.get()) // 沿一条冲突链逐结点前进。
        ++count; // 每经过一个有效结点就计数一次，不修改该结点或后继关系。
    return count; // 链长可用于验证强制冲突测试，不等于桶数组的总长度。
}

void BookIndex::build(const QVector<Book> &books)
{
    // 为什么接收整份数组？数组删除后后续图书会向前移动，必须重新写入最新下标。
    // 本函数只重建辅助索引，不改变图书数组的内容和顺序。
    // 先清空旧链，再以图书在动态数组中的当前位置重建索引。
    // reset 释放每条旧链；遍历 books 时 row=0,1,2...，与实体实际位置一致。
    for (auto &bucket : buckets_) // 第一轮遍历所有桶；使用引用，才能真正清空桶数组中的桶头。
        bucket.reset(); // 释放旧链，桶头变为空指针，消除旧编号和旧下标。
    for (int row = 0; row < books.size(); ++row) // 第二轮按实体数组的新顺序遍历，row 就是当前真实位置。
        insert(books[row].id, row); // 为每本书重新记录“编号 -> 最新下标”，修复删除造成的位置变化。
}

void BookIndex::insert(const QString &id, int row)
{
    // 画图理解头插：原桶头 A -> B，加入 C 后变成 C -> A -> B。
    // 除字符串哈希与编号复制外，接链本身只改两个所有权指针，无需遍历原链。
    // 链地址法处理冲突：新结点头插到对应桶中，原来的链接到新结点后面。
    // 编号唯一性由调用方 BookRepository 检查，这里负责建立索引。
    const int bucket = bucketForId(id); // 同编号的插入、查询和删除都必须使用同一种桶号计算规则。
    // 第 1 步：分配新结点并写入“编号、数组下标”。
    auto node = std::make_unique<Node>(); // auto 推导为 unique_ptr<Node>；新分配一个由智能指针管理的结点。
    node->id = id; // 拷贝完整编号作为后续冲突比较的键。
    node->row = row; // 保存实体位置，不把整本书复制到哈希链中。
    // 第 2 步：原桶头转交给新结点的 next；move 转移所有权，不复制链表。
    node->next = std::move(buckets_[bucket]);
    // 此时旧链归 node->next 管理，桶头暂时为空；下一句把新结点交回桶头。
    // 第 3 步：让桶数组拥有新头结点，形成“新结点 -> 原链表”。
    buckets_[bucket] = std::move(node);
}

void BookIndex::erase(const QString &id)
{
    // 本函数只删除索引，不负责 books_ 数组；完整删除图书要走 BookRepository。
    // 若只记住前一个 Node*，删除头结点要单独分支；这里保存“拥有结点的指针地址”来统一处理。
    // 命中后让前面的链接接管 next，其余冲突结点仍然保留。
    // 测试把桶数设为 1，四本书形成 B004 -> B003 -> B002 -> B001。
    // 删除 B002 后应为 B004 -> B003 -> B001，可验证其余冲突结点未丢失。
    auto *link = &buckets_[bucketForId(id)]; // link 指向“持有结点的 unique_ptr 变量”，便于直接改接链关系。
    while (link->get()) { // 只要当前链接仍持有结点就继续；空链接说明已到链尾，编号不存在。
        if ((*link)->id == id) { // 先解引用取得智能指针，再用 -> 访问结点并比较完整编号。
            *link = std::move((*link)->next); // 用后继接替当前结点，实现跳过并释放当前结点。
            return; // 命中一个唯一编号后立即结束，避免继续访问已经释放的结点。
        }
        link = &(*link)->next; // 将“待检查的链接位置”移到当前结点的 next，下一轮处理后继。
    }
}

int BookIndex::rowForId(const QString &id) const
{
    // const 表示查询不会改动索引。返回 -1 是“没找到”，不能继续拿它作数组下标。
    // 冲突不是错误：同桶多个结点逐个比较 id；相同桶号不代表相同图书。
    // 编号长度固定且负载因子受控时平均近似 O(1)；固定 101 桶不断加书会使链变长，最坏 O(N)。
    // 先定位桶，再只遍历这个桶的链；找到返回数组下标，未找到返回 -1。
    // get() 取得观察用指针；沿 next 前进时不转移所有权，也不删除结点。
    for (const Node *node = buckets_[bucketForId(id)].get(); node;
         node = node->next.get()) { // 初始为桶头，每轮转到后继，遇空停止；范围仅是一个桶。
        if (node->id == id) // 比较完整编号；哈希冲突时其他编号会在这里被跳过。
            return node->row; // 命中后直接返回实体下标，无需继续扫描余下冲突结点。
    }
    return -1; // 整条链都没命中，用特殊值明确表示“查无此编号”。
}
