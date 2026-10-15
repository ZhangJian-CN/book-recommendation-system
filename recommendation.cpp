#include "recommendation.h"

#include <algorithm>
#include <cmath>

namespace {

bool better(const Recommendation &a, const Recommendation &b)
{
    if (a.source != b.source)
        return a.source == RecommendationSource::Collaborative;
    if (a.source == RecommendationSource::Popular && a.ratingCount != b.ratingCount)
        return a.ratingCount > b.ratingCount;
    if (a.score != b.score)
        return a.score > b.score;
    return a.bookId < b.bookId;
}

class TopKHeap {
public:
    explicit TopKHeap(int capacity) : capacity_(capacity) {}

    void add(const Recommendation &item)
    {
        if (capacity_ <= 0)
            return;
        if (items_.size() < capacity_) {
            items_.append(item);
            for (int i = items_.size() - 1; i > 0;) {
                const int parent = (i - 1) / 2;
                if (!better(items_[parent], items_[i]))
                    break;
                std::swap(items_[parent], items_[i]);
                i = parent;
            }
        } else if (better(item, items_[0])) {
            items_[0] = item;
            for (int i = 0;;) {
                const int left = i * 2 + 1;
                if (left >= items_.size())
                    break;
                const int right = left + 1;
                int worst = left;
                if (right < items_.size() && better(items_[left], items_[right]))
                    worst = right;
                if (!better(items_[i], items_[worst]))
                    break;
                std::swap(items_[i], items_[worst]);
                i = worst;
            }
        }
    }

    QVector<Recommendation> sorted() const
    {
        QVector<Recommendation> result = items_;
        std::sort(result.begin(), result.end(), better);
        return result;
    }

private:
    int capacity_;
    QVector<Recommendation> items_;
};

} // namespace

RecommendationService::RecommendationService(const BookRepository &books,
                                             const QVector<User> &users,
                                             const QVector<Rating> &ratings)
    : books_(books), users_(users)
{
    for (int i = 0; i < users_.size(); ++i)
        userRows_.insert(users_[i].id, i);
    for (int i = 0; i < books_.all().size(); ++i)
        bookColumns_.insert(books_.all()[i].id, i);
    matrix_.resize(users_.size());
    for (auto &row : matrix_)
        row.fill(0, books_.all().size());
    for (const Rating &rating : ratings) {
        const int user = userRows_.value(rating.userId, -1);
        const int book = bookColumns_.value(rating.bookId, -1);
        if (user >= 0 && book >= 0)
            matrix_[user][book] = rating.score;
    }
}

double RecommendationService::similarity(const QString &userA,
                                         const QString &userB) const
{
    const int a = userRows_.value(userA, -1);
    const int b = userRows_.value(userB, -1);
    if (a < 0 || b < 0)
        return 0.0;
    double dot = 0.0;
    double squareA = 0.0;
    double squareB = 0.0;
    for (int book = 0; book < books_.all().size(); ++book) {
        const int scoreA = matrix_[a][book];
        const int scoreB = matrix_[b][book];
        if (scoreA > 0 && scoreB > 0) {
            dot += scoreA * scoreB;
            squareA += scoreA * scoreA;
            squareB += scoreB * scoreB;
        }
    }
    return squareA > 0 && squareB > 0 ? dot / std::sqrt(squareA * squareB) : 0.0;
}

QVector<Recommendation> RecommendationService::recommend(const QString &userId,
                                                          int topK) const
{
    const int target = userRows_.value(userId, -1);
    if (target < 0 || topK <= 0)
        return {};
    QVector<double> similarities(users_.size());
    for (int user = 0; user < users_.size(); ++user) {
        if (user != target)
            similarities[user] = similarity(userId, users_[user].id);
    }

    TopKHeap heap(topK);
    for (int col = 0; col < books_.all().size(); ++col) {
        const Book &book = books_.all()[col];
        if (matrix_[target][col] > 0 || book.stock <= 0)
            continue;
        double weighted = 0.0;
        double weight = 0.0;
        double total = 0.0;
        int count = 0;
        for (int user = 0; user < users_.size(); ++user) {
            const int score = matrix_[user][col];
            if (score <= 0)
                continue;
            ++count;
            total += score;
            if (user != target && similarities[user] > 0.0) {
                weighted += similarities[user] * score;
                weight += similarities[user];
            }
        }
        const RecommendationSource source = weight > 0.0
                                                ? RecommendationSource::Collaborative
                                                : RecommendationSource::Popular;
        heap.add({book.id, book.title, weight > 0.0 ? weighted / weight
                                                      : (count > 0 ? total / count : 0.0),
                  count, source});
    }
    return heap.sorted();
}
