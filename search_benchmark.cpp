#include "bookrepository.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>
#include <algorithm>
#include <array>

namespace {

QString bookId(int number)
{
    return QStringLiteral("B%1").arg(number, 5, 10, QLatin1Char('0'));
}

const Book *findSequentially(const QVector<Book> &books, const QString &id)
{
    for (const Book &book : books) {
        if (book.id == id)
            return &book;
    }
    return nullptr;
}

struct Timing {
    qint64 nanoseconds;
    qint64 checksum;
};

Timing measure(const BookRepository &repository, const QVector<QString> &queries,
               bool useHash)
{
    QElapsedTimer timer;
    timer.start();
    qint64 checksum = 0;
    for (const QString &id : queries) {
        const Book *book = useHash ? repository.findById(id)
                                   : findSequentially(repository.all(), id);
        checksum += book ? book->stock : -1;
    }
    return {timer.nsecsElapsed(), checksum};
}

qint64 median(std::array<qint64, 3> values)
{
    std::sort(values.begin(), values.end());
    return values[1];
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    out << "books,queries,buckets,sequential_ms,hash_ms,speedup\n";
    for (const int size : {100, 1000, 5000}) {
        BookRepository repository;
        QString error;
        for (int i = 1; i <= size; ++i) {
            if (!repository.insert({bookId(i), QStringLiteral("演示图书"),
                                    QStringLiteral("作者"), QStringLiteral("计算机"), 1},
                                   &error))
                return 1;
        }

        QVector<QString> queries;
        queries.reserve(100000);
        for (int i = 0; i < 100000; ++i) {
            queries.append(i % 10 == 0 ? QStringLiteral("X%1").arg(i)
                                       : bookId(1 + (i * 37) % size));
        }

        std::array<qint64, 3> sequentialTimes;
        std::array<qint64, 3> hashTimes;
        for (int run = 0; run < 3; ++run) {
            const Timing sequential = measure(repository, queries, false);
            const Timing hash = measure(repository, queries, true);
            if (sequential.checksum != hash.checksum)
                return 2;
            sequentialTimes[run] = sequential.nanoseconds;
            hashTimes[run] = hash.nanoseconds;
        }
        const double sequentialMs = median(sequentialTimes) / 1e6;
        const double hashMs = median(hashTimes) / 1e6;
        out << size << ",100000,101," << sequentialMs << ',' << hashMs << ','
            << sequentialMs / hashMs << '\n';
    }
    return 0;
}
