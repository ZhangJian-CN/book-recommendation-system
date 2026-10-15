# 表格选中行白缝：修正与验证（2026-10-15）

依据：`D:/PDF/课程设计（数据结构与算法）/output/表格选中行白缝修正方案-20261015.md`。

起点：干净的 `feature/selection-toggle@fa8ebc0f82f82c1dbfddbdc27fe558ea933217af`。本轮独立分支：`fix/table-selection-seams`。

## 实际原因验证与采用方案

先为原代码增加像素检查，保留原来的交替行状态与绘制委托，运行 Windows 原生 Qt 基线；随后仅关闭共用 `configureTable()` 的交替行，再以同一套检查对比。

| 阶段 | 交替行 | 隔行色 QSS | 150% 下 108 个状态的结果 |
| --- | --- | --- | --- |
| 原版 | 开启 | 保留 | 44 个选中状态检测到背景色差；12 个取消状态仍为原隔行近白色 |
| 第一步，仅一行代码 | 关闭 | 保留 | 108 个状态全部通过 |
| 最终版 | 关闭 | 删除 | 108 个状态全部通过；100%、125% 分别也全部通过 |

原版图书第 2 行的空白采样带在列边界出现 `#F3F8F5`，主体为 `#EAF2ED`，与方案描述的颜色一致。例：[原始 PNG](table-selection-seams/before-150/booksTable-row2-focused.png) 中窗口物理坐标 `(178,659)` 可见浅缝。图书第 4 行、推荐和借阅表的相应交替行也复现了问题。

结论：交替行状态是本项目已验证的触发入口；关闭它即可消除当前白缝。最终运行时代码仅有两处改动：

1. `mainwindow_ui.cpp`：`setAlternatingRowColors(true)` 改为 `false`。
2. `styles/app.qss`：删除不再使用的 `alternate-background-color: #FDFEFC`。

共用 `RowDelegate` 完整保留。普通白底、整行浅绿色、淡横线、2 逻辑像素的左标记及徽标的原绘制方式继续使用。

## 像素检查范围

扩展现有 `selection_toggle_smoke`，保留本轮之前的全部断言，没有新增测试目标或改变 CMake 注册。新增检查使用临时 Goodbooks 数据，推荐条数设为 12，保证三页均可选择前四行并测试内部滚动。

每个有效缩放比例检查 **108 个状态**：

- 默认尺寸与 1000×680，两种尺寸下三页各选择第 1～4 行。
- 点击编号列、文字列和徽标列；逐项检查表格聚焦、右侧输入框聚焦、窗口失去激活、再次点击取消。
- 三页分别检查最大化、恢复窗口、滚动选择最后一行、取消后滚回顶部。
- 真正的 selection model 与当前索引保持原交互语义，取消时验证二者均已清除；图书及借阅输入复位继续验证。
- 视觉操作前后逐字节比较四份临时 CSV，内容一致。

采样使用 `window.grab()` 生成的原始物理像素图像，通过实际 DPR 与 viewport 在窗口内的位置换算坐标。在行底部避开文字、徽标、横线与左标记，横跨整行读取连续三条物理像素扫描线，包含所有可见列交界。选中时预期 `#EAF2ED`，取消后预期 `#FFFFFF`，要求扫描带内所有像素精确一致。保存的 CSV 包含状态、行号、尺寸、DPR、采样数、差异数及首个差异的坐标和颜色。

这是针对白缝的背景采样检查；文字、徽标与整体布局另通过原始 PNG 视觉核对，原数值控件和焦点标记断言继续由 `ui_polish_smoke` 验证。

## 三档原生 Qt 结果

| 有效显示缩放 | 实际 DPR | 状态数 | 采样物理像素数 | 差异状态数 |
| --- | --- | --- | --- | --- |
| 100% | 1 | 108 | 222660 | 0 |
| 125% | 1.25 | 108 | 272214 | 0 |
| 150% | 1.5 | 108 | 322416 | 0 |
| 合计 | — | 324 | 817290 | 0 |

本机原生 DPR 为 1.5，分别用进程内 `QT_SCALE_FACTOR=0.6666666666666666`、`0.8333333333333334`、`1` 验证上述有效 DPR。环境变量仅用于测试进程，未修改 Windows 显示设置。Qt 的有效 DPR 是覆盖比例与原生 DPR 的乘积，见 [Qt 高 DPI 测试说明](https://doc.qt.io/qt-6.11/highdpi.html#qt-scale-factor)。

原始记录：[修正前 150%](table-selection-seams/before-150/pixel-audit.csv)、[仅关闭交替行](table-selection-seams/step1-only-alternating-off.csv)、[最终 100%](table-selection-seams/after-100/pixel-audit.csv)、[最终 125%](table-selection-seams/after-125/pixel-audit.csv)、[最终 150%](table-selection-seams/after-150/pixel-audit.csv)。

### 原始 PNG 截图

| 页面 | 修正前 150% 第 2 / 4 行 | 修正后 125% 第 2 / 4 行 | 修正后 150% 第 2 / 4 行 |
| --- | --- | --- | --- |
| 图书 | [第 2 行](table-selection-seams/before-150/booksTable-row2-focused.png) / [第 4 行](table-selection-seams/before-150/booksTable-row4-focused.png) | [第 2 行](table-selection-seams/after-125/booksTable-row2-focused.png) / [第 4 行](table-selection-seams/after-125/booksTable-row4-focused.png) | [第 2 行](table-selection-seams/after-150/booksTable-row2-focused.png) / [第 4 行](table-selection-seams/after-150/booksTable-row4-focused.png) |
| 推荐 | [第 2 行](table-selection-seams/before-150/recommendTable-row2-focused.png) / [第 4 行](table-selection-seams/before-150/recommendTable-row4-focused.png) | [第 2 行](table-selection-seams/after-125/recommendTable-row2-focused.png) / [第 4 行](table-selection-seams/after-125/recommendTable-row4-focused.png) | [第 2 行](table-selection-seams/after-150/recommendTable-row2-focused.png) / [第 4 行](table-selection-seams/after-150/recommendTable-row4-focused.png) |
| 借还 | [第 2 行](table-selection-seams/before-150/borrowTable-row2-focused.png) / [第 4 行](table-selection-seams/before-150/borrowTable-row4-focused.png) | [第 2 行](table-selection-seams/after-125/borrowTable-row2-focused.png) / [第 4 行](table-selection-seams/after-125/borrowTable-row4-focused.png) | [第 2 行](table-selection-seams/after-150/borrowTable-row2-focused.png) / [第 4 行](table-selection-seams/after-150/borrowTable-row4-focused.png) |

100% 截图也保存在 `table-selection-seams/after-100/`。PNG 为原始抓取文件，未作后期缩放或加工；查看单像素差异时应使用 100% 显示或整数倍放大。

## 构建与完整回归

Windows / Qt 6.11.2 MinGW 64-bit / MinGW 13.1 / Debug；使用独立的 `build/TableSeams_Debug`。

```powershell
$env:PATH='D:\Qt\6.11.2\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build/TableSeams_Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/TableSeams_Debug -j 4
& 'D:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build/TableSeams_Debug --output-on-failure --timeout 30
```

修改前 **8/8 CTest 通过**；最终应用与测试构建成功，**8/8 CTest 通过**：

`bookrepository_smoke`、`mainwindow_smoke`、`ui_polish_smoke`、`selection_toggle_smoke`、`hash_smoke`、`core_smoke`、`borrow_smoke`、`large_demo_smoke`。

`selection_toggle_smoke` 继续覆盖首次选择、跨列取消、快速双击、图书解除编辑对象、推荐保留输入、借阅复位、键盘选择、筛选、刷新、保存/新增后自动定位以及重启。`ui_polish_smoke` 的 1000×680、模拟 1280×720、数字控件和焦点标记检查继续通过。

复现原生截图与像素记录：设置 `QT_QPA_PLATFORM=windows`、所需的 `QT_SCALE_FACTOR`，将 `SEAMS_SCREENSHOT_DIR` 设为输出目录，运行 `build/TableSeams_Debug/selection_toggle_smoke.exe`。

原始样例、演示数据、业务逻辑与中文讲解注释保留；`main` 保持原提交，本轮仅作本地修复提交。新版程序是 `build/TableSeams_Debug/BookRecommendation.exe`，也可在 Qt Creator 重新构建当前分支后查看。
