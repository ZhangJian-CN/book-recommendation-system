#ifndef BOOKINDEX_H
#define BOOKINDEX_H

#include <QString>
#include <QVector>
#include <memory>
#include <vector>
//链地址哈希表：保存“图书编号→数组下标”；桶数组定位，单链表保存冲突结点
// 先看 Node 和 buckets_，再到 .cpp 看“算桶号 -> 入链 -> 沿链查询”。
// 对外提供编号到数组下标的映射；本类不读 CSV、不显示界面，也不拥有 Book 实体。
// 必须保持的关系：成功查询 id 得到 row 后，图书仓库的 books_[row].id 应等于 id。
//链地址哈希表：桶数组+链表：用于图书编号的快速查询。

struct Book; // 前置声明：先告诉编译器有 Book 这个结构体；完整字段在 bookrepository.h 中。

// 链地址哈希索引：只保存“图书编号 -> 图书数组下标”，图书完整信息由 BookRepository 保存。
// 默认 101 个桶且不自动扩容；编号长度视为常数、负载因子受控时平均查找 O(1)，最坏 O(n)。
class BookIndex { // class 定义一个类：把哈希索引的数据和操作封装成一种类型，具体对象由调用方创建。
public: // 公有区：其他模块可以调用这些接口；访问范围一直持续到下一个 public/private 等标记。
    explicit BookIndex(int bucketCount = 101); // 构造索引，默认创建 101 个空桶；explicit 避免整数隐式转换成索引对象。
    BookIndex(const BookIndex &other); // 拷贝构造：逐结点复制 other，生成可独立修改的索引，用于业务备份和回滚。
    BookIndex &operator=(const BookIndex &other); // 拷贝赋值：用另一索引替换当前内容，返回当前对象引用。
    BookIndex(BookIndex &&) = default; // 移动构造：&& 是右值引用；由编译器生成转移桶和链表所有权的操作。
    BookIndex &operator=(BookIndex &&) = default; // 移动赋值：接管另一个对象的已有存储，避免逐结点复制。
    void build(const QVector<Book> &books); // 按完整图书数组重建“编号 -> 数组下标”；加载及数组删除后调用。
    void insert(const QString &id, int row); // 把 id、row 头插到对应桶；调用方须先保证编号不重复。
    void erase(const QString &id); // 删除一个索引结点；不删除图书实体，也不调整其他结点记录的下标。
    int rowForId(const QString &id) const; // 查编号并返回图书数组下标，未找到返回 -1；末尾 const 表示查询不改索引。
    int bucketForId(const QString &id) const; // 计算字符串哈希并取模，返回 id 所在桶号，供查询和冲突测试使用。
    int bucketSize(int bucket) const; // 遍历指定桶并返回链长；桶号无效时返回 0，用于观察冲突。

private: // 私有区：外部不能直接修改内部结点和桶，必须通过上述接口维护索引一致性。
    // struct 也能包含函数；C++ 中 struct 默认成员为 public，class 默认成员为 private。
    // 本项目用 struct 打包一条索引记录；Node 类型位于私有区，只作为 BookIndex 的内部实现。
    struct Node { // 定义链表结点：保存“编号、实体下标、后继”，把同桶冲突项连接起来。
        QString id; // 完整图书编号；桶号相同仍要比较这个字段，才能区分不同图书。
        int row; // 图书在 BookRepository::books_ 中的下标，不能当成筛选后的界面行号。
        std::unique_ptr<Node> next; // 独占后继结点的智能指针；空指针表示链尾，负责管理后继的生命周期。
    };
    // 桶数组的每个元素指向该桶链表的头结点，空指针表示空桶。
    std::vector<std::unique_ptr<Node>> buckets_; // 每个数组元素是一个桶头；vector 仅提供存储，哈希和链操作由本类实现。
};

#endif // BOOKINDEX_H
