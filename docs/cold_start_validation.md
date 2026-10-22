# 新用户冷启动与热门榜单验证记录

日期：2026-10-22（Asia/Shanghai）。对应执行方案：`新用户冷启动与热门榜单推荐-执行方案-20261022.md`。

## 实现与使用

- 评分与推荐页新增 `popularButton`“热门榜单”，无需用户编号，不新增用户、不改写编号输入。
- 已登记的零评分用户自动使用同一个 `popularTopK`，展示原因；首次评分后自动进入已有评分用户流程。有评分但无共同评分的用户仍可得到原有逐候选热门回退。
- 保留推荐表原四列位置，追加第五列“评分人数”；热门模式显示“平均分”和名次，个人模式显示“分数”，来源区分“协同过滤预测”“热门回退”“热门榜单”。个人推荐原本已有纵向行号，切换后继续保留。
- 全局榜仅纳入库存大于零且有正评分的图书，按评分人数降序、原始平均分降序、编号升序。显示均分四位小数，排序不使用格式化结果。
- 评分、借出、归还、图书信息及库存修改后，按最近成功展示的模式、用户、Top K 刷新；保留未提交输入，按图书编号恢复仍存在的选中项。
- 无候选时明确显示“暂无符合条件的热门图书，需有评分且有可借库存”。未知或空用户点击个人推荐仍报错，并保留此前结果。

演示使用 [cold_start 数据与复制启动命令](../demo_data/README.md)，操作顺序见 [演示排练单](demo_rehearsal.md#新用户冷启动与热门榜单2026-10-22)。原始 `data`、Goodbooks 和合成大数据均保持原值。

## 分支与既有修改保护

执行前分支：`study/cao-comments-20261016`；执行分支：`feature/cold-start-popular-20261022`。

基准 HEAD：`6bd24e94a8323603f841ea9cda757764419a0c04`。本轮未创建提交，交付内容位于当前工作区。

修改前已有 26 个修改文件。原始状态、完整补丁、修改文件副本和全部受 Git 跟踪文件的 SHA-256 清单保存在 `build/ColdStartPopular_Debug/baseline/`；该构建目录被 Git 忽略。

本轮修改已有文件：`recommendation.h/.cpp`、`mainwindow.h/.cpp`、`mainwindow_ui.cpp`、`ui_polish_smoke.cpp`、`CMakeLists.txt`、`demo_data/README.md`、`docs/demo_rehearsal.md`。新增 `cold_start_smoke.cpp`、`demo_data/cold_start/` 的四份 CSV、本记录及 `docs/cold-start/` 截图。其他原有修改保留。

已按修改前 SHA-256 清单核对：本轮仅改变上述 9 个已有文件；原有 26 个修改文件中另外 19 个逐字节保持原值，原始数据集保持原值。相对本轮开始时的文本差异见 `build/ColdStartPopular_Debug/baseline/task-changes.patch`，范围核对见同目录 `scope-check.json`。

并发会话曾在推荐模块加入同名接口，构建检出了重复定义；已合并为各一个声明、实现和零评分分流判断，之后重新构建测试。

## 构建与自动测试

环境：Windows、Qt 6.11.2、MinGW 13.1.0、CMake、Ninja，独立 Debug 目录 `build/ColdStartPopular_Debug`。

```powershell
$env:PATH = 'D:/Qt/6.11.2/mingw_64/bin;D:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' -S . -B build/ColdStartPopular_Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' --build build/ColdStartPopular_Debug -j 4
& 'D:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build/ColdStartPopular_Debug -R cold_start_smoke --output-on-failure --timeout 30
& 'D:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build/ColdStartPopular_Debug --output-on-failure --timeout 30
```

修改前：构建成功，8/8 CTest 通过，日志 `build/ColdStartPopular_Debug/baseline/ctest-before.txt`。修改后：构建成功，**9/9 CTest 全部通过，0 失败，总耗时 8.28 秒**，日志 `build/ColdStartPopular_Debug/ctest-final.txt`。`search_benchmark` 仍不计入功能测试总数。

最终注册项：`bookrepository_smoke`、`mainwindow_smoke`、`ui_polish_smoke`、`selection_toggle_smoke`、`hash_smoke`、`core_smoke`、`cold_start_smoke`、`borrow_smoke`、`large_demo_smoke`。`git diff --check` 通过。

| 验收项 | 测试与实际核对 |
| --- | --- |
| CS01 | `cold_start_smoke`：4/4/10/2、Top 4 编号顺序、人数、原始均分均精确核对 |
| CS02 | 核心及 UI：UCOLD 零评分与全局榜一致，全部 Popular；页面明确显示零评分原因 |
| CS03 | 核心及 UI：未知编号不获个人推荐；未知和空编号均可主动浏览全局榜，用户仍为 4 名；错误时保留结果及输入 |
| CS04–CS05 | 构造多人数低分、少人数高分、同人数不同均分、同人数同均分；反转图书/用户/评分顺序后，各 K 的排名保持确定 |
| CS06–CS07 | 零库存、无评分不入榜；空仓库、全无评分、全部无库存安全返回空；K≤0、K=1、K 大于候选数正确；UI 空结果文案与窄窗口可达性通过 |
| CS08 | 更新 U0/B001 改变均分，评分记录和人数不增加 |
| CS09–CS10 | UCOLD/B001=4 后共 11 条评分，个人榜为 B003、B004、B002；全局 B001 仍在，4 人、3.7500；无共同评分时保持原有热门回退 |
| CS11 | UI 全局模式保存 U1/B001=4 后均分 3.5000，恢复 5 后 3.7500；人数保持 4，模式及未提交用户/K 均保留 |
| CS12 | 核心及 UI：B003 库存借还 1→0→1，移榜后恢复；书名、直接修改库存同步；切页摘要、选中恢复、再次点击取消及方向键导航通过 |
| CS13 | 保存后关闭并销毁原窗口，在新的测试进程重新创建 QApplication/MainWindow、加载同一 CSV 副本；11 条评分保留，UCOLD 进入个人流程，全局 B001 为 4 人、3.7500 |
| CS14 | 原样例 U0 仍为 B003=35/9、B004=19/9；原有全部测试断言保留并参与回归 |

`cold_start_smoke` 以核心业务断言覆盖规则，`ui_polish_smoke` 增加真实控件按钮操作与进程重载；CS01—CS14 是验收点，并非 14 个 CTest 目标。

## 截图与显示检查

下列图片由实际 Qt Widgets 控件执行上述操作后 `QWidget::grab()` 生成，没有拼接或重绘。离屏平台 `offscreen:configfile=ui_screen_1280x720.json`，DPR=1。检查包括按钮、五列表格、提示、热门颜色、选中及聚焦显示。

| 截图 | 场景 |
| --- | --- |
| [global-1280x720.png](cold-start/global-1280x720.png) | 初始全局 Top 4，1280×720 |
| [global-1000x680.png](cold-start/global-1000x680.png) | 窄窗口的按钮与五列表格 |
| [cold-start.png](cold-start/cold-start.png) | UCOLD 零评分原因、热门来源及人数 |
| [first-rating-personal.png](cold-start/first-rating-personal.png) | 首次评分后个人推荐；未提交编号和 K 保留 |
| [global-after-first-rating.png](cold-start/global-after-first-rating.png) | 全局 B001 仍在榜，4 人、3.7500，表格选择状态 |
| [reloaded-global.png](cold-start/reloaded-global.png) | 新测试进程重新加载后的榜单 |
| [empty-1000x680.png](cold-start/empty-1000x680.png) | 无评分时的空结果和窄窗口 |

在构建目录单独重现截图：

```powershell
$env:QT_QPA_PLATFORM = 'offscreen:configfile=ui_screen_1280x720.json'
$env:COLD_SCREENSHOT_DIR = 'D:/Code/QtProjects/BookRecommendation/docs/cold-start'
& './ui_polish_smoke.exe'
```

该命令同样需要前述 Qt/MinGW PATH。CTest 的常规执行不启用截图输出。

## 原理与边界

`hasRatings` 检查用户矩阵行是否存在正评分；未知用户由 UI 先验证。已登记零评分用户直接调用与全局浏览相同的 `popularTopK`，无需计算相似度。有评分用户的协同计算和逐候选回退保持原值。

`popularTopK` 逐图书扫描矩阵，统计人数与评分总和，计算均分，交给原有 `TopKHeap` 和 `better` 比较器，最终排序输出至多 K 项。构造矩阵成本 O(U×B + R)，榜单计算成本 O(U×B + B×log K + K×log K)，堆新增空间 O(K)，原有稠密矩阵空间仍为 O(U×B)。

本轮通过文件加载 UCOLD，不提供界面用户注册。榜单的热度定义沿用评分人数优先；评分表示显式的 1～5 分，不统计借阅次数。界面结果及自动操作证据不代表教师已重新验收，也不替代成员现场讲解。新进程重启使用与主程序相同的 MainWindow 和业务实现，由 UI 测试入口驱动。
