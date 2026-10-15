# UI 美化验证记录（2026-10-08）

执行依据：`D:/PDF/课程设计（数据结构与算法）/output/UI美化执行方案-20261008.md`。
基线为 `feature/full-project@497dedb`，执行前工作区干净。本次在 `feature/ui-polish` 完成 A 批及 B 批的统计、数据目录、空状态、编号补全和按钮状态。

## 改动

| 文件 | 内容 |
| --- | --- |
| `mainwindow_ui.cpp` | 三页两栏布局、318 px 操作区、表单滚动、顶部统计、有效默认用户、编号补全、数据目录对话框 |
| `mainwindow.cpp` / `.h` | 表格文本与 tooltip、选中对象、动态统计、空状态、成功与失败反馈 |
| `styles/app.qss` / `ui.qrc` | 浅色青绿色主题；应用及两个界面测试目标加载同一资源 |
| `ui_polish_smoke.cpp` / `ui_screen_1280x720.json` | 新界面状态、布局、两套数据的按钮排练与截图 |
| `CMakeLists.txt` | 接入界面源文件、样式资源和第 7 个测试 |

原三页次序、既有 objectName、表格列顺序和 Top K 默认值保持可测试。推荐分数仍保留四位小数；借阅空归还日期仅显示为“—”，CSV 仍保持空字段。业务层、算法、四份 CSV 格式以及仓库中的 `data`、`demo_data` 未修改。

## 构建与回归

环境：Windows，Qt 6.11.2 MinGW 64-bit，MinGW 13.1，现有 Debug 构建目录。

```powershell
$env:PATH='D:\Qt\6.11.2\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug -j 4
& 'D:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug --output-on-failure --timeout 30
```

结果：构建成功，**7/7 CTest 通过**。原 6 个测试及断言保留。新增 `ui_polish_smoke` 使用临时数据目录，测试结束自动清理。

| 核对范围 | 结果 |
| --- | --- |
| 启动统计与默认用户 | 小样例 4/3/10/2、U0；Goodbooks 100/40/800/10、U00 |
| 禁用与选中 | 未选择时保存/删除禁用，新增可用；选中后启用；删除成功后恢复禁用 |
| 筛选与空列表 | 作者“布莱恩特”命中 B004；筛掉 B001 时仍注明编辑 B001；无匹配提示可恢复 |
| 失败保留输入 | 空书名保存、重复编号新增失败后输入保留；引用删除仍被拒绝；状态栏显示失败 |
| 统计与补全更新 | 新增/删除后图书数与补全列表同步；评分与借阅成功后计数同步 |
| 借阅显示与重复操作 | 空日期“—”；当前记录说明与实际状态一致；重复借/还被拒绝 |
| 空推荐与无效用户 | 评完 U0 的候选后显示空推荐提示；无效用户报错且不替换输入或结果摘要 |
| 空借阅 | 仅有表头的临时借阅 CSV 可加载，显示 0 和空记录提示 |
| 路径与聚焦 | 完整路径只读、可选中复制；编号框聚焦边框可见；主按钮另有深色聚焦边框 |
| 格式 | 原列序、整行单选、禁止表内编辑；所有单元格有完整 tooltip；推荐四位小数 |

### 原排练单的核心链路

按 [验收演示排练单](demo_rehearsal.md) 的步骤进行自动按钮排练：

- 小样例：原 `mainwindow_smoke` 验证 B001 库存 3→4 保存与重启、B005 新增/关键词检索/删除、R003 借还及 B004 库存 2→1→2、U0/B003=4 保存与重启。新增测试补验重复编号、B001 引用删除拒绝、重复借还、计数和输入保留。
- 推荐基准：U0 初始 Top 2 仍为 **B003=3.8889（35/9）、B004=2.1111（19/9）**。
- Goodbooks：独立可写副本；B001 书名与完整 tooltip、B100《The Poisonwood Bible》正确；U00 初始 Top 5 仍为 B051、B059、B068、B085、B090；保存 U00/B051=4 后评分数变为 801，B051 移出候选；关闭窗口并从同一副本重新构造窗口后仍成立。

这是对真实 Qt 控件的自动排练与窗口重建测试，不代表两名成员已完成现场讲解。公开数据的库存、分类、用户名称和借阅记录仍为模拟/占位字段，来源口径见 [SOURCE.md](../demo_data/goodbooks/SOURCE.md)。

## 尺寸与截图

| 条件 | 验证方法及结果 |
| --- | --- |
| 1280×720 屏幕 | CTest 的 Qt offscreen 配置模拟 1280×720、DPR=1；启动客户区受屏幕约束为约 1200×656；检查初始布局及每个输入/按钮可达 |
| 1000×680 窗口 | 原生 Windows Qt 窗口检查三页；表单必要时滚动，保存/新增/删除、保存评分、归还按钮固定在滚动区外 |
| 150% | 本机原生 Windows QPA：可用区域 1707×1067 逻辑像素，实际 DPR=1.5；正常和小窗口两套布局通过 |
| 125% | 原生 Windows QPA 加 `QT_SCREEN_SCALE_FACTORS=1.25` 覆盖：可用区域 2048×1280，实际 DPR=1.25；两套布局通过 |

正常三页截图来自本机原生 Qt 窗口抓取，不是 HTML 预览。PNG 含实际设备像素，因此 1200×780 逻辑窗口在 DPR=1.5 时为 1800×1170 像素。

| 页面 | 小样例 | Goodbooks |
| --- | --- | --- |
| 图书 | [图书管理](ui-polish/sample-books.png) | [公开书目](ui-polish/goodbooks-books.png) |
| 推荐 | [手算基准](ui-polish/sample-recommend.png) | [U00 Top 5](ui-polish/goodbooks-recommend.png) |
| 借还 | [小样例记录](ui-polish/sample-borrow.png) | [10 条模拟记录](ui-polish/goodbooks-borrow.png) |

小窗口：[图书](ui-polish/sample-compact-books.png)、[推荐](ui-polish/sample-compact-recommend.png)、[借还](ui-polish/sample-compact-borrow.png)。
125% 小窗口：[图书](ui-polish/sample-compact-125-books.png)、[推荐](ui-polish/sample-compact-125-recommend.png)、[借还](ui-polish/sample-compact-125-borrow.png)。
其他状态：[聚焦](ui-polish/sample-focus.png)、[新增错误](ui-polish/sample-error.png)、[删除确认](ui-polish/sample-delete-confirm.png)。

截图复现（在工程根目录、已设置 Qt PATH 时）：

```powershell
$env:UI_SCREENSHOT_DIR='D:/Code/QtProjects/BookRecommendation/docs/ui-polish'
& .\build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug\ui_polish_smoke.exe
# 125% 覆盖检查；不修改 Windows 的系统缩放设置
$env:QT_SCREEN_SCALE_FACTORS='1.25'
$env:UI_SCREENSHOT_TAG='-125'
& .\build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug\ui_polish_smoke.exe
```

### 检查中修正的问题与限制

- 表格单元格的边框样式会遮住 `BackgroundRole` 提供的状态浅底色，已去掉该样式；原生截图核对未归还棕橙色、已归还青绿色及状态文字。
- 125% 截图发现系统样式仍会绘制单个单元格的焦点框；使用标准绘制委托清除单元格焦点/悬停标记，保留整行选中和整张表格的聚焦边框，没有自行绘制表格。
- 表单滚动时固定主要按钮，避免小窗口将保存、删除、评分或归还操作裁在内容区外。编号、书名等字段可滚动访问。
- 125% 通过 Qt 环境变量模拟；1280×720 通过 offscreen 模拟。没有切换系统显示设置，也没有在独立 1280×720 物理显示器上排练。offscreen 缺少中文字体时只用于行为/几何验证，视觉核对使用原生窗口。
- 现场演示仍需沿用干净数据副本，并由两名成员实际操作和讲解。本次未进行现场录屏，也未合并到 `main`。
