#include "bookrepository.h"

#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>

namespace {

bool parseCsvLine(const QString &line, QStringList *fields)
{
    QString field;
    bool quoted = false;
    bool closedQuote = false;
    fields->clear();
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    quoted = false;
                    closedQuote = true;
                }
            } else {
                field += ch;
            }
        } else if (ch == ',') {
            fields->append(field);
            field.clear();
            closedQuote = false;
        } else if (ch == '"' && field.isEmpty() && !closedQuote) {
            quoted = true;
        } else if (closedQuote || ch == '"') {
            return false;
        } else {
            field += ch;
        }
    }
    if (quoted)
        return false;
    fields->append(field);
    return true;
}

QString csvField(const QString &value)
{
    if (!value.contains(',') && !value.contains('"') && !value.contains('\n'))
        return value;
    QString escaped = value;
    escaped.replace('"', "\"\"");
    return '"' + escaped + '"';
}

bool validBook(const Book &book)
{
    return !book.id.isEmpty() && book.id == book.id.trimmed()
           && !book.title.trimmed().isEmpty()
           && !book.author.trimmed().isEmpty() && book.stock >= 0;
}

} // namespace

bool BookRepository::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("无法读取 %1：%2").arg(path, file.errorString());
        return false;
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    if (in.readLine() != QStringLiteral("book_id,title,author,category,stock")) {
        *error = QStringLiteral("books.csv 表头不符合约定");
        return false;
    }
    QVector<Book> loaded;
    QSet<QString> ids;
    int lineNumber = 1;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNumber;
        QStringList fields;
        if (!parseCsvLine(line, &fields) || fields.size() != 5) {
            *error = QStringLiteral("books.csv 第 %1 行格式错误").arg(lineNumber);
            return false;
        }
        bool stockOk = false;
        const int stock = fields[4].toInt(&stockOk);
        Book book{fields[0], fields[1], fields[2], fields[3], stock};
        if (!stockOk || !validBook(book) || ids.contains(book.id)) {
            *error = QStringLiteral("books.csv 第 %1 行数据无效或编号重复").arg(lineNumber);
            return false;
        }
        ids.insert(book.id);
        loaded.append(book);
    }
    if (in.status() != QTextStream::Ok) {
        *error = QStringLiteral("读取 books.csv 时发生错误");
        return false;
    }
    books_ = loaded;
    return true;
}

bool BookRepository::save(const QString &path, QString *error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        *error = QStringLiteral("无法保存 %1：%2").arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "book_id,title,author,category,stock\n";
    for (const Book &book : books_) {
        out << csvField(book.id) << ',' << csvField(book.title) << ','
            << csvField(book.author) << ',' << csvField(book.category) << ','
            << book.stock << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        *error = QStringLiteral("写入 books.csv 失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

const Book *BookRepository::findById(const QString &id) const
{
    for (const Book &book : books_) {
        if (book.id == id)
            return &book;
    }
    return nullptr;
}

bool BookRepository::update(const Book &book, QString *error)
{
    if (!validBook(book)) {
        *error = QStringLiteral("编号、书名、作者不能为空，库存不能为负数");
        return false;
    }
    for (Book &existing : books_) {
        if (existing.id == book.id) {
            existing = book;
            return true;
        }
    }
    *error = QStringLiteral("没有找到编号 %1").arg(book.id);
    return false;
}
