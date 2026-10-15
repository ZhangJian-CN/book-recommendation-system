<div align="center">


# 📚 Book Recommendation System

### 基于哈希索引与用户协同过滤的图书检索及个性化推荐系统

**让图书检索更高效，让阅读发现更精准。**

<p>
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white" alt="C++17" />
  <img src="https://img.shields.io/badge/Qt-6.5%2B-41CD52?style=flat-square&logo=qt&logoColor=white" alt="Qt 6.5+" />
  <img src="https://img.shields.io/badge/CMake-3.19%2B-064F8C?style=flat-square&logo=cmake&logoColor=white" alt="CMake 3.19+" />
  <img src="https://img.shields.io/badge/Platform-Desktop-555555?style=flat-square" alt="Desktop" />
  <img src="https://img.shields.io/badge/Storage-CSV-7B61FF?style=flat-square" alt="CSV" />
</p>


以经典数据结构为基础，将桌面交互、图书数据管理与个性化推荐算法设计融入同一课程设计项目。

[项目介绍](#项目介绍) · [系统架构](#系统架构) · [核心技术](#核心技术) · [快速开始](#快速开始) · [数据组织](#数据组织) · [测试验证](#测试验证)

</div>

---

## 项目介绍

**Book Recommendation System** 是一个基于 **C++17 + Qt Widgets** 的本地图书信息管理与个性化推荐课程设计项目。

项目以校园图书应用场景为背景，围绕图书检索、数据维护、用户评分及推荐计算等问题，探索经典数据结构与推荐算法在实际桌面软件中的组织方式。系统采用 Qt 构建可视化界面，使用 CSV 维护本地数据，无需数据库服务器或在线服务即可运行基础图书管理功能。

设计重点包括：

- **高效数据访问**：研究基于哈希索引的精确查找与字符串匹配检索。
- **个性化信息发现**：以用户评分为依据，研究相似度计算与协同过滤推荐。
- **清晰的模块边界**：分离交互界面、数据操作、校验逻辑与文件持久化。
- **轻量化桌面体验**：原生 Qt Widgets 界面、本地数据存储、CMake 构建与自动化测试。

> **版本说明**：本分支包含验收演示版本，已接入图书增删改查、哈希编号索引、书名与作者 KMP 检索、评分与协同过滤推荐、借还与库存联动，以及四份 CSV 的校验和持久化。界面提供图书、推荐、借还三个页面，包含再次点击条目取消选择及表单复位、选中行白缝修正；排练步骤见 [演示说明](docs/demo_rehearsal.md)。

## 产品能力

| 能力             | 说明                                             |
| :--------------- | :----------------------------------------------- |
| 📖 图书信息展示   | 在桌面界面中查看图书编号、书名、作者、分类与库存 |
| 🔎 图书检索       | 使用链地址哈希索引查编号，使用 KMP 匹配书名与作者 |
| ✏️ 图书信息维护   | 新增、编辑、删除图书；被评分或借阅记录引用的图书禁止删除 |
| ⭐ 评分与推荐     | 保存或更新 1～5 分评分，以共同评分余弦相似度预测未评分图书，使用 Top K 小顶堆筛选并支持热门回退 |
| 🔄 借阅与归还     | 检查库存与重复借还，联动更新库存和借阅记录，支持写入中断后的恢复 |
| 💾 数据本地持久化 | 加载和校验四份 UTF-8 CSV，将图书、评分和借还修改写回文件 |
| 🛡️ 数据有效性校验 | 检查编号冲突、无效引用、非法库存、评分范围及异常 CSV 内容 |
| 🧪 自动质量验证   | 通过 8 项 CTest 检查数据层、算法、界面交互、保存重启及选中行背景 |

## 系统架构

项目采用轻量化桌面应用架构，以 **Qt Widgets** 承载人机交互，以数据管理模块承载读写和校验逻辑，以 **CSV** 实现文件持久化。

```mermaid
flowchart TB
    A["Qt Widgets · 图书 / 推荐 / 借还"] --> B["LibraryStore · 业务校验与持久化"]
    B --> C["BookRepository · 图书与哈希 / KMP 检索"]
    A --> R["RecommendationService · 余弦相似度 / Top K / 热门回退"]
    B <--> D[("books / users / ratings / borrow_records · 四份 CSV")]
    E["main.cpp · 程序启动"] --> A
    F["seed.qrc · 内置示例资源"] -.->|"初始化数据"| D
```

**架构特点**

- **界面与数据逻辑分离**：降低 UI 交互与数据读写之间的耦合。
- **本地优先**：不依赖网络连接或远程数据服务。
- **数据可迁移**：CSV 格式易于查看、备份与交换。
- **易于调试**：CMake 管理构建，CTest 支持基础回归验证。

## 核心技术

本课题的算法设计以 **“检索效率 + 个性化推荐”** 为主线，将经典数据结构与实际业务问题对应起来。

| 技术方向   | 数据结构 / 算法                    | 设计思路                                         |
| :--------- | :--------------------------------- | :----------------------------------------------- |
| 精确检索   | 链地址哈希表                       | 对图书编号建立索引，使用桶数组与链表处理哈希冲突 |
| 关键字检索 | KMP 字符串匹配                     | 针对书名与作者文本进行模式匹配                   |
| 用户相似度 | 余弦相似度                         | 从共同评分记录中度量用户兴趣的相近程度           |
| 个性化推荐 | User-Based Collaborative Filtering | 根据相似用户的评分信息形成候选图书预测分数       |
| 排名筛选   | Top K 小顶堆                       | 使用有限容量的堆保留评分较高的候选结果           |
| 冷启动处理 | 热门回退                           | 对无协同预测的候选按评分人数、平均分排序，展示分数为平均分 |

**协同过滤的核心思想**：兴趣相近的用户可能偏好相似的图书。对用户间的共同评分进行比较后，可通过邻近用户的反馈估计目标用户对未评分图书的兴趣。

以上算法已接入界面与自动测试。四本书小样例中，U0 的初始 Top 2 应为 B003（3.8889）、B004（2.1111）；具体复现步骤见 [演示说明](docs/demo_rehearsal.md)。

## 技术栈

| 类别       | 技术选型                         |
| :--------- | :------------------------------- |
| 开发语言   | C++17                            |
| GUI 框架   | Qt 6.5+ / Qt Widgets             |
| 项目构建   | CMake 3.19+                      |
| IDE        | Qt Creator                       |
| 编译器     | 与 Qt Kit 匹配的 MinGW / MSVC 等 |
| 数据持久化 | UTF-8 CSV                        |
| 资源管理   | Qt Resource System（`.qrc`）     |
| 自动化测试 | CTest + Smoke Test               |

## 快速开始

### 环境要求

建议安装 **Qt Creator**、**Qt 6.5 或更高版本**、**CMake 3.19 或更高版本**及对应的桌面编译器。Windows 环境下可使用配套的 MinGW 64-bit Kit。

### 获取源码

```bash
git clone https://github.com/ZhangJian-CN/book-recommendation-system.git
cd book-recommendation-system
```

### 使用 Qt Creator（推荐）

1. 在 Qt Creator 中选择 **文件 → 打开文件或项目**。
2. 打开仓库根目录下的 `CMakeLists.txt`。
3. 选择已安装的 **Qt 6 Desktop Kit**。
4. 完成配置后点击 **构建**，选择 `BookRecommendation` 运行目标，再点击 **运行**。名称以 `_smoke` 结尾的目标用于测试，`search_benchmark` 用于性能测量。

### 使用 CMake 命令行

确保 CMake 能找到对应版本的 Qt：

```bash
cmake -S . -B build
cmake --build build --config Debug
```

可执行文件的具体位置取决于编译器与构建配置。Qt 未被自动识别时，可通过 Qt Creator 选择 Kit，或配置 `CMAKE_PREFIX_PATH`。

### 指定数据目录

程序支持通过启动参数指定 CSV 所在目录：

```bash
BookRecommendation --data-dir "/path/to/data"
```

将 `BookRecommendation` 替换为实际生成的可执行文件路径（Windows 下通常带 `.exe`）。

> 未指定参数时，程序使用可执行文件旁的 `data` 子目录；指定目录不存在或完全为空时，首次启动会复制四份内置小样例。界面右下角“数据目录…”可查看实际路径。公开数据演示应先将 `demo_data/goodbooks` 的四份业务 CSV 复制到独立可写目录，再通过 `--data-dir` 加载该副本。

## 项目结构

```text
book-recommendation-system/
├── CMakeLists.txt                 # 构建及测试配置
├── main.cpp                       # 程序入口与数据初始化
├── mainwindow.h
├── mainwindow.cpp                 # 三页业务交互
├── mainwindow_ui.cpp              # 页面布局、样式与共用控件
├── mainwindow.ui                  # Qt Designer 界面
├── bookrepository.h
├── bookrepository.cpp             # 图书模型、校验和 CSV 访问
├── bookindex.cpp                  # 链地址哈希索引
├── booksearch.cpp                 # KMP 检索
├── recommendation.cpp             # 协同过滤、热门回退与 Top K
├── librarystore.cpp               # 业务校验、评分、借还与写入恢复
├── csvutil.cpp                    # CSV 解析与校验
├── bookrepository_smoke.cpp       # 数据层基础测试
├── mainwindow_smoke.cpp           # 界面交互基础测试
├── BookRecommendation_zh_CN.ts    # 中文翻译资源
├── seed.qrc                       # 示例数据资源
├── ui.qrc                         # 样式与图标资源
├── demo_data/                     # Goodbooks 公开数据子集及合成样例
├── docs/                          # 演示步骤、测试记录与界面截图
├── data/
│   ├── books.csv
│   ├── users.csv
│   ├── ratings.csv
│   └── borrow_records.csv
└── README.md
```

## 数据组织

系统使用 UTF-8 编码的 CSV 文件保存结构化数据，便于本地查看和调试。

| 数据文件             | 主要字段                                                     | 数据含义               |
| :------------------- | :----------------------------------------------------------- | :--------------------- |
| `books.csv`          | `book_id, title, author, category, stock`                    | 图书基础信息与可用库存 |
| `users.csv`          | `user_id, name`                                              | 用户信息               |
| `ratings.csv`        | `user_id, book_id, score`                                    | 用户—图书评分关系      |
| `borrow_records.csv` | `record_id, user_id, book_id, borrow_date, return_date, status` | 借阅记录               |

四份 CSV 均已接入程序。`data` 保留四本书小样例，用于手算与边界核对；`demo_data/goodbooks` 包含 100 本图书、40 名用户、800 条评分和 10 条借阅记录，推荐用户为 U00。书名、作者及评分来自 Goodbooks-10k，显示名、分类、库存及借阅记录为模拟或占位数据，来源与许可见 [SOURCE.md](demo_data/goodbooks/SOURCE.md)。`demo_data/large` 为完全合成的备用测试数据。交互演示使用独立可写副本，保留仓库原始样例。

## 测试验证

项目通过 CTest 组织 8 项测试：

- **`bookrepository_smoke`**：验证 CSV 加载、图书修改、文件保存与重新读取。
- **`mainwindow_smoke`**：验证窗口查询、信息编辑及持久化后的行为。
- **`hash_smoke`**：验证哈希冲突、索引更新与 KMP 检索。
- **`core_smoke`**：验证 CSV 与引用校验、评分、推荐分数和 Top K。
- **`borrow_smoke`**：验证借还约束、库存联动、持久化与写入恢复。
- **`large_demo_smoke`**：验证两套较大演示数据的加载及推荐。
- **`ui_polish_smoke`**：验证布局、数字控件、界面业务与保存重启。
- **`selection_toggle_smoke`**：验证选中与再次点击复位、CSV 不变及选中背景连续性。

`search_benchmark` 是单独运行的检索性能测量程序，不计入上述 8 项 CTest。

在完成构建后执行：

```bash
ctest --test-dir build -C Debug --output-on-failure
```

测试命令用于运行仓库中已配置的测试用例，不等同于对所有课程设计算法的完整验收。

---

<div align="center">


**Book Recommendation System**

*From Data Structures to Intelligent Book Discovery.*

<sub>Built with C++17 · Qt Widgets · CMake</sub>

</div>
