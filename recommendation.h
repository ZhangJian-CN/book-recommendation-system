#ifndef RECOMMENDATION_H
#define RECOMMENDATION_H

#include "librarystore.h"

#include <QHash>

enum class RecommendationSource { Collaborative, Popular };

struct Recommendation {
    QString bookId;
    QString title;
    double score = 0.0;
    int ratingCount = 0;
    RecommendationSource source = RecommendationSource::Popular;
};

class RecommendationService {
public:
    RecommendationService(const BookRepository &books, const QVector<User> &users,
                          const QVector<Rating> &ratings);
    double similarity(const QString &userA, const QString &userB) const;
    QVector<Recommendation> recommend(const QString &userId, int topK) const;

private:
    const BookRepository &books_;
    QVector<User> users_;
    QVector<QVector<int>> matrix_;
    QHash<QString, int> userRows_;
    QHash<QString, int> bookColumns_;
};

#endif // RECOMMENDATION_H
