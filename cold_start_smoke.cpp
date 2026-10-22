// 冷启动规则验收：使用独立临时副本，原始演示数据不参与写回。
#include "recommendation.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>

#define CHECK(condition) do { if (!(condition)) { \
    qCritical() << "Line" << __LINE__ << ":" << #condition; return false; } } while (false)

namespace {
bool initialPopular(const QVector<Recommendation> &results)
{
    CHECK(results.size() == 4);
    const QStringList ids{"B001", "B002", "B003", "B004"};
    const int counts[]{3, 3, 2, 2};
    const double scores[]{11.0 / 3, 7.0 / 3, 3.0, 3.0};
    for (int i = 0; i < results.size(); ++i) {
        CHECK(results[i].bookId == ids[i]);
        CHECK(results[i].ratingCount == counts[i]);
        CHECK(std::abs(results[i].score - scores[i]) < 1e-9);
        CHECK(results[i].source == RecommendationSource::Popular);
    }
    return true;
}

bool rankingAndBoundaries()
{
    QString error;
    BookRepository books;
    for (const QString &id : QStringList{"B", "E", "D", "G", "F", "A", "C"})
        CHECK(books.insert({id, id, "Author", "Category", id == "G" ? 0 : 1}, &error));
    QVector<User> users{{"U1", "One"}, {"U2", "Two"}, {"NEW", "New"}};
    QVector<Rating> ratings{{"U1", "A", 1}, {"U2", "A", 1}, {"U1", "B", 5},
                            {"U1", "C", 4}, {"U2", "C", 4}, {"U1", "D", 4},
                            {"U2", "D", 4}, {"U1", "E", 3}, {"U2", "E", 3},
                            {"U1", "G", 5}, {"U2", "G", 5}};
    RecommendationService service(books, users, ratings);
    const QStringList expected{"C", "D", "E", "A", "B"};
    const auto results = service.popularTopK(99);
    CHECK(results.size() == expected.size());
    for (int i = 0; i < results.size(); ++i)
        CHECK(results[i].bookId == expected[i]);
    CHECK(results[3].ratingCount == 2 && results[3].score == 1.0);
    CHECK(results[4].ratingCount == 1 && results[4].score == 5.0);
    CHECK(service.popularTopK(1).first().bookId == "C");
    CHECK(service.popularTopK(0).isEmpty() && service.popularTopK(-2).isEmpty());
    CHECK(service.recommend("NEW", 0).isEmpty() && service.recommend("NEW", -1).isEmpty());
    CHECK(service.recommend("missing", 10).isEmpty());
    CHECK(!service.hasRatings("missing") && !service.hasRatings("NEW") && service.hasRatings("U1"));
    // 图书、用户和评分都反向插入；堆替换和并列排序仍得到同一份榜单。
    BookRepository reversed;
    for (auto i = books.all().crbegin(); i != books.all().crend(); ++i)
        CHECK(reversed.insert(*i, &error));
    std::reverse(users.begin(), users.end());
    std::reverse(ratings.begin(), ratings.end());
    RecommendationService reverseService(reversed, users, ratings);
    for (int k : {1, 2, 3, 4, 5, 99}) {
        const auto top = reverseService.popularTopK(k);
        CHECK(top.size() == qMin(k, int(expected.size())));
        for (int i = 0; i < top.size(); ++i)
            CHECK(top[i].bookId == expected[i]);
    }
    // 首次评分只保证进入个人流程；无共同评分时依然允许逐候选热门回退。
    users.append(User{"ISOLATED", "No common ratings"});
    ratings.append(Rating{"ISOLATED", "F", 4});
    RecommendationService isolated(books, users, ratings);
    CHECK(isolated.hasRatings("ISOLATED"));
    const auto fallback = isolated.recommend("ISOLATED", 99);
    CHECK(fallback.size() == expected.size());
    for (const auto &item : fallback) {
        CHECK(item.bookId != "F");
        CHECK(item.source == RecommendationSource::Popular);
    }
    CHECK(RecommendationService(books, users, {}).popularTopK(10).isEmpty());
    CHECK(RecommendationService(BookRepository{}, {}, {}).popularTopK(10).isEmpty());
    for (Book book : books.all()) {
        book.stock = 0;
        CHECK(books.update(book, &error));
    }
    CHECK(RecommendationService(books, users, ratings).popularTopK(10).isEmpty());
    qInfo() << "CS04-CS07 PASS: count/mean/ID order, insertion order, availability, empty data, K boundaries";
    return true;
}

bool coldStartAndPersistence()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    for (const QString &name : QStringList{"books.csv", "users.csv", "ratings.csv", "borrow_records.csv"})
        CHECK(QFile::copy(QDir(QStringLiteral(SAMPLE_COLD_START_DATA_DIR)).filePath(name), dir.filePath(name)));
    LibraryStore store;
    QString error;
    CHECK(store.load(dir.path(), &error));
    CHECK(store.books().all().size() == 4 && store.users().size() == 4);
    CHECK(store.ratings().size() == 10 && store.borrowRecords().size() == 2);
    RecommendationService initial(store.books(), store.users(), store.ratings());
    CHECK(initialPopular(initial.popularTopK(4)));
    CHECK(initialPopular(initial.recommend("UCOLD", 4)));
    CHECK(!initial.hasRatings("UCOLD"));
    CHECK(initial.recommend("missing", 4).isEmpty() && store.users().size() == 4);
    qInfo() << "CS01-CS03 PASS: 4/4/10/2, exact global and cold-start ranking, unknown user rejected";
    const auto oldUser = initial.recommend("U0", 2);
    CHECK(oldUser.size() == 2 && oldUser[0].bookId == "B003" && oldUser[1].bookId == "B004");
    CHECK(std::abs(oldUser[0].score - 35.0 / 9) < 1e-9);
    CHECK(std::abs(oldUser[1].score - 19.0 / 9) < 1e-9);
    CHECK(oldUser[0].source == RecommendationSource::Collaborative);
    qInfo() << "CS14 PASS: U0 original collaborative predictions unchanged";

    CHECK(store.rate("U0", "B001", 4, &error));
    CHECK(store.ratings().size() == 10);
    const auto updated = RecommendationService(store.books(), store.users(), store.ratings()).popularTopK(4);
    CHECK(updated[0].ratingCount == 3 && std::abs(updated[0].score - 10.0 / 3) < 1e-9);
    CHECK(store.rate("U0", "B001", 5, &error));
    CHECK(store.ratings().size() == 10);
    qInfo() << "CS08 PASS: updating an existing rating changes the mean without adding a voter or record";

    CHECK(store.rate("UCOLD", "B001", 4, &error));
    CHECK(store.ratings().size() == 11);
    RecommendationService rated(store.books(), store.users(), store.ratings());
    CHECK(rated.hasRatings("UCOLD"));
    const auto personal = rated.recommend("UCOLD", 4);
    const QStringList expected{"B003", "B004", "B002"};
    const double scores[]{3.0, 3.0, 7.0 / 3};
    CHECK(personal.size() == 3);
    for (int i = 0; i < personal.size(); ++i) {
        CHECK(personal[i].bookId == expected[i]);
        CHECK(std::abs(personal[i].score - scores[i]) < 1e-9);
        CHECK(personal[i].source == RecommendationSource::Collaborative);
    }
    const auto global = rated.popularTopK(4);
    CHECK(global[0].bookId == "B001" && global[0].ratingCount == 4 && global[0].score == 3.75);
    qInfo() << "CS09-CS10 PASS: first rating switches to personal candidates; global B001 remains 4 voters / 3.7500";

    QString record;
    const QString date = QDate::currentDate().toString(Qt::ISODate);
    CHECK(store.borrowBook("UCOLD", "B003", date, &record, &error));
    CHECK(store.books().findById("B003")->stock == 0);
    const auto borrowed = RecommendationService(store.books(), store.users(), store.ratings()).popularTopK(4);
    CHECK(borrowed.size() == 3);
    for (const auto &item : borrowed)
        CHECK(item.bookId != "B003");
    CHECK(store.returnBook(record, date, &error));
    CHECK(RecommendationService(store.books(), store.users(), store.ratings()).popularTopK(4).size() == 4);
    store = LibraryStore{}; // Release before independently loading saved data.
    LibraryStore restarted;
    CHECK(restarted.load(dir.path(), &error));
    CHECK(restarted.ratings().size() == 11 && restarted.users().size() == 4);
    CHECK(restarted.books().findById("B003")->stock == 1);
    RecommendationService afterRestart(restarted.books(), restarted.users(), restarted.ratings());
    CHECK(afterRestart.hasRatings("UCOLD"));
    CHECK(afterRestart.recommend("UCOLD", 4).first().bookId == "B003");
    CHECK(afterRestart.popularTopK(4).first().score == 3.75);
    qInfo() << "CS12-CS13 PASS: borrow/return ranking and saved first rating survive reload; UI checks run in ui_polish_smoke";
    return true;
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return rankingAndBoundaries() && coldStartAndPersistence() ? 0 : 1;
}
