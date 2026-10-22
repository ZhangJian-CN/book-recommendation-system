#ifndef RECOMMENDATION_H
#define RECOMMENDATION_H

#include "librarystore.h"

#include <QHash>

// 推荐侧接口简述：读取图书、用户、评分，输出可显示的推荐条目；不在本模块保存 CSV。
// 与图书侧联调时关注传入实体、已评分排除和库存过滤，矩阵/相似度/堆是推荐侧算法。

// 推荐来源区分“相似用户加权预测”和“热门榜单/热门回退”。
enum class RecommendationSource { Collaborative, Popular }; // 两种来源：协同预测、热门统计。

struct Recommendation { // 一条推荐输出，打包编号、书名、分数、评分人数及来源供界面展示。
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
class RecommendationService { // 推荐算法服务类，组织评分矩阵、相似度与 Top K 计算。
public: // 对外提供构造与计算接口。
    // 输入当前图书仓库、用户数组、评分数组，建立编号映射和 U*B “评分矩阵”。
    RecommendationService(const BookRepository &books, const QVector<User> &users,
                          const QVector<Rating> &ratings); // 根据当前记录创建评分矩阵和编号映射。
    // 输入两个用户编号，输出共同评分余弦相似度；无效编号或无共同评分返回 0。
    double similarity(const QString &userA, const QString &userB) const; // 返回两用户共同评分的余弦相似度。
    // 输入目标用户及 K，输出排好序的至多 K 项；候选不足时不凑满 K 项。
    QVector<Recommendation> recommend(const QString &userId, int topK) const; // 返回排好序的至多 topK 条推荐。
    // 无需用户编号；只选有评分且库存大于 0 的图书，按人数、平均分、编号排序。
    QVector<Recommendation> popularTopK(int topK) const;
    // 是否有正评分；未知用户返回 false，调用方仍需单独验证用户存在。
    bool hasRatings(const QString &userId) const;

private: // 计算所需的内部数据，不作为界面直接修改入口。
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
