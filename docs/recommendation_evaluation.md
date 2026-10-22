# 推荐效果与现有规模耗时记录

日期：2026-10-22（Asia/Shanghai）。代码入口：`recommendation_evaluation.cpp`。评估复用生产 `RecommendationService`；本轮没有修改 `recommendation.h/.cpp`，没有调参。

## 固定划分与候选规则

数据为当前 Goodbooks 教学子集，100 本图书、40 名用户、800 条评分。图书/用户/评分/借阅文件 SHA-256、编译器与参数见 [manifest.json](evaluation-20261022/manifest.json)。该子集事先选择了高活跃用户及共同评分图书，不能代表无偏线上用户流量；书籍、评分与模拟库存/借阅的来源区别见 [SOURCE.md](../demo_data/goodbooks/SOURCE.md)。

按用户 ID 排序，每名用户的评分先按图书 ID 排序，再使用共享 `std::mt19937(20261022)` 打乱，取最接近 20% 的条目作为测试，其余训练。本数据每人 20 条，实际每人训练 16、测试 4，合计 **640/160**。实际文件为 [train_ratings.csv](evaluation-20261022/train_ratings.csv) 和 [test_ratings.csv](evaluation-20261022/test_ratings.csv)，不依赖只记住种子重建划分。

全部用户的测试评分同时移出训练矩阵。相似度、热门评分人数、平均分及排序只使用训练数据。两种方法均排除目标用户的训练集已评图书，且库存必须大于零。热门方法先取得完整有评分榜单，按该用户候选条件过滤，再取 Top 5。测试评分不用于过滤候选。

相关项定义为测试集中评分 ≥4 且有可借库存的图书。Precision@5 分母固定为 5；Recall@5 分母为该用户的相关项数；二元 NDCG@5 使用 `1/log2(rank+1)` 折扣，并用前 min(5,相关项数) 个相关项归一化。没有相关项的用户保留 Precision，但从三项“有效用户均值”的共同队列中排除，Recall/NDCG 留空。本次有效用户 **40**、排除 **0**，所以全体 Precision 与有效用户 Precision 一致。

## 实测效果

| 方法 | Precision@5 | Recall@5 | NDCG@5 | 平均返回数 | 不足 5 项的用户 | 推荐到的不同图书 / 100 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 当前个人推荐 | 0.065000 | 0.093750 | 0.074465 | 5 | 0 | 18 / 100 |
| 热门基线 | 0.070000 | 0.125000 | 0.160997 | 5 | 0 | 13 / 100 |

逐用户排序、相关项数、命中、实际返回数和指标见 [per_user.csv](evaluation-20261022/per_user.csv)（80 个“用户×方法”结果）；汇总见 [summary.csv](evaluation-20261022/summary.csv)。覆盖率指各方法推荐到的不同图书数 / 当前可借图书数，本数据可借图书数为 100。

独立使用 Python 从划分、库存及排名重新计算了候选过滤、热门排序、80 行指标和宏平均，核对了训练/测试互斥且并集等于原始评分，结果一致，见 [独立复算记录](evaluation-20261022/independent-audit.txt)。

**本次决策：保留算法，交付测量证据。** 本次最终测试中热门基线的三项命中排序指标更高，个人推荐的图书覆盖更广。这不能证明个人算法普遍优于热门，也不能证明另一套数据上一定落后。只有一次小规模、经过筛选的留出测试，尚无独立验证集，不能继续盯着该测试集挑共同评分阈值或收缩参数。后续若做算法实验，先另设验证集，并保留最终测试及 U0 手算兼容检查。本轮不以“必须提升”为验收条件。

个人流程可能输出无人评分的回退候选，热门流程不会；保留真实产品行为，本次两种方法均返回 5 项。离线命中不等于真实满意度，本文不作线上效果或显著性结论。

## Release 分段耗时

一次本机测量：Windows 11 25H2，Intel Core Ultra 9 185H（16 核 / 22 逻辑处理器），Qt 6.11.2，MinGW 13.1.0，Release；窗口 1280×720，Qt offscreen、DPR=1。使用完整 100/40/800 的独立可写副本，不使用效果评估的 640 条训练集来冒充现有交互规模。

先加载 CSV 和创建窗口，再预热 3 次，固定 K=5，按清单中的用户序列记录 40 次请求。序列和构建信息见 manifest；全部采样见 [latency_requests.csv](evaluation-20261022/latency_requests.csv)，汇总见 [latency_summary.csv](evaluation-20261022/latency_summary.csv)。P95 采用升序样本的第 ceil(0.95×N) 项。

| 阶段 | 中位数 / ms | P95 / ms | 最大 / ms |
| --- | ---: | ---: | ---: |
| 建映射与评分矩阵 | 0.039000 | 0.076200 | 0.188200 |
| 推荐计算 | 0.019350 | 0.024400 | 0.031200 |
| UI 填表、事件处理与同步重绘 | 4.364600 | 5.035500 | 6.147500 |
| 请求到本次刷新完成 | 4.423200 | 5.100500 | 6.203400 |

前三项分段与整体请求在同一个生产窗口调用链计时，CSV 加载不进入请求时间。UI 段另包含状态栏和按钮分发开销；原始 CSV 同时列出纯 model_fill_ms。offscreen 包含 Qt 控件重绘，不包含真实显示器扫描输出或人的输入时间，不应写成真实显示器端到端响应保证。不同采样的中位数不要求相加精确等于整体中位数。

这次现有规模没有测得需要后台计算的卡顿，因此不引入缓存、后台线程或稀疏矩阵。服务构建与矩阵空间仍按 O(U×B + R)、O(U×B) 说明，Top K 堆额外空间 O(K) 不能代表整个服务空间。更大规模不在本次验证范围内。

## 重现命令

在项目根目录先配置 Release（工具链参数同 [一致性验证](data_consistency_validation.md)），再执行：

```powershell
$env:PATH = 'D:/Qt/6.11.2/mingw_64/bin;D:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' --build build/DataConsistency_Release --target recommendation_evaluation -j 4
Push-Location build/DataConsistency_Release
$env:QT_QPA_PLATFORM = 'offscreen:configfile=ui_screen_1280x720.json'
& './recommendation_evaluation.exe' '../../demo_data/goodbooks' '../../docs/evaluation-20261022'
Pop-Location
```

同一工具链的实际划分与效果 CSV 可逐字节重复；计时因系统负载改变。实际交付目录的新 Release 构建已经重新运行，训练、测试、逐用户结果和效果汇总四份文件与本次记录逐字节一致。复跑整体耗时中位数 4.894900 ms、P95 7.058900 ms、最大 7.482800 ms；这与前表是两次本机测量，不能挑选更小值作为普遍保证。交付目录的 `evaluation/` 保存复跑原始结果，`logs/evaluation-delivery.txt` 保存执行输出。评估和 `search_benchmark` 均不计入 10 项功能 CTest。
