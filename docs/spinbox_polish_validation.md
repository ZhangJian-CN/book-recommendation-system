# 数字增减按钮验证记录（2026-10-08）

依据：`D:/PDF/课程设计（数据结构与算法）/output/数字增减按钮美化方案-20261008.md`。
基线：`feature/ui-polish@860ac50`，开始前工作区干净。

## 完成内容

- 保留 `bookStock`、`topK`、`ratingScore` 三个 QSpinBox、对象名、范围、默认值、步长和信号连接。
- `number(...)` 设置 PlusMinus；QSS 显式定义横排内部按钮及符号，减号在左，加号在右，各 28×28，间隔 2 px，右侧内缩 4 px。
- 正常、悬停、按下、边界和整框禁用均使用方案指定颜色。资源中包含两种符号的四种颜色，以及 12 px / 24 px（@2x）透明 PNG。
- Top K 宽度改为 136 px；库存与评分宽度沿用表单。聚焦时边框仍为 1 px，编辑区与按钮位置不变；其他 QLineEdit 的样式未调整。
- 图标打包在 `ui.qrc`，实际应用及两项界面测试共用资源。不增加 UI 库，CSV 和算法文件未修改。

使用 [Qt 官方 QSpinBox 子控件样式](https://doc.qt.io/qt-6/stylesheet-examples.html#customizing-qspinbox) 中的按钮、符号和范围边界状态；同时覆盖图标，不仅依赖 [PlusMinus 设置](https://doc.qt.io/qt-6/qabstractspinbox.html#buttonSymbols-prop)。

## 实际发现并修正

初版将 CSS 右内边距直接设为 70 px 时，Qt 还会为步进子控件自动预留 28 px，导致 136 px 的 Top K 编辑区只剩 27 px，`1000` 被挤压。现改为 CSS 42 px，加上 Qt 的 28 px，实际预留约 70 px；编辑区为 55 px，原生 Windows 字体下四位数字完整显示。库存 `1000000` 同样核对通过。

为在 Windows offscreen 测试中准确检查字体宽度，界面测试在系统微软雅黑文件存在时用 Qt 加载该字体；不修改系统字体文件。原布局检查补充实际滚动条操作后仍保留“整个控件可达”的断言。

## 构建与测试

环境：Qt 6.11.2 MinGW 64-bit / MinGW 13.1 / Debug。

原构建目录的 `BookRecommendation.exe` 被正在运行的程序占用，链接阶段出现 `Permission denied`。使用独立构建目录 `build/UiSpinPolish_Debug` 完成应用及全部测试的构建，没有关闭原窗口。

```powershell
$env:PATH='D:\Qt\6.11.2\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build/UiSpinPolish_Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/UiSpinPolish_Debug -j 4
& 'D:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build/UiSpinPolish_Debug --output-on-failure --timeout 30
```

结果：**构建成功，7/7 CTest 通过**。扩展现有 `ui_polish_smoke`，测试总数不变，没有删除原断言。所有业务操作使用临时 CSV 副本。

| 项目 | 核对结果 |
| --- | --- |
| 几何及点击 | 三处按钮均为 28×28、横排、间隔 2 px；点击加/减各改变 1，编辑区不与按钮相交 |
| 范围 | 库存 0～1000000、Top K 1～1000、评分 1～5；默认值分别为 0、2、4，普通步长仍为 1 |
| 边界 | 最小值减号无效但加号可用；最大值加号无效但减号可用；不会循环越界 |
| 输入与键盘 | 键盘上下键增减 1；直接输入三个最大值成功；超出范围的输入不会放宽范围 |
| 长按 | Top K 加号保持按下 900 ms 后产生重复步进，沿用 Qt 原有定时行为 |
| 聚焦和禁用 | 聚焦前后编辑区和按钮几何不变；整框禁用时点击不改值，符号与输入变淡 |
| 原业务 | 小样例图书操作、借还库存、推荐手算基准，以及 Goodbooks 评分保存与窗口重建仍通过原测试 |

## 原生 Qt 状态截图与 DPI

三张状态总览从真实 MainWindow 中的三个控件逐项抓取，按列排成截图表；未重写或模拟控件绘制。包含默认、两个按钮的悬停/按下、最小值、最大值、聚焦和整框禁用。总览使用中间值显示默认按钮色，不代表修改了项目初始值。

| 有效缩放 | 原生 Windows QPA 实测 DPR | 状态截图 |
| --- | --- | --- |
| 100% | 1.0 | [100% 状态总览](spinbox-polish/states-100.png) |
| 125% | 1.25 | [125% 状态总览](spinbox-polish/states-125.png) |
| 150% | 1.5 | [150% 状态总览](spinbox-polish/states-150.png) |

完整页面：[图书页](spinbox-polish/books-page.png)、[评分与推荐页](spinbox-polish/recommend-page.png)。

本机系统缩放为 150%；100% 和 125% 使用 Qt 环境变量覆盖有效比例，没有切换 Windows 系统设置。三档均运行同一套点击、输入、键盘、边界、聚焦和长按检查，并检查符号清晰度与居中效果。

状态截图复现（工程根目录，已设置 Qt PATH）：

```powershell
$env:SPIN_SCREENSHOT_DIR='D:/Code/QtProjects/BookRecommendation/docs/spinbox-polish'
# 本机原生 150%
$env:SPIN_SCREENSHOT_TAG='150'
& .\build\UiSpinPolish_Debug\ui_polish_smoke.exe
# 覆盖为有效 125%
$env:QT_SCREEN_SCALE_FACTORS='1.25'
$env:SPIN_SCREENSHOT_TAG='125'
& .\build\UiSpinPolish_Debug\ui_polish_smoke.exe
Remove-Item Env:QT_SCREEN_SCALE_FACTORS
# 本机 1.5 DPR 乘 2/3，覆盖为有效 100%
$env:QT_SCALE_FACTOR='0.6666666666666666'
$env:SPIN_SCREENSHOT_TAG='100'
& .\build\UiSpinPolish_Debug\ui_polish_smoke.exe
```

当前已打开的旧程序不会随源文件修改而更新。关闭旧窗口后可在原 Qt Creator 构建目录重新构建；本次验证使用的新版位于独立构建目录，在 Qt 运行环境下启动。
