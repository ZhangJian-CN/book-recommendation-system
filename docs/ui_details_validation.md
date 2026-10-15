# UI 细节修订交付与验证（2026-10-15）

执行依据：`D:/PDF/课程设计（数据结构与算法）/output/UI细节修订清单-20261015.md` 及同目录三页修订 HTML 预览。下列图片由实际 Windows Qt 窗口抓取，HTML 仅用于参照。

## 分支与范围

- 起点为干净的 `feature/ui-refine@39d22a4609756bf5cc442f34650556aba949c36c`，新建本地 `feature/ui-details`。
- 修改集中在 `mainwindow_ui.cpp`、`mainwindow.cpp`、`mainwindow.h`、`styles/app.qss` 和 UI 回归测试；业务算法、CSV 文件、已有中文讲解注释完整保留。
- 原有控件 objectName、三页次序、表格列序、业务连接及数值范围与默认值保留。主分支未修改，本次提交不推送远端。

## 完成项

| 项目 | 实现与核对 |
| --- | --- |
| P0-1 表格焦点 | 清除常态、聚焦态 QSS 外框及原生 frame；选中行首列绘制 2 逻辑像素细线，离开焦点为 `#91B19E`，进入焦点为 `#245C4D`。空表在表头下显示 48×2 的短线。Tab 进入/离开，方向键继续选行，未关闭焦点。 |
| P0-2 自然高度 | 非空列表按实际行数与页面容量计算 sizeHint：图书 5～9、推荐 3～8、借阅 4～9；计入表头最小/期望高度、实际行高和必要横向滚动条。结果卡片顶部对齐，可用空间不足时由布局压缩，数据在表内滚动。行数、横向滚动范围和窗口尺寸变化均触发布局更新。 |
| 空数据处理 | 沿用原有空结果提示；空表仅保留一行视口显示键盘焦点，避免继续预留 3～5 行空白。模型 rowCount 仍为 0，没有补空记录。 |
| P0-3 字体 | 顶部统计、预测分、图书/借阅编号、日期和库存使用 Segoe UI；中文继续用 Microsoft YaHei UI。预测分四位小数，日期与编号原值保持。作者色调整为 `#5D6B62`。 |
| P0-4 归还区 | 移除表单底部 stretch，将归还按钮、规则说明放回记录说明之后，说明与按钮间隔实测 12 逻辑像素。操作卡自然高度，小窗口时整个表单可滚动。 |
| P0-5 页内状态 | 切页显示该页实际摘要，推荐摘要保存最近成功的用户与结果数；未提交输入或无效推荐不覆盖该状态。操作提示显示 5 秒后恢复当前页摘要。借阅编号变为无效值时，立即恢复记录数量摘要。切页不重新计算推荐、不清空输入和筛选。 |
| P1 按钮与对齐 | 保存修改、新增图书、保存评分改为纯文字按钮；保留 36 像素高度。书名/作者表头与正文起点约 12 像素（正文 padding 9 加原生 delegate 约 3），来源表头与胶囊起点同为 12；分类、状态胶囊保持 24 像素高并居中。 |
| P1 推荐说明 | 明确小标题与两段正文；说明区距评分卡 16 像素，内部左缩进 18，与输入框对齐；12 像素正文采用 120% 行高，在本机约 18～20 逻辑像素。 |

数字增减按钮仍为 28×28，横向间隔 2，Top K 宽 136，QSS 右 padding 42。点击、键盘、直接输入、边界禁用、聚焦几何和长按重复继续通过原断言。

## 构建与自动验证

环境：Windows、Qt 6.11.2 MinGW 64-bit、MinGW 13.1、Debug。使用独立目录 `build/UiDetails_Debug`，测试全部操作独立的可写临时 CSV 副本。

```powershell
$env:PATH='D:\Qt\6.11.2\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build/UiDetails_Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/UiDetails_Debug -j 4
& 'D:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build/UiDetails_Debug --output-on-failure --timeout 30
```

修改前基线 **7/7 通过**；修改后应用、测试构建成功，当前全部 **7/7 CTest 通过**：

`bookrepository_smoke`、`mainwindow_smoke`、`ui_polish_smoke`、`hash_smoke`、`core_smoke`、`borrow_smoke`、`large_demo_smoke`。

保留全部原功能断言，扩展 `ui_polish_smoke` 验证：

- 小样例三页按容量收缩、实际模型行数不变；Goodbooks 100 条记录、Top K=1000 时内部滚动。
- Goodbooks 筛选为 1 条后卡片变矮，显示全部后恢复；切页保留筛选和输入。
- 选中标记的实际像素随焦点变化；Tab 可离开，方向键可移动当前行；空表仍有可见焦点短线，frameWidth 始终为 0。
- 三个按钮无图标，数字字段声明 Segoe UI，原 tooltip 和无 cellWidget 断言继续通过。
- 借阅摘要与归还按钮间隔 10～14 像素；无效借阅编号不保留上一个选中摘要。
- 未提交推荐用户和 Top K 不改变结果；无效推荐失败后，真实等待 5.3 秒，状态恢复最近成功用户与条数，输入仍保留无效值。
- 原小样例 `B003=3.8889`、`B004=2.1111`，增删改查、失败保留输入、引用删除拒绝、评分、借还库存与重启断言继续通过。
- Goodbooks 初始 100/40/800/10，U00 Top 5 为 B051/B059/B068/B085/B090；借还 R011、评分后退出候选及保存重启继续通过。

额外核对：所有原有中文 `//` 讲解注释无缺失；原始样例、演示 CSV 与业务源码相对起点无 diff；原 Qt Creator 构建目录的四份 CSV 与工程样例 SHA-256 一致。

## 屏幕与缩放

| 环境 | 核对结果 |
| --- | --- |
| 原生 Windows，实际 DPR 1.5，可用区域 1707×1067 | GUI 回归通过；默认窗口和 1000×680 三页截图通过检查 |
| 有效 100%，`QT_SCALE_FACTOR=0.6666666666666666`，实际 DPR 1，可用区域 2560×1600 | GUI 回归通过；字段滚动、标签、长书名及数字最大值检查通过 |
| 有效 125%，`QT_SCREEN_SCALE_FACTORS=1.25`，实际 DPR 1.25，可用区域 2048×1280 | GUI 回归通过；字段滚动、标签、长书名及数字最大值检查通过 |
| CTest offscreen，可用屏幕 1280×720 | GUI 回归通过；默认窗口按屏幕收敛，另验证 1000×680 布局 |

100%/125% 通过独立进程的 Qt 缩放覆盖验证，未切换 Windows 系统显示设置。原生截图覆盖真实系统字体；offscreen 环境用于布局和交互回归，不能代替原生字体观察。Qt 截图不包含操作系统窗口装饰。

## 实机截图入口

### 小样例：普通、选中、键盘焦点

| 页面 | 初始/普通 | 选中、焦点在表外 | 键盘焦点在表内 |
| --- | --- | --- | --- |
| 图书 | [普通](ui-details/neutral-books.png) | [选中](ui-details/sample-books.png) | [焦点细线](ui-details/sample-books-table-focus.png) |
| 推荐 | [普通](ui-details/neutral-recommend.png) | [选中](ui-details/sample-recommend.png) | [焦点细线](ui-details/sample-recommend-table-focus.png) |
| 借还 | [普通](ui-details/neutral-borrow.png) | [选中](ui-details/sample-borrow.png) | [焦点细线](ui-details/sample-borrow-table-focus.png) |

### Goodbooks 与小窗口

- Goodbooks：[图书](ui-details/goodbooks-books.png)、[推荐](ui-details/goodbooks-recommend.png)、[借还](ui-details/goodbooks-borrow.png)。
- 1000×680：[图书](ui-details/goodbooks-compact-books-table-focus.png)、[推荐](ui-details/goodbooks-compact-recommend-table-focus.png)、[借还](ui-details/goodbooks-compact-borrow-table-focus.png)。
- 有效 100%：[图书](ui-details/dpi-100/sample-compact-books-table-focus.png)、[推荐](ui-details/dpi-100/sample-compact-recommend-table-focus.png)、[借还](ui-details/dpi-100/sample-compact-borrow-table-focus.png)。
- 有效 125%：[图书](ui-details/dpi-125/sample-compact-books-table-focus.png)、[推荐](ui-details/dpi-125/sample-compact-recommend-table-focus.png)、[借还](ui-details/dpi-125/sample-compact-borrow-table-focus.png)。
- 数字交互及最大值：[100%](ui-details/spin/states-100.png)、[125%](ui-details/spin/states-125.png)、[150%](ui-details/spin/states-150.png)。

### 异常、空状态与补全

[图书空结果及键盘焦点](ui-details/books-empty.png)、[推荐空结果](ui-details/recommend-empty.png)、[借阅空记录](ui-details/borrow-empty.png)、[新增错误](ui-details/sample-error.png)、[删除确认](ui-details/sample-delete-confirm.png)、[编号补全](ui-details/id-completion.png)、[长目录](ui-details/neutral-and-long-directory.png)。

## 查看新版

已构建的程序为 `build/UiDetails_Debug/BookRecommendation.exe`，可以通过 `--data-dir <可写副本目录>` 指定数据，也可以在 Qt Creator 重新构建当前分支后运行。已有旧构建进程继续显示它启动时的旧界面；本轮未关闭该进程。核心修订及 P1 均已完成，无遗留实现项。
