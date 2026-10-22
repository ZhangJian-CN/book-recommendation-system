// 【曹振江重点：怎样证明自己实现的哈希表能处理冲突】
// 先复制样例到临时目录；退出码 0 表示通过，其他数字帮助定位失败的检查步骤。
// 关键用例是桶数设为 1，使不同编号必然冲突，而不是碰巧挑选几个没冲突的编号。
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
    // 测试操作副本，避免新增/删除过程改动仓库中准备给老师展示的原始 CSV。
    if (!temp.isValid())
        return 1;

    const QDir sample(QStringLiteral(SAMPLE_DATA_DIR));
    for (const QString &name : {QStringLiteral("books.csv"), QStringLiteral("users.csv"),
                                QStringLiteral("ratings.csv"),
                                QStringLiteral("borrow_records.csv")}) { // 逐份复制四个样例文件，后续读写只发生在临时目录。
        if (!QFile::copy(sample.filePath(name), temp.filePath(name)))
            return 2;
    }

    QString error;
    LibraryStore store;
    if (!store.load(temp.path(), &error))
        return 3;

    BookIndex collisions(1);
    // 四本书都在桶 0：既要数出四个结点，又要能找到链中指定编号对应的正确数组下标。
    collisions.build(store.books().all());
    if (collisions.bucketSize(0) != 4
        || collisions.rowForId(QStringLiteral("B004")) != 3)
        return 4;
    collisions.erase(QStringLiteral("B002"));
    // 删掉链中一个结点后，查它应为 -1，而同桶其他编号仍能正常找到。
    if (collisions.rowForId(QStringLiteral("B002")) != -1
        || collisions.rowForId(QStringLiteral("B004")) != 3)
        return 5;

    const auto matches = BookSearch::keywordRows(store.books().all(),
                                                  QStringLiteral("布莱恩特"));
    if (matches.size() != 1 || store.books().all()[matches[0]].id != QStringLiteral("B004"))
        return 6;

    // B001 被样例记录引用，删除应被拒绝；测试的成功条件是“非法操作确实没成功”。
    if (store.deleteBook(QStringLiteral("B001"), &error))
        return 7;
    const Book newBook{QStringLiteral("B005"), QStringLiteral("新书"),
                       QStringLiteral("作者"), QStringLiteral("计算机"), 1};
    // 第一次增加合法编号应成功，第二次同编号必须失败，证明唯一性检查有效。
    if (!store.addBook(newBook, &error) || store.addBook(newBook, &error))
        return 8;
    if (!store.deleteBook(newBook.id, &error) || store.books().findById(newBook.id))
        return 9;

    store = LibraryStore{}; // Release the old snapshot and its directory lock before restart.
    LibraryStore restarted;
    // 再加载同一临时目录，确认删除不只是改了内存列表，磁盘文件也已经同步。
    if (!restarted.load(temp.path(), &error)
        || restarted.books().findById(newBook.id))
        return 10;
    return 0;
}
