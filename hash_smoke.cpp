#include "bookindex.h"
#include "booksearch.h"
#include "librarystore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid())
        return 1;

    const QDir sample(QStringLiteral(SAMPLE_DATA_DIR));
    for (const QString &name : {QStringLiteral("books.csv"), QStringLiteral("users.csv"),
                                QStringLiteral("ratings.csv"),
                                QStringLiteral("borrow_records.csv")}) {
        if (!QFile::copy(sample.filePath(name), temp.filePath(name)))
            return 2;
    }

    QString error;
    LibraryStore store;
    if (!store.load(temp.path(), &error))
        return 3;

    BookIndex collisions(1);
    collisions.build(store.books().all());
    if (collisions.bucketSize(0) != 4
        || collisions.rowForId(QStringLiteral("B004")) != 3)
        return 4;
    collisions.erase(QStringLiteral("B002"));
    if (collisions.rowForId(QStringLiteral("B002")) != -1
        || collisions.rowForId(QStringLiteral("B004")) != 3)
        return 5;

    const auto matches = BookSearch::keywordRows(store.books().all(),
                                                  QStringLiteral("布莱恩特"));
    if (matches.size() != 1 || store.books().all()[matches[0]].id != QStringLiteral("B004"))
        return 6;

    if (store.deleteBook(QStringLiteral("B001"), &error))
        return 7;
    const Book newBook{QStringLiteral("B005"), QStringLiteral("新书"),
                       QStringLiteral("作者"), QStringLiteral("计算机"), 1};
    if (!store.addBook(newBook, &error) || store.addBook(newBook, &error))
        return 8;
    if (!store.deleteBook(newBook.id, &error) || store.books().findById(newBook.id))
        return 9;

    LibraryStore restarted;
    if (!restarted.load(temp.path(), &error)
        || restarted.books().findById(newBook.id))
        return 10;
    return 0;
}
