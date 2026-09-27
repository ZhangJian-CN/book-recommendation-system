#include "librarystore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

bool blockedFileCase(const QDir &sample, const QString &blockedName)
{
    QTemporaryDir temp;
    if (!temp.isValid())
        return false;
    for (const QString &name : {QStringLiteral("books.csv"), QStringLiteral("users.csv"),
                                QStringLiteral("ratings.csv"),
                                QStringLiteral("borrow_records.csv")}) {
        if (!QFile::copy(sample.filePath(name), temp.filePath(name)))
            return false;
    }
    LibraryStore store;
    QString error;
    if (!store.load(temp.path(), &error))
        return false;
    const QDir dir(temp.path());
    const QString original = dir.filePath(blockedName);
    const QString held = original + QStringLiteral(".held");
    if (!QFile::rename(original, held) || !QDir(temp.path()).mkdir(blockedName))
        return false;
    QString recordId;
    const bool refused = !store.borrowBook(QStringLiteral("U0"), QStringLiteral("B004"),
                                           QStringLiteral("2026-09-27"), &recordId, &error);
    const bool memoryUnchanged = store.books().findById(QStringLiteral("B004"))->stock == 2
                                 && store.borrowRecords().size() == 2;
    if (!QDir(temp.path()).rmdir(blockedName) || !QFile::rename(held, original))
        return false;
    LibraryStore restarted;
    return refused && memoryUnchanged && restarted.load(temp.path(), &error)
           && restarted.books().findById(QStringLiteral("B004"))->stock == 2
           && restarted.borrowRecords().size() == 2;
}

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
    LibraryStore store;
    QString error;
    QString id;
    if (!store.load(temp.path(), &error)
        || !store.borrowBook(QStringLiteral("U0"), QStringLiteral("B004"),
                             QStringLiteral("2026-09-27"), &id, &error)
        || id != QStringLiteral("R003")
        || store.books().findById(QStringLiteral("B004"))->stock != 1)
        return 3;
    const int recordCount = store.borrowRecords().size();
    if (store.borrowBook(QStringLiteral("U0"), QStringLiteral("B004"),
                         QStringLiteral("2026-09-27"), &id, &error)
        || store.borrowBook(QStringLiteral("missing"), QStringLiteral("B004"),
                            QStringLiteral("2026-09-27"), &id, &error)
        || store.borrowBook(QStringLiteral("U0"), QStringLiteral("missing"),
                            QStringLiteral("2026-09-27"), &id, &error)
        || store.borrowBook(QStringLiteral("U0"), QStringLiteral("B003"),
                            QStringLiteral("2026-02-30"), &id, &error)
        || store.borrowRecords().size() != recordCount)
        return 4;
    if (!store.borrowBook(QStringLiteral("U2"), QStringLiteral("B003"),
                          QStringLiteral("2026-09-27"), &id, &error)
        || id != QStringLiteral("R004")
        || store.books().findById(QStringLiteral("B003"))->stock != 0
        || store.borrowBook(QStringLiteral("U0"), QStringLiteral("B003"),
                            QStringLiteral("2026-09-27"), &id, &error))
        return 5;
    if (store.returnBook(QStringLiteral("R004"), QStringLiteral("2026-09-26"), &error)
        || !store.returnBook(QStringLiteral("R004"), QStringLiteral("2026-09-27"), &error)
        || store.returnBook(QStringLiteral("R004"), QStringLiteral("2026-09-27"), &error)
        || store.books().findById(QStringLiteral("B003"))->stock != 1)
        return 6;
    if (!store.borrowBook(QStringLiteral("U2"), QStringLiteral("B003"),
                          QStringLiteral("2026-09-28"), &id, &error)
        || id != QStringLiteral("R005"))
        return 7;

    LibraryStore restarted;
    if (!restarted.load(temp.path(), &error)
        || restarted.books().findById(QStringLiteral("B004"))->stock != 1
        || restarted.books().findById(QStringLiteral("B003"))->stock != 0
        || restarted.borrowRecords().size() != 5)
        return 8;

    const QString bookBackup = temp.filePath(QStringLiteral(".books.csv.borrow_backup"));
    const QString recordBackup = temp.filePath(QStringLiteral(".borrow_records.csv.borrow_backup"));
    if (!QFile::copy(temp.filePath(QStringLiteral("books.csv")), bookBackup)
        || !QFile::copy(temp.filePath(QStringLiteral("borrow_records.csv")), recordBackup))
        return 9;
    QFile marker(temp.filePath(QStringLiteral(".borrow_transaction")));
    if (!marker.open(QIODevice::WriteOnly) || marker.write("pending") != 7)
        return 10;
    marker.close();
    BookRepository partial;
    if (!partial.load(temp.filePath(QStringLiteral("books.csv")), &error))
        return 11;
    Book changed = *partial.findById(QStringLiteral("B004"));
    changed.stock = 99;
    if (!partial.update(changed, &error)
        || !partial.save(temp.filePath(QStringLiteral("books.csv")), &error))
        return 12;
    LibraryStore recovered;
    if (!recovered.load(temp.path(), &error)
        || recovered.books().findById(QStringLiteral("B004"))->stock != 1
        || recovered.borrowRecords().size() != 5
        || QFile::exists(temp.filePath(QStringLiteral(".borrow_transaction"))))
        return 13;
    QFile duplicate(temp.filePath(QStringLiteral("borrow_records.csv")));
    if (!duplicate.open(QIODevice::Append)
        || duplicate.write("R006,U1,B001,2026-09-27,,borrowed\n") < 0)
        return 14;
    duplicate.close();
    LibraryStore invalid;
    if (invalid.load(temp.path(), &error))
        return 15;
    if (!blockedFileCase(sample, QStringLiteral("books.csv")))
        return 16;
    if (!blockedFileCase(sample, QStringLiteral("borrow_records.csv")))
        return 17;
    return 0;
}
