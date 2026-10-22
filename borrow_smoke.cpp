// 借还测试简述：临时目录中检查库存、重复借还、日期、保存失败和中断恢复。
#include "librarystore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <limits>

bool stockBoundaries(const QDir &sample)
{
    QTemporaryDir dir;
    if (!dir.isValid()) return false;
    for (const QString &name : QStringList{"books.csv", "users.csv", "ratings.csv", "borrow_records.csv"})
        if (!QFile::copy(sample.filePath(name), dir.filePath(name))) return false;
    QString error, record;
    LibraryStore store;
    if (!store.load(dir.path(), &error)) return false;
    Book book = *store.books().findById("B004");
    book.stock = 0;
    if (!store.updateBook(book, &error) || store.borrowBook("U0", "B004", "2026-10-22", &record, &error)) return false;
    book.stock = std::numeric_limits<int>::max();
    if (!store.updateBook(book, &error) || !store.borrowBook("U0", "B004", "2026-10-22", &record, &error)
        || store.books().findById("B004")->stock != book.stock - 1) return false;
    if (!store.returnBook(record, "2026-10-22", &error) || store.books().findById("B004")->stock != book.stock) return false;
    if (!store.borrowBook("U0", "B004", "2026-10-22", &record, &error) || !store.updateBook(book, &error)) return false;
    const int count = store.borrowRecords().size();
    if (store.returnBook(record, "2026-10-22", &error) || store.books().findById("B004")->stock != book.stock
        || store.borrowRecords().size() != count || store.borrowRecords().last().status != "borrowed") return false;
    store = LibraryStore{};
    LibraryStore loaded;
    return loaded.load(dir.path(), &error) && loaded.books().findById("B004")->stock == book.stock
        && loaded.borrowRecords().last().status == "borrowed";
}

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
    store = LibraryStore{}; // Release the old writer before verifying saved files.
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

    store = LibraryStore{}; // Release the old writer before verifying saved files.
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
    restarted = LibraryStore{};
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
    recovered = LibraryStore{};
    LibraryStore invalid;
    if (invalid.load(temp.path(), &error))
        return 15;
    if (!blockedFileCase(sample, QStringLiteral("books.csv")))
        return 16;
    if (!blockedFileCase(sample, QStringLiteral("borrow_records.csv")))
        return 17;
    if (!stockBoundaries(sample))
        return 18;
    return 0;
}
