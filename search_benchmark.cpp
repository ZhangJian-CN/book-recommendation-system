// 【曹振江重点：顺序查找与哈希查找的效率对照】
// 本程序单独运行，不是主窗口，也不是补丁；输出 CSV 格式的耗时与加速比。
// 比较前提：两种方法使用同一个仓库、同一批查询，结果校验和相同；建数据不在计时区间内。
// 现场说明应带上 Release/硬件/数据规模，不能把旧机器的一次结果当成通用性能承诺。
#include "bookrepository.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>
#include <algorithm>
#include <array>

namespace {

QString bookId(int number)
{
    // 统一构造五位数字编号，如 B00001；基准数据与四本书小样例是不同的独立场景。
    return QStringLiteral("B%1").arg(number, 5, 10, QLatin1Char('0'));
}

const Book *findSequentially(const QVector<Book> &books, const QString &id)
{
    // 朴素对照方法：从头逐本比较，命中返回实体指针，遍历完未命中返回空指针。
    for (const Book &book : books) { // 顺序对照算法逐本比较，命中即返回；未命中时须扫描整份数组。
        if (book.id == id)
            return &book;
    }
    return nullptr;
}

struct Timing { // struct 打包一次测量的两个输出，便于同时返回耗时与结果校验和。
    qint64 nanoseconds; // 本轮查询总耗时，单位纳秒，不包含数据生成时间。
    qint64 checksum; // 汇总查询结果，用于检查两种方法是否出现明显结果差异。
};

Timing measure(const BookRepository &repository, const QVector<QString> &queries,
               bool useHash)
{
    // useHash 决定走哪种算法；循环与计时方式相同，避免把不同测量口径当作速度差异。
    // 返回纳秒和校验和；校验和可发现常见结果差异，也使查询结果被实际使用。
    QElapsedTimer timer;
    timer.start();
    qint64 checksum = 0;
    for (const QString &id : queries) { // 每轮取一个预先生成的编号，计时区间内只做查询与结果累加。
        const Book *book = useHash ? repository.findById(id)
                                   : findSequentially(repository.all(), id);
        checksum += book ? book->stock : -1;
    }
    return {timer.nsecsElapsed(), checksum}; // 按 Timing 的字段顺序初始化返回值，分别交出耗时和校验和。
}

qint64 median(std::array<qint64, 3> values)
{
    // 三个测量值排序后取中间值，降低某一轮偶然系统调度或后台负载造成的影响。
    std::sort(values.begin(), values.end());
    return values[1];
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    out << "books,queries,buckets,sequential_ms,hash_ms,speedup\n";
    for (const int size : {100, 1000, 5000}) { // 外层依次切换三种图书规模，观察随数据量增长的耗时变化。
        // 每种规模重新建立独立仓库；默认桶数固定 101，不会随 size 自动增长。
        BookRepository repository;
        QString error;
        for (int i = 1; i <= size; ++i) { // 先生成本规模的所有实体及索引；此循环不在查询计时中。
            if (!repository.insert({bookId(i), QStringLiteral("演示图书"),
                                    QStringLiteral("作者"), QStringLiteral("计算机"), 1},
                                   &error))
                return 1;
        }

        QVector<QString> queries;
        // 共 100000 次查询，每 10 个放入 1 个不存在的 X 编号，包含查找失败的路径。
        queries.reserve(100000);
        for (int i = 0; i < 100000; ++i) { // 预先生成同一批查询，让顺序和哈希方法接受相同输入。
            queries.append(i % 10 == 0 ? QStringLiteral("X%1").arg(i)
                                       : bookId(1 + (i * 37) % size));
        }

        std::array<qint64, 3> sequentialTimes;
        std::array<qint64, 3> hashTimes;
        for (int run = 0; run < 3; ++run) { // 每种方法运行三轮，分别保存耗时，最后取各自中位数。
            const Timing sequential = measure(repository, queries, false);
            const Timing hash = measure(repository, queries, true);
            if (sequential.checksum != hash.checksum)
                return 2;
            sequentialTimes[run] = sequential.nanoseconds;
            hashTimes[run] = hash.nanoseconds;
        }
        const double sequentialMs = median(sequentialTimes) / 1e6;
        // 前面的计时单位是纳秒，除以 1e6 得毫秒；加速比 = 顺序耗时 / 哈希耗时。
        const double hashMs = median(hashTimes) / 1e6;
        out << size << ",100000,101," << sequentialMs << ',' << hashMs << ','
            << sequentialMs / hashMs << '\n';
    }
    return 0;
}
