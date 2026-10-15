#ifndef RECOMMENDATION_H
#define RECOMMENDATION_H

#include "librarystore.h"

#include <QHash>

//动态数组+用户——图书评分矩阵

// 推荐来源区分“相似用户加权预测”和“没有有效邻居时的热门回退”。
enum class RecommendationSource { Collaborative, Popular };

struct Recommendation {
    // 编号用于定位图书实体，书名用于显示推荐结果。
    QString bookId;
    QString title;
    // 协同来源时为预测分；热门来源时为已有评分平均值，无人评分则为 0。
    double score = 0.0;
    // 给这本书评分的用户数，热门回退排名优先比较此值。
    int ratingCount = 0;
    RecommendationSource source = RecommendationSource::Popular;
};

// 每次界面请求推荐时构造服务，用当前“图书、用户”和评分建立计算用矩阵。
class RecommendationService {
public:
    // 输入当前图书仓库、用户数组、评分数组，建立编号映射和 U*B “评分矩阵”。
    RecommendationService(const BookRepository &books, const QVector<User> &users,
                          const QVector<Rating> &ratings);
    // 输入两个用户编号，输出共同评分余弦相似度；无效编号或无共同评分返回 0。
    double similarity(const QString &userA, const QString &userB) const;//接口
    // 输入目标用户及 K，输出排好序的至多 K 项；候选不足时不凑满 K 项。
    QVector<Recommendation> recommend(const QString &userId, int topK) const;

private:
    // 保留仓库的只读引用；界面创建服务后立即计算，仓库在此期间仍然存在。
    const BookRepository &books_;
    QVector<User> users_;
    // 稠密评分矩阵：行对应用户、列对应图书；0 表示未评分，实际评分为 1～5。
    // matrix_[u][b] 直接访问一名用户对一本书的评分；空间随 U*B 增长。
    QVector<QVector<int>> matrix_;
    // 把业务编号转换成矩阵下标；这是 Qt 的辅助映射，项目实现的图书哈希索引在 BookIndex。
    QHash<QString, int> userRows_;
    QHash<QString, int> bookColumns_;
};

#endif // RECOMMENDATION_H
