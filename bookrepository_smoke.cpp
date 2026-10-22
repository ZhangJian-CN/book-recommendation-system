// 【曹振江重点：保存与重读的最小闭环】
// 自建两本书 -> 加载 -> 将 B001 改成含逗号的书名、库存 4 -> 保存 -> 用新仓库重读。
// 同时验证 CSV 转义与持久化：不能仅观察旧对象中的值，因为它本来就已被内存更新。
#include "bookrepository.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <limits>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString path = dir.filePath(QStringLiteral("books.csv"));
    // 测试自行写初始 CSV，同时核对程序约定的表头、UTF-8 中文和字段顺序。
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
    // 特意在书名中加入逗号：write 应加引号，read 应把它读回同一个字段，而不是多出一列。
    if (!first.update({QStringLiteral("B001"), QStringLiteral("算法,图解"),
                       QStringLiteral("作者甲"), QStringLiteral("计算机"), 4}, &error)
        || !first.save(path, &error))
        return 4;

    BookRepository restarted;
    // 新对象没有旧内存中的修改，只能从保存后的文件重新恢复数据。
    if (!restarted.load(path, &error))
        return 5;
    const Book *book = restarted.findById(QStringLiteral("B001"));
    if (!book || book->title != QStringLiteral("算法,图解") || book->stock != 4)
        return 6;
    for (int stock : {0, 1, 1000000, 1000001, std::numeric_limits<int>::max() - 1, std::numeric_limits<int>::max()}) {
        Book changed = *restarted.findById(QStringLiteral("B001"));
        changed.stock = stock;
        if (!restarted.update(changed, &error) || !restarted.save(path, &error)) return 7;
        BookRepository loaded;
        if (!loaded.load(path, &error) || loaded.findById(QStringLiteral("B001"))->stock != stock) return 8;
    }
    for (const QByteArray &invalid : {QByteArray("-1"), QByteArray("not-a-number"), QByteArray("2147483648")}) {
        const QByteArray bytes = "book_id,title,author,category,stock\nB001,Title,Author,Category," + invalid + "\n";
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size()) return 9;
        file.close();
        BookRepository rejected;
        if (rejected.load(path, &error) || !file.open(QIODevice::ReadOnly)) return 10;
        if (file.readAll() != bytes) return 11;
        file.close();
    }
    return 0;
}
