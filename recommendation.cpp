// 中检讲解：本文件集中实现评分矩阵、用户协同过滤和 Top K 小顶堆。
// 推荐流程：建立矩阵 -> 计算共同评分相似度 -> 预测候选分数 -> 堆选取 -> 排序输出。
#include "recommendation.h"

#include <algorithm>
#include <cmath>

namespace {

bool better(const Recommendation &a, const Recommendation &b)
{
    // 统一优先级：协同过滤优先；热门项先比评分人数；再比分数，最后按编号升序。
    // 堆维护和最终排序共用此规则，因此这里的“最小”指推荐优先级最低。
    // 返回 true 表示 a 应排在 b 前面；热门项并非仅按平均分排序。
    if (a.source != b.source)
        return a.source == RecommendationSource::Collaborative;
    if (a.source == RecommendationSource::Popular && a.ratingCount != b.ratingCount)
        return a.ratingCount > b.ratingCount;
    if (a.score != b.score)
        return a.score > b.score;
    // 前面的条件全部相同时以编号打破平局，使多次运行的排名保持确定。
    return a.bookId < b.bookId;
}

//topK小顶堆算法：怎么高效的选出推荐分数最高的K本书

// 动态数组表示完全二叉树：父结点 (i-1)/2，左右孩子 2*i+1、2*i+2。
// 最多保留 K 项，堆顶是已保留项中最差的一项，便于用更好的候选替换。
class TopKHeap {
public:
    explicit TopKHeap(int capacity) : capacity_(capacity) {}

    void add(const Recommendation &item)
    {
        // 输入一个已计算分数的候选；只维护保留集合，不产生最终排名。
        // 最坏沿树高上浮或下沉 O(log K)，数组保存至多 K 项，空间 O(K)。
        if (capacity_ <= 0)
            return;
        if (items_.size() < capacity_) {
            // 堆未满：追加到数组末尾，再上浮较差的项，保持堆顶最差。
            items_.append(item);
            for (int i = items_.size() - 1; i > 0;) {
                const int parent = (i - 1) / 2;
                // 父结点没有比子结点更好，说明“较差项在上方”的堆序已满足。
                if (!better(items_[parent], items_[i]))
                    break;
                std::swap(items_[parent], items_[i]);
                i = parent;
            }
        } else if (better(item, items_[0])) {
            // 堆已满：只有候选优于堆顶才替换，否则直接舍弃该候选。
            items_[0] = item;
            for (int i = 0;;) {
                const int left = i * 2 + 1;
                // 没有左孩子就一定是叶结点，无需继续下沉。
                if (left >= items_.size())
                    break;
                const int right = left + 1;
                // 选左右孩子中较差的一项；若父结点更好，就交换并继续下沉。
                int worst = left;
                if (right < items_.size() && better(items_[left], items_[right]))
                    worst = right;
                // 父结点已不优于最差孩子，父子位置正确，停止交换。
                if (!better(items_[i], items_[worst]))
                    break;
                std::swap(items_[i], items_[worst]);
                i = worst;
            }
        }
    }

//创建评分矩阵

    QVector<Recommendation> sorted() const
    {
        // 堆保证选出的集合正确，数组本身并非完整排名；复制后按优先级排序输出。
        QVector<Recommendation> result = items_;
        std::sort(result.begin(), result.end(), better);
        return result;
    }

private:
    int capacity_;
    QVector<Recommendation> items_;
};

} // namespace

//协同过滤算法

RecommendationService::RecommendationService(const BookRepository &books,
                                             const QVector<User> &users,
                                             const QVector<Rating> &ratings)
    : books_(books), users_(users)
{
    // 评分矩阵第 1 步：建立用户编号到行、图书编号到列的映射。
    // 编号是字符串，不能直接当数组下标；映射把 U0/B001 换成具体整数位置。
    for (int i = 0; i < users_.size(); ++i)
        userRows_.insert(users_[i].id, i);
    for (int i = 0; i < books_.all().size(); ++i)
        bookColumns_.insert(books_.all()[i].id, i);
    // 第 2 步：创建 U 行 B 列的稠密矩阵，全部初始化为 0（未评分）。
    // 原始小样例的行列顺序由 CSV 决定，建好后为：
    //          B001 B002 B003 B004
    //     U0     5    1    0    0
    //     U1     5    1    5    1
    //     U2     1    5    1    5
    // 初始化时间和矩阵空间均为 O(U*B)，随后填入 R 条评分的时间为 O(R)。
    matrix_.resize(users_.size());
    for (auto &row : matrix_)
        row.fill(0, books_.all().size());
    // 第 3 步：把评分记录中的业务编号换成下标，填入 matrix_[用户行][图书列]。
    for (const Rating &rating : ratings) {
        const int user = userRows_.value(rating.userId, -1);
        const int book = bookColumns_.value(rating.bookId, -1);
        // value(...,-1) 标记不存在的编号；检查下标后才填入评分，避免越界。
        if (user >= 0 && book >= 0)
            matrix_[user][book] = rating.score;
    }
}

double RecommendationService::similarity(const QString &userA,
                                         const QString &userB) const
{
    // 仅在两名用户都评过的图书上计算余弦相似度，0（未评分）不参与计算。
    // sim(a,b) = sum(r_a*r_b) / sqrt(sum(r_a^2)*sum(r_b^2))，单次扫描 O(B)。
    // 对合法的正评分，共同评分存在时相似度在 (0,1]；缺少共同评分返回 0。
    const int a = userRows_.value(userA, -1);
    const int b = userRows_.value(userB, -1);
    if (a < 0 || b < 0)
        return 0.0;
    // dot 是点积，squareA/squareB 是两个用户在共同评分图书上的评分平方和。
    double dot = 0.0;
    double squareA = 0.0;
    double squareB = 0.0;
    for (int book = 0; book < books_.all().size(); ++book) {
        const int scoreA = matrix_[a][book];
        const int scoreB = matrix_[b][book];
        // 只取交集：两人至少一人未评分就跳过，不把缺失评分当真实 0 分计算。
        if (scoreA > 0 && scoreB > 0) {
            dot += scoreA * scoreB;
            squareA += scoreA * scoreA;
            squareB += scoreB * scoreB;
        }
    }
    // 没有共同评分时分母为 0，返回 0；不把“未知评分”当成真实的零分。
    // 仓库小样例核对值：sim(U0,U1)=1，sim(U0,U2)=5/13。
    // U0/U1 共同评分都是 [5,1]：点积 26，分母 sqrt(26*26)=26，相似度为 1。
    // U0/U2 为 [5,1]、[1,5]：点积 10，分母仍为 26，相似度为 10/26=5/13。
    return squareA > 0 && squareB > 0 ? dot / std::sqrt(squareA * squareB) : 0.0;
}

QVector<Recommendation> RecommendationService::recommend(const QString &userId,
                                                          int topK) const
{
    // U 为用户数、B 为图书数、M 为候选数：相似度与预测合计 O(U*B)，
    // 堆维护 O(M*log K)，最后排序至多 K 项为 O(K*log K)。
    const int target = userRows_.value(userId, -1);
    // 不存在的用户或非正 K 没有合法推荐请求，直接返回空数组。
    if (target < 0 || topK <= 0)
        return {};

    // 余弦相似度计算：第 1 步：预先计算目标用户与所有其他用户的相似度，后续逐书预测复用。
    // 本实现使用全部具有正相似度的其他用户作为有效邻居，不只取固定人数。
    QVector<double> similarities(users_.size());
    for (int user = 0; user < users_.size(); ++user) {
        if (user != target)
            similarities[user] = similarity(userId, users_[user].id);
    }

    TopKHeap heap(topK);
    for (int col = 0; col < books_.all().size(); ++col) {
        const Book &book = books_.all()[col];
        // 第 2 步：只考虑目标用户未评过、当前可借库存大于 0 的图书。
        // 评分用于计算相似度；借阅历史不参与相似度，但借还库存影响候选资格。
        if (matrix_[target][col] > 0 || book.stock <= 0)
            continue;
        // 第 3 步：累加有效邻居的“相似度*评分”和相似度之和。
        // 同时统计所有评分人数及总分，用于无法预测时的热门回退。
        // weighted=加权评分总和，weight=有效权重总和，total=普通评分总和，count=评分人数。
        double weighted = 0.0;
        double weight = 0.0;
        double total = 0.0;
        int count = 0;
        for (int user = 0; user < users_.size(); ++user) {
            const int score = matrix_[user][col];
            // 未评分用户没有这本候选书的可用分数，既不计人数，也不贡献预测。
            if (score <= 0)
                continue;
            // 普通统计面向全部已评分用户；加权统计只使用正相似度的其他用户。
            ++count;
            total += score;
            if (user != target && similarities[user] > 0.0) {
                weighted += similarities[user] * score;
                weight += similarities[user];
            }
        }
        // 预测分 = weighted/weight；分母为 0 则回退到平均评分，完全无人评分则为 0。
        // 热门项的排名先看评分人数再看平均分；所有相似度为 0 的新用户也走此分支。
        // 按候选书分别判断来源：同一次推荐可同时包含协同预测项和热门回退项。
        const RecommendationSource source = weight > 0.0
                                                ? RecommendationSource::Collaborative
                                                : RecommendationSource::Popular;
        // 第 4 步：每算出一项即交给容量 K 的堆，无需保存并排序全部候选。
        // 原始小样例 U0 的 Top 2 应为 B003=35/9、B004=19/9（评分修改后会变化）。
        // B003：(1*5 + (5/13)*1)/(1+5/13) = 35/9，界面显示 3.8889。
        // B004：(1*1 + (5/13)*5)/(1+5/13) = 19/9，界面显示 2.1111。
        heap.add({book.id, book.title, weight > 0.0 ? weighted / weight
                                                      : (count > 0 ? total / count : 0.0),
                  count, source});
    }
    // 最后只对堆中少量结果排序；实际输出数量为 min(K,候选数)。
    return heap.sorted();
}
