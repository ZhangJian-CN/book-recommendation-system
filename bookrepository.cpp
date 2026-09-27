#include "bookrepository.h"
#include "csvutil.h"

#include <QSet>

namespace {
const QString header = QStringLiteral("book_id,title,author,category,stock");

bool validBook(const Book &book)
{
    return !book.id.isEmpty() && book.id == book.id.trimmed()
           && !book.title.trimmed().isEmpty()
           && !book.author.trimmed().isEmpty() && book.stock >= 0;
}
}

bool BookRepository::load(const QString &path, QString *error)
{
    QVector<QStringList> rows;
    if (!Csv::read(path, header, 5, &rows, error))
        return false;
    QVector<Book> loaded;
    QSet<QString> ids;
    for (int row = 0; row < rows.size(); ++row) {
        const QStringList &fields = rows[row];
        bool stockOk = false;
        const int stock = fields[4].toInt(&stockOk);
        Book book{fields[0], fields[1], fields[2], fields[3], stock};
        if (!stockOk || !validBook(book) || ids.contains(book.id)) {
            *error = QStringLiteral("books.csv 第 %1 行数据无效或编号重复").arg(row + 2);
            return false;
        }
        ids.insert(book.id);
        loaded.append(book);
    }
    books_ = loaded;
    index_.build(books_);
    return true;
}

bool BookRepository::save(const QString &path, QString *error) const
{
    QVector<QStringList> rows;
    for (const Book &book : books_)
        rows.append({book.id, book.title, book.author, book.category,
                     QString::number(book.stock)});
    return Csv::write(path, header, rows, error);
}

const Book *BookRepository::findById(const QString &id) const
{
    const int row = index_.rowForId(id);
    return row < 0 ? nullptr : &books_[row];
}

bool BookRepository::insert(const Book &book, QString *error)
{
    if (!validBook(book)) {
        *error = QStringLiteral("编号、书名、作者不能为空，库存不能为负数");
        return false;
    }
    if (findById(book.id)) {
        *error = QStringLiteral("图书编号 %1 已存在").arg(book.id);
        return false;
    }
    index_.insert(book.id, books_.size());
    books_.append(book);
    return true;
}

bool BookRepository::update(const Book &book, QString *error)
{
    if (!validBook(book)) {
        *error = QStringLiteral("编号、书名、作者不能为空，库存不能为负数");
        return false;
    }
    const int row = index_.rowForId(book.id);
    if (row < 0) {
        *error = QStringLiteral("没有找到编号 %1").arg(book.id);
        return false;
    }
    books_[row] = book;
    return true;
}

bool BookRepository::removeById(const QString &id, QString *error)
{
    const int row = index_.rowForId(id);
    if (row < 0) {
        *error = QStringLiteral("没有找到编号 %1").arg(id);
        return false;
    }
    books_.removeAt(row);
    index_.build(books_);
    return true;
}
