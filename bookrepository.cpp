// 上接 LibraryStore 的业务操作，下接 Csv 的文本读写和 BookIndex 的编号索引。
// 讲解时分清三件事：仓库校验图书字段，业务层检查评分/借阅引用，CSV 层解析文件格式。
#include "bookrepository.h"
#include "csvutil.h"

#include <QSet>

namespace {
const QString header = QStringLiteral("book_id,title,author,category,stock");

bool validBook(const Book &book)
{
    // 这是内存接口也会使用的校验；不能假定所有调用一定来自带输入限制的界面。
    // id 不允许空值或两端空白；书名/作者至少含非空白内容；category 当前允许为空。
    return !book.id.isEmpty() && book.id == book.id.trimmed()
           && !book.title.trimmed().isEmpty()
           && !book.author.trimmed().isEmpty() && book.stock >= 0;
}
}

bool BookRepository::load(const QString &path, QString *error)
{
    // 输入图书 CSV 路径；成功返回 true，失败通过 error 交出原因。
    // loaded 是候选新数据，books_ 是原有效数据；没全部验完前不能先清空原仓库。
    // CSV 字符串字段转为 Book；库存必须是非负整数，编号唯一，书名和作者非空。
    // 用临时数组校验全部行，成功后替换仓库并建立哈希索引。
    QVector<QStringList> rows;
    if (!Csv::read(path, header, 5, &rows, error))
        return false;
    QVector<Book> loaded;
    // QSet 用于加载时检查编号唯一性；运行时按编号查找使用 BookIndex。
    QSet<QString> ids;
    // QSet 在这里仅用于加载时去重；运行时按编号查找仍调用自己实现的 BookIndex。
    for (int row = 0; row < rows.size(); ++row) { // 逐条处理 CSV 数据行；表头已由 Csv::read 读过。
        const QStringList &fields = rows[row]; // 借用这一行五个文本字段，避免再次复制整行。
        bool stockOk = false; // 单独记录库存转换是否成功，不能只看转换后的整数。
        // 还须检查 stockOk，区分合法库存文本 "0" 与转整数失败得到的 0。
        const int stock = fields[4].toInt(&stockOk);
        Book book{fields[0], fields[1], fields[2], fields[3], stock};
        if (!stockOk || !validBook(book) || ids.contains(book.id)) {
            *error = QStringLiteral("books.csv 第 %1 行数据无效或编号重复").arg(row + 2);
            // row 从 0 开始且文件有一行表头，所以错误位置为 row+2。
            return false;
        }
        ids.insert(book.id); // 登记已见编号，下一行若出现相同编号就会被判为重复。
        loaded.append(book); // 仅把已通过校验的实体放入候选数组，尚不修改原仓库。
    }
    // 校验结束后提交完整数组，索引必须按新数组位置建立。
    books_ = loaded;
    index_.build(books_);
    return true;
}

bool BookRepository::save(const QString &path, QString *error) const
{
    // const 表示保存不修改内存实体；字段顺序必须与 header 约定完全一致。
    // 磁盘中的 stock 是文字，内存中是整数；读写两端各做一次类型转换。
    // 按固定字段顺序把 Book 转回文本行，CSV 引号处理及文件提交交给通用层。
    QVector<QStringList> rows;
    for (const Book &book : books_) // 顺序遍历所有实体，每本书序列化为一条五字段记录。
        rows.append({book.id, book.title, book.author, book.category,
                     QString::number(book.stock)});
    return Csv::write(path, header, rows, error);
}

const Book *BookRepository::findById(const QString &id) const
{
    // 这几行是证明“界面编号查询真的用了哈希”的入口，而不仅是在项目里放了一个哈希类。
    // 通过 BookIndex 哈希索引找到数组下标，再访问完整图书，不顺序扫描全部图书。
    const int row = index_.rowForId(id); // 先取得下标，后面才决定能否访问 books_[row]。
    // 找到返回实体指针，未找到返回 nullptr，不能拿 -1 去访问数组。
    return row < 0 ? nullptr : &books_[row];
}

bool BookRepository::insert(const Book &book, QString *error)
{
    // 参数是完整实体；成功时数组多一项、索引多一个映射，但此函数本身不写文件。
    // true 表示内存仓库已新增；最终持久化由 LibraryStore 调用 save 完成。
    if (!validBook(book)) {
        *error = QStringLiteral("编号、书名、作者不能为空，库存不能为负数");
        return false;
    }
    if (findById(book.id)) {
        // 先用哈希查重再插入，保证一个业务编号只对应一本书。
        *error = QStringLiteral("图书编号 %1 已存在").arg(book.id);
        return false;
    }
    // 新图书追加在数组末尾，追加前的 size 就是它将使用的下标。
    index_.insert(book.id, books_.size());
    books_.append(book);
    return true;
}

bool BookRepository::update(const Book &book, QString *error)
{
    // 业务主键是 id，界面修改现有图书时继续使用 selectedId_，不从“新书编号”框改主键。
    // book.id 定位待修改实体；只能更新已有编号，不能通过更新接口新增或改号。
    if (!validBook(book)) {
        *error = QStringLiteral("编号、书名、作者不能为空，库存不能为负数");
        return false;
    }
    const int row = index_.rowForId(book.id);
    if (row < 0) {
        *error = QStringLiteral("没有找到编号 %1").arg(book.id);
        return false;
    }
    // 按原编号找到位置并替换字段；编号及数组下标不变，因此无需重建索引。
    books_[row] = book;
    return true;
}

bool BookRepository::removeById(const QString &id, QString *error)
{
    // 原数组 [B001,B002,B003] 删除 B002 后变成 [B001,B003]；B003 的下标从 2 变成 1。
    // 所以完整删除包含“移动数组元素 + 重建索引”，不能把它的耗时讲成仅删除一个链结点。
    // 调用前业务层先检查评分及借阅引用；本函数处理实体数组及其索引。
    const int row = index_.rowForId(id);
    if (row < 0) {
        *error = QStringLiteral("没有找到编号 %1").arg(id);
        return false;
    }
    // 动态数组删除会使后面的元素下标前移，所以必须重建整个编号索引。
    // 此处整体删除为 O(n)，不能仅删除哈希结点后保留旧下标。
    // 例：删除下标 1 后，原下标 2、3 变成 1、2，旧索引会指错实体甚至越界。
    books_.removeAt(row);
    index_.build(books_);
    return true;
}
