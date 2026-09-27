#include "bookrepository.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString path = dir.filePath(QStringLiteral("books.csv"));
    // The smoke test writes its own CSV so it also checks the agreed header.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return 2;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "book_id,title,author,category,stock\n"
        << "B001,算法图解,作者甲,计算机,3\n"
        << "B002,数据结构,作者乙,计算机,2\n";
    out.flush();
    file.close();

    QString error;
    BookRepository first;
    if (!first.load(path, &error) || first.all().size() != 2
        || !first.findById(QStringLiteral("B001")))
        return 3;
    if (!first.update({QStringLiteral("B001"), QStringLiteral("算法,图解"),
                       QStringLiteral("作者甲"), QStringLiteral("计算机"), 4}, &error)
        || !first.save(path, &error))
        return 4;

    BookRepository restarted;
    if (!restarted.load(path, &error))
        return 5;
    const Book *book = restarted.findById(QStringLiteral("B001"));
    return book && book->title == QStringLiteral("算法,图解") && book->stock == 4 ? 0 : 6;
}
