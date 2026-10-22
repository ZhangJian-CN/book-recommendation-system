// 独立评估入口：复用生产推荐实现，输出真实划分、逐用户指标与 Release 交互耗时。
#include "mainwindow.h"
#include "recommendation.h"
#include "csvutil.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QSpinBox>
#include <QSysInfo>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr int k = 5;
constexpr unsigned seed = 20261022;
QString number(double value) { return QString::number(value, 'f', 9); }

bool writeJson(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

QString digest(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
}

bool writeRatings(const QString &path, QVector<Rating> ratings, QString *error)
{
    std::sort(ratings.begin(), ratings.end(), [](const Rating &a, const Rating &b) {
        return a.userId == b.userId ? a.bookId < b.bookId : a.userId < b.userId;
    });
    QVector<QStringList> rows;
    for (const auto &rating : ratings)
        rows.append({rating.userId, rating.bookId, QString::number(rating.score)});
    return Csv::write(path, "user_id,book_id,score", rows, error);
}

struct Summary {
    double precisionAll = 0, precision = 0, recall = 0, ndcg = 0;
    int valid = 0, shortLists = 0, returned = 0;
    QSet<QString> coverage;
};

bool evaluate(const QString &input, const QString &output)
{
    LibraryStore store;
    QString error;
    if (!store.load(input, &error) || !QDir().mkpath(output)) {
        qCritical().noquote() << "Input/output error:" << error;
        return false;
    }
    QVector<User> users = store.users();
    std::sort(users.begin(), users.end(), [](const User &a, const User &b) { return a.id < b.id; });
    if (users.isEmpty()) return false;
    std::mt19937 rng(seed);
    QVector<Rating> training, test;
    QHash<QString, QSet<QString>> trained, relevant;
    QHash<QString, int> testCounts;
    for (const auto &user : users) {
        QVector<Rating> ratings;
        for (const auto &rating : store.ratings())
            if (rating.userId == user.id) ratings.append(rating);
        std::sort(ratings.begin(), ratings.end(), [](const Rating &a, const Rating &b) { return a.bookId < b.bookId; });
        std::shuffle(ratings.begin(), ratings.end(), rng);
        const int count = ratings.size() < 2 ? 0 : qBound(1, int(std::lround(ratings.size() * 0.2)), int(ratings.size()) - 1);
        for (int i = 0; i < ratings.size(); ++i) {
            const auto &rating = ratings[i];
            if (i < count) {
                test.append(rating);
                ++testCounts[user.id];
                const Book *book = store.books().findById(rating.bookId);
                if (rating.score >= 4 && book && book->stock > 0) relevant[user.id].insert(rating.bookId);
            } else {
                training.append(rating);
                trained[user.id].insert(rating.bookId);
            }
        }
    }
    // 所有测试评分同时移除；既不参与相似度，也不参与热门人数/均分。
    if (training.size() + test.size() != store.ratings().size()) return false;
    for (const auto &rating : test)
        if (trained[rating.userId].contains(rating.bookId)) return false;
    const QDir out(output);
    if (!writeRatings(out.filePath("train_ratings.csv"), training, &error)
        || !writeRatings(out.filePath("test_ratings.csv"), test, &error)) return false;
    RecommendationService service(store.books(), users, training);
    const auto allPopular = service.popularTopK(store.books().all().size());
    Summary summary[2];
    const QStringList methods{"personalized", "popular"};
    QVector<QStringList> rows;
    int available = 0;
    for (const Book &book : store.books().all()) if (book.stock > 0) ++available;
    for (const auto &user : users) {
        int candidates = 0;
        for (const auto &book : store.books().all())
            if (book.stock > 0 && !trained[user.id].contains(book.id)) ++candidates;
        QVector<Recommendation> results[2];
        results[0] = service.recommend(user.id, k);
        // 先过滤全部热门候选，再截 K，避免先取 Top K 后排除已评导致短列表。
        for (const auto &item : allPopular) {
            if (!trained[user.id].contains(item.bookId) && results[1].size() < k) results[1].append(item);
        }
        for (int method = 0; method < 2; ++method) {
            int hits = 0;
            double dcg = 0, ideal = 0;
            QStringList ids;
            QSet<QString> unique;
            for (int rank = 0; rank < results[method].size(); ++rank) {
                const auto &item = results[method][rank];
                const Book *book = store.books().findById(item.bookId);
                if (!book || book->stock <= 0 || trained[user.id].contains(item.bookId) || unique.contains(item.bookId))
                    return false;
                unique.insert(item.bookId);
                ids.append(item.bookId);
                summary[method].coverage.insert(item.bookId);
                if (relevant[user.id].contains(item.bookId)) {
                    ++hits;
                    dcg += 1.0 / std::log2(rank + 2.0);
                }
            }
            for (int rank = 0; rank < qMin(k, int(relevant[user.id].size())); ++rank)
                ideal += 1.0 / std::log2(rank + 2.0);
            const double precision = double(hits) / k; // 缺位仍计入分母。
            const bool valid = !relevant[user.id].isEmpty();
            const double recall = valid ? double(hits) / relevant[user.id].size() : 0;
            const double ndcg = valid ? dcg / ideal : 0;
            auto &s = summary[method];
            s.precisionAll += precision;
            s.returned += results[method].size();
            if (results[method].size() < k) ++s.shortLists;
            if (valid) { ++s.valid; s.precision += precision; s.recall += recall; s.ndcg += ndcg; }
            rows.append({methods[method], user.id, QString::number(testCounts[user.id]), QString::number(relevant[user.id].size()),
                QString::number(results[method].size()), QString::number(hits), number(precision),
                valid ? number(recall) : QString(), valid ? number(ndcg) : QString(), QString::number(candidates), ids.join('|')});
        }
    }
    if (!Csv::write(out.filePath("per_user.csv"), "method,user_id,test_count,relevant_count,returned,hits,precision_at_5,recall_at_5,ndcg_at_5,eligible_candidates,ranked_ids", rows, &error)) return false;
    QVector<QStringList> summaries;
    for (int method = 0; method < 2; ++method) {
        const auto &s = summary[method];
        summaries.append({methods[method], QString::number(users.size()), QString::number(s.valid), QString::number(users.size() - s.valid),
            number(s.precisionAll / users.size()), s.valid ? number(s.precision / s.valid) : QString(),
            s.valid ? number(s.recall / s.valid) : QString(), s.valid ? number(s.ndcg / s.valid) : QString(),
            number(double(s.returned) / users.size()), QString::number(s.shortLists), QString::number(s.coverage.size()),
            available ? number(double(s.coverage.size()) / available) : QString()});
    }
    if (!Csv::write(out.filePath("summary.csv"), "method,users,valid_users,excluded_users,precision_all_users,precision_at_5_valid,recall_at_5_valid,ndcg_at_5_valid,mean_returned,short_lists,unique_recommended_books,catalog_coverage", summaries, &error)) return false;

    // 交互测量使用完整 800 条评分的独立副本；加载 CSV 和首次建窗不计入请求耗时。
    QTemporaryDir uiData;
    if (!uiData.isValid()) return false;
    for (const QString &name : QStringList{"books.csv", "users.csv", "ratings.csv", "borrow_records.csv"})
        if (!QFile::copy(QDir(input).filePath(name), uiData.filePath(name))) return false;
    MainWindow window(uiData.path());
    window.resize(1280, 720);
    window.show();
    window.findChild<QTabWidget *>("mainTabs")->setCurrentIndex(1);
    window.findChild<QSpinBox *>("topK")->setValue(k);
    QApplication::processEvents();
    qint64 matrixNs = 0, computeNs = 0, modelNs = 0;
    QObject::connect(&window, &MainWindow::recommendationMeasured, &window, [&](qint64 a, qint64 b, qint64 c) {
        matrixNs = a; computeNs = b; modelNs = c;
    });
    QVector<QStringList> timings;
    QVector<double> phases[4];
    for (int request = -3; request < 40; ++request) {
        const QString user = users[(request + 3) % users.size()].id;
        window.findChild<QLineEdit *>("recommendUser")->setText(user);
        QElapsedTimer total; total.start();
        window.findChild<QPushButton *>("recommendButton")->click();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        window.repaint();
        const qint64 totalNs = total.nsecsElapsed();
        if (request < 0) continue;
        const double matrix = matrixNs / 1e6, compute = computeNs / 1e6;
        const double ui = (totalNs - matrixNs - computeNs) / 1e6;
        timings.append({QString::number(request + 1), user, number(matrix), number(compute), number(modelNs / 1e6), number(ui), number(totalNs / 1e6)});
        phases[0].append(matrix); phases[1].append(compute); phases[2].append(ui); phases[3].append(totalNs / 1e6);
    }
    if (!Csv::write(out.filePath("latency_requests.csv"), "request,user_id,matrix_ms,compute_ms,model_fill_ms,ui_refresh_ms,total_ms", timings, &error)) return false;
    QVector<QStringList> latency;
    const QStringList names{"matrix", "compute", "ui_refresh", "total"};
    for (int phase = 0; phase < 4; ++phase) {
        auto samples = phases[phase];
        std::sort(samples.begin(), samples.end());
        const int n = samples.size();
        latency.append({names[phase], QString::number(n), number((samples[n / 2 - 1] + samples[n / 2]) / 2),
            number(samples[int(std::ceil(n * .95)) - 1]), number(samples.last())});
    }
    if (!Csv::write(out.filePath("latency_summary.csv"), "phase,requests,median_ms,p95_ms,max_ms", latency, &error)) return false;
    QJsonObject inputs;
    for (const QString &name : QStringList{"books.csv", "users.csv", "ratings.csv", "borrow_records.csv"})
        inputs.insert(name, digest(QDir(input).filePath(name)));
    QJsonArray sequence;
    for (const auto &row : timings) sequence.append(row[1]);
    QJsonObject manifest{{"seed", int(seed)}, {"k", k}, {"train_fraction", 0.8}, {"relevance_min_score", 4},
        {"split", "sort users by ID; sort each user's ratings by book ID; shuffle with shared std::mt19937; nearest 20% held out"},
        {"books", int(store.books().all().size())}, {"users", int(users.size())}, {"ratings", int(store.ratings().size())},
        {"training_ratings", int(training.size())}, {"test_ratings", int(test.size())}, {"input_sha256", inputs},
        {"train_sha256", digest(out.filePath("train_ratings.csv"))}, {"test_sha256", digest(out.filePath("test_ratings.csv"))},
        {"qt", qVersion()}, {"compiler", __VERSION__}, {"build_type", EVALUATION_BUILD_TYPE},
        {"os", QSysInfo::prettyProductName()}, {"cpu_arch", QSysInfo::currentCpuArchitecture()},
        {"processor", qEnvironmentVariable("PROCESSOR_IDENTIFIER")}, {"logical_processors", QThread::idealThreadCount()},
        {"platform", QApplication::platformName()}, {"window", "1280x720"}, {"warmup_requests", 3}, {"measured_requests", 40},
        {"latency_uses_full_ratings", true}, {"latency_excludes_csv_load", true}, {"user_sequence", sequence},
        {"ui_refresh", "widget fill, event processing and synchronous repaint; includes status/dispatch overhead; offscreen has no physical display scanout"},
        {"generated_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    if (!writeJson(out.filePath("manifest.json"), manifest)) return false;
    qInfo().noquote() << "Evaluation saved to" << QDir(output).absolutePath() << "training/test" << training.size() << test.size();
    for (const auto &row : summaries) qInfo().noquote() << row.join(',');
    for (const auto &row : latency) qInfo().noquote() << row.join(',');
    return true;
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QApplication app(argc, argv);
    if (app.platformName() == "offscreen" && QFile::exists("C:/Windows/Fonts/msyh.ttc"))
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    if (app.arguments().size() != 3) {
        qCritical() << "Usage: recommendation_evaluation <Goodbooks CSV directory> <output directory>";
        return 2;
    }
    return evaluate(app.arguments()[1], app.arguments()[2]) ? 0 : 1;
}
