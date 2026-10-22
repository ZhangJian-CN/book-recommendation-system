// 共用业务测试简述：在临时样例中核对数据校验、图书操作、评分与推荐手算结果。
#include "bookindex.h"
#include "booksearch.h"
#include "librarystore.h"
#include "recommendation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>

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

    BookIndex collisionIndex(1);
    collisionIndex.build(store.books().all());
    if (collisionIndex.bucketSize(0) != 4
        || collisionIndex.rowForId(QStringLiteral("B004")) != 3)
        return 4;
    collisionIndex.erase(QStringLiteral("B002"));
    if (collisionIndex.rowForId(QStringLiteral("B002")) != -1
        || collisionIndex.rowForId(QStringLiteral("B004")) != 3)
        return 5;
    BookRepository collisionRepository(1);
    if (!collisionRepository.load(temp.filePath(QStringLiteral("books.csv")), &error)
        || !collisionRepository.findById(QStringLiteral("B004")))
        return 18;
    const auto matches = BookSearch::keywordRows(store.books().all(),
                                                  QStringLiteral("布莱恩特"));
    if (matches.size() != 1 || store.books().all()[matches[0]].id != QStringLiteral("B004"))
        return 6;

    RecommendationService recommendation(store.books(), store.users(), store.ratings());
    if (std::abs(recommendation.similarity(QStringLiteral("U0"), QStringLiteral("U1")) - 1.0) > 1e-9
        || std::abs(recommendation.similarity(QStringLiteral("U0"), QStringLiteral("U2")) - 5.0 / 13) > 1e-9)
        return 7;
    const auto top = recommendation.recommend(QStringLiteral("U0"), 2);
    if (top.size() != 2 || top[0].bookId != QStringLiteral("B003")
        || top[1].bookId != QStringLiteral("B004")
        || top[0].source != RecommendationSource::Collaborative
        || std::abs(top[0].score - 35.0 / 9) > 1e-9
        || std::abs(top[1].score - 19.0 / 9) > 1e-9)
        return 8;
    BookRepository reversed;
    for (const QString &id : {QStringLiteral("B001"), QStringLiteral("B002"),
                              QStringLiteral("B004"), QStringLiteral("B003")}) {
        if (!reversed.insert(*store.books().findById(id), &error))
            return 24;
    }
    RecommendationService heapCheck(reversed, store.users(), store.ratings());
    const auto best = heapCheck.recommend(QStringLiteral("U0"), 1);
    if (best.size() != 1 || best[0].bookId != QStringLiteral("B003"))
        return 25;
    Book unavailable = *reversed.findById(QStringLiteral("B003"));
    unavailable.stock = 0;
    if (!reversed.update(unavailable, &error)
        || RecommendationService(reversed, store.users(), store.ratings())
               .recommend(QStringLiteral("U0"), 1)[0].bookId != QStringLiteral("B004"))
        return 26;

    if (store.deleteBook(QStringLiteral("B001"), &error))
        return 9;
    if (!store.addBook({QStringLiteral("B005"), QStringLiteral("新书"),
                        QStringLiteral("作者"), QStringLiteral("计算机"), 1}, &error)
        || store.addBook({QStringLiteral("B005"), QStringLiteral("重复"),
                          QStringLiteral("作者"), QStringLiteral("计算机"), 1}, &error))
        return 10;
    if (!store.addBook({QStringLiteral("B006"), QStringLiteral("另一新书"),
                        QStringLiteral("作者"), QStringLiteral("计算机"), 1}, &error))
        return 19;
    if (!store.deleteBook(QStringLiteral("B005"), &error)
        || store.books().findById(QStringLiteral("B005"))
        || !store.books().findById(QStringLiteral("B006"))
        || !store.books().findById(QStringLiteral("B004")))
        return 11;
    if (!store.deleteBook(QStringLiteral("B006"), &error))
        return 20;

    if (!store.rate(QStringLiteral("U0"), QStringLiteral("B003"), 4, &error))
        return 12;
    store = LibraryStore{}; // Restart after releasing the original writer.
    LibraryStore restarted;
    if (!restarted.load(temp.path(), &error))
        return 13;
    bool foundRating = false;
    for (const Rating &rating : restarted.ratings()) {
        if (rating.userId == QStringLiteral("U0") && rating.bookId == QStringLiteral("B003"))
            foundRating = rating.score == 4;
    }
    if (!foundRating || restarted.deleteBook(QStringLiteral("B003"), &error))
        return 14;

    if (!restarted.addBook({QStringLiteral("B007"), QStringLiteral("借阅过的书"),
                            QStringLiteral("作者"), QStringLiteral("计算机"), 1}, &error))
        return 21;
    QFile borrowed(temp.filePath(QStringLiteral("borrow_records.csv")));
    if (!borrowed.open(QIODevice::Append)
        || borrowed.write("R003,U1,B007,2026-09-20,2026-09-21,returned\n") < 0)
        return 22;
    borrowed.close();
    if (!restarted.load(temp.path(), &error)
        || restarted.deleteBook(QStringLiteral("B007"), &error))
        return 23;

    QFile users(temp.filePath(QStringLiteral("users.csv")));
    if (!users.open(QIODevice::Append) || users.write("U3,New User\n") < 0)
        return 15;
    users.close();
    if (!restarted.load(temp.path(), &error))
        return 16;
    RecommendationService cold(restarted.books(), restarted.users(), restarted.ratings());
    const auto popular = cold.recommend(QStringLiteral("U3"), 2);
    if (cold.similarity(QStringLiteral("U3"), QStringLiteral("U1")) != 0.0
        || popular.size() != 2 || popular[0].source != RecommendationSource::Popular
        || popular[0].bookId != QStringLiteral("B001"))
        return 17;
    return 0;
}
