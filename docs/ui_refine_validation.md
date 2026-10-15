# UI 视觉深化交付与验证（2026-10-15）

执行依据：`D:/PDF/课程设计（数据结构与算法）/output/UI视觉深化执行方案-20261015.md` 及同目录三页 HTML 预览。HTML 已读取并渲染参照；以下交付图片均为真实 Qt 窗口截图。

## 1. 原有修改的保护与分支

- 起点：`feature/ui-polish@1eb98ab`，现场有 16 个未提交源码文件，312 行新增、3 行删除，主要为中文讲解注释。
- 编码前逐文件复制，并校验 SHA-256；完整文件、原始 Git 补丁及校验清单保存在 `D:/PDF/课程设计（数据结构与算法）/output/UI视觉深化-原有修改备份-20261015-01/`。
- 新建 `feature/ui-refine`，先以 **`61b3490`** 单独保存上述已有改动。本轮视觉提交以该提交为基线，未混入新的算法修改。
- 完成后核对：16 份原有文件的中文讲解注释保留；其中 13 份非界面文件与备份逐字节相同。原始 CSV、演示目录及算法模块未修改。

## 2. 完成范围

| 批次 | 实现 |
| --- | --- |
| A | 暖白 / 墨绿配色；正文保留 10 pt；21 / 18 / 15 px 标题层级；紧凑品牌与统计；白底选中的分段导航；单层可见内容边界；表头与数据对齐；44 px 普通行、40 px 小窗口行 |
| A | 评分卡片按内容自然高度排列，保存按钮紧跟评分输入；说明移到卡片外；小窗口时表单可滚动，保存按钮保持可达 |
| B | 本地品牌、导航、检索、保存、新增等统一线性图标；资源包含普通与 @2x PNG；数字增减仅统一色值 |
| B | 选中摘要读取已保存的书名、编号及作者；未保存输入不会冒充已保存数据；未选中使用中性底色；长标题与目录省略并保留完整 tooltip |
| B | 分类、推荐来源及借还状态由现有 RowDelegate 绘制轻量圆角标签；保留 DisplayRole 原文本与 tooltip，不为单元格添加 QWidget |
| B | 借出 / 归还分区、空状态图形与提示、补全弹层、路径对话框、错误框及状态栏视觉统一 |

统计、分数及日期数字使用 Consolas，中文标签继续使用 Microsoft YaHei UI。实测 20 px 半粗数字 0～9 的字宽均为 11 px，避免仅指定 Segoe UI 时数字宽度不一致。

### 保留的接口与语义

保留 `mainTabs`、三页次序、全部既有 objectName、QLineEdit / QSpinBox 类型和业务连接。图书仍为五列、推荐四列、借阅六列。默认 Top K=2，分数四位小数，排序与来源不变；借阅空日期仅显示“—”。

数字增减仍为横排减号 / 加号，各 28×28，间隔 2 px，Top K 宽 136 px；QSS 右 padding **42 px**，连同 Qt 自身预留空间防止 `1000` 被遮住。范围、初值、普通步长、直接输入、键盘和长按均沿用现有行为。

## 3. 本次真实构建与测试

环境：Windows / Qt 6.11.2 MinGW 64-bit / MinGW 13.1 / Debug。

原 Qt 程序仍在原构建目录中运行。本轮采用独立目录 **`build/UiRefine_Debug`**，保留现有运行窗口与演示数据。

```powershell
$env:PATH='D:\Qt\6.11.2\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build/UiRefine_Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/UiRefine_Debug -j 4
& 'D:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build/UiRefine_Debug --output-on-failure --timeout 30
```

结果：**应用和测试构建成功，7/7 CTest 通过**。本轮重新执行，非沿用 10 月 8 日记录。扩展现有 `ui_polish_smoke`，原断言保留。

| 核对 | 结果 |
| --- | --- |
| 评分卡片 | 保存按钮与评分框距离 8～24 px；卡片自然高度小于内容区，余下区域为窗口底色 |
| 摘要 | 选中 B001 后显示保存的《算法图解》；更改未保存输入不影响摘要；筛选后仍明确原编辑对象 |
| 表格与补全 | 原列数及原文本保留；所有单元格 tooltip 与原文本一致，无 cellWidget；补全来自实际四个图书编号 |
| 目录 | 长中文目录不撑大窗口，可见省略号，tooltip 保留完整路径；路径对话框只读且可复制 |
| 数字控件 | 尺寸、点击、边界、最大值完整显示、直接输入、键盘、900 ms 长按重复、聚焦不移动及整框禁用通过 |
| 小样例 | U0 初始 B003=3.8889、B004=2.1111；图书增删改查、失败保留输入、引用删除拒绝、借还库存 2→1→2、评分与重启通过 |
| Goodbooks 副本 | 100/40/800/10、默认 U00；初始 Top 5 保持 B051/B059/B068/B085/B090；评分 B051 后退出候选，重启仍为 801 条评分 |
| Goodbooks 借还 | U00 借 B004 生成 R011，库存 3→2，归还回到 3；重启保留 11 条借阅、R011 已归还和库存 3 |
| 异常与空状态 | 重复编号、引用删除、重复借还、无效推荐用户仍报错；图书、推荐、借阅空状态可显示 |

两套数据均使用独立可写临时副本。Goodbooks 的模拟库存、分类、用户名称和借阅字段继续遵循 [来源说明](../demo_data/goodbooks/SOURCE.md)，不声明为真实借阅数据。

## 4. 实机截图

### 三页与代表状态

| 页面 | 选中 / 操作状态 | 输入聚焦 |
| --- | --- | --- |
| 图书 | [图书列表与选中摘要](ui-refine/sample-books.png) | [书名聚焦](ui-refine/sample-books-focus.png) |
| 推荐 | [推荐结果与紧凑评分卡](ui-refine/sample-recommend.png) | [评分聚焦](ui-refine/sample-recommend-focus.png) |
| 借还 | [借还分区与状态标签](ui-refine/sample-borrow.png) | [归还编号聚焦](ui-refine/sample-borrow-focus.png) |

公开数据：[图书页](ui-refine/goodbooks-books.png)、[推荐页](ui-refine/goodbooks-recommend.png)、[借还页](ui-refine/goodbooks-borrow.png)。

辅助状态：[未选中与长目录](ui-refine/neutral-and-long-directory.png)、[补全列表](ui-refine/id-completion.png)、[新增错误](ui-refine/sample-error.png)、[删除确认](ui-refine/sample-delete-confirm.png)、[空图书](ui-refine/books-empty.png)、[空推荐](ui-refine/recommend-empty.png)、[空借阅](ui-refine/borrow-empty.png)。

### 尺寸与缩放

| 条件 | 实际方式 | 截图 |
| --- | --- | --- |
| 1280×720 | CTest offscreen 配置，DPR=1，启动尺寸受可用区域约束；检查所有输入与按钮通过滚动可达 | 使用行为及几何断言，视觉以原生窗口为准 |
| 1000×680，150% | 原生 Windows QPA，DPR=1.5 | [图书](ui-refine/sample-compact-books.png) / [推荐](ui-refine/sample-compact-recommend.png) / [借还](ui-refine/sample-compact-borrow.png) |
| 1000×680，125% | `QT_SCREEN_SCALE_FACTORS=1.25`，实际 DPR=1.25 | [图书](ui-refine/dpi-125/sample-compact-books.png) / [推荐](ui-refine/dpi-125/sample-compact-recommend.png) / [借还](ui-refine/dpi-125/sample-compact-borrow.png) |
| 1000×680，100% | 本机 1.5 DPR 乘 `QT_SCALE_FACTOR=0.6666666666666666`，实际 DPR=1.0 | [图书](ui-refine/dpi-100/sample-compact-books.png) / [推荐](ui-refine/dpi-100/sample-compact-recommend.png) / [借还](ui-refine/dpi-100/sample-compact-borrow.png) |

数字状态总览：[100%](ui-refine/states-100.png)、[125%](ui-refine/states-125.png)、[150%](ui-refine/states-150.png)。包含默认、悬停、按下、最小 / 最大、聚焦和禁用；由实际控件逐项抓取排列。

每档均完整运行两套数据及原有数字交互检查。表单短于全部内容时通过滚动访问，固定保存、删除、评分、归还按钮不会被裁到滚动区外。

## 5. 复现及限制

原生截图：设置 `UI_SCREENSHOT_DIR` 后直接运行 `build/UiRefine_Debug/ui_polish_smoke.exe`；数字状态图设置 `SPIN_SCREENSHOT_DIR` 和 `SPIN_SCREENSHOT_TAG`。CTest 会清空截图输出变量，避免 offscreen 覆盖原生图片。

- A、B 两批均已实现。本轮未增加任务书外的业务功能。
- 100% / 125% 为 Qt 有效比例覆盖，150% 为本机当前比例；没有切换 Windows 系统设置，也未在独立 1280×720 物理屏幕上排练。
- 本次为实际 Qt 控件自动操作、截图与人工图像检查；不代表两名成员已完成最终现场演示或讲解。
- 沿用干净数据副本进行现场验收。独立目录中的新版可在 Qt 运行环境启动；原窗口需关闭并重新构建/启动后才会显示新界面。
