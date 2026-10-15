# 再次点击取消选择：实现与验证（2026-10-15）

执行依据：`D:/PDF/课程设计（数据结构与算法）/output/再次点击条目取消选择与复位方案-20261015.md`。

## 分支与实现

从干净的 `feature/ui-details@134fbbe063ea7c54b7af2c589ae34352c7d0c136` 建立本地 `feature/selection-toggle`。

- 共用 `ResultsTable` 在鼠标左键按下、Qt 默认选择处理之前，保存业务编号与该行是否已经选中。原有图书、借阅 `cellClicked` 入口合并为选择/复位两个分支；推荐增加相同判定，正常首次选择沿用 Qt。
- 临时状态仅用于这次点击，释放后立即清除；模型行数变化、重置、数据更新也清除状态。快速第二次按下收到双击事件时，仍按一次普通按下处理，由释放触发唯一一次点击逻辑，没有重复绑定 `cellDoubleClicked`。
- 图书取消：清空 `selectedId_`、查找框、新书编号和图书表单，库存输入为 0；清除选择和当前索引；恢复未选中摘要，保存/删除禁用，新增可用。关键词与筛选结果保留。
- 借阅取消：清空归还编号，经现有 `textChanged` 更新摘要，再清除选择和当前索引；上方借阅输入保持，归还按钮可用逻辑保持。
- 推荐取消：仅清除行选择和当前索引，恢复最近成功推荐的用户/数量摘要；推荐列表、排序、输入和评分均保留。
- 选择模型使用 [`QItemSelectionModel::clear()`](https://doc.qt.io/qt-6/qitemselectionmodel.html#clear)。程序保存的图书对象另由复位函数清除；`selectBook()` 的编号查找、保存和新增后定位逻辑保持原样。
- 派生表格析构时断开模型到自身的连接，避免基类清空模型时回调已经结束生命周期的点击状态。

复位函数只操作界面，不调用业务写入。原样式、对象名、列序、数值范围、默认值及已有中文讲解注释保留。

## 构建与测试

环境：Windows、Qt 6.11.2 MinGW 64-bit、MinGW 13.1、Debug。独立构建目录：`build/UiSelectionToggle_Debug`。

```powershell
$env:PATH='D:\Qt\6.11.2\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build/UiSelectionToggle_Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/UiSelectionToggle_Debug -j 4
& 'D:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build/UiSelectionToggle_Debug --output-on-failure --timeout 30
```

修改前基线 **7/7 通过**。新增独立的 `selection_toggle_smoke`，原测试源码和断言保留。应用构建成功，当前完整结果 **8/8 CTest 通过**：

`bookrepository_smoke`、`mainwindow_smoke`、`ui_polish_smoke`、`selection_toggle_smoke`、`hash_smoke`、`core_smoke`、`borrow_smoke`、`large_demo_smoke`。

本轮测试向表格 viewport 发送真实的鼠标按下、释放及双击事件类型，不直接调用复位函数。小样例与 Goodbooks 均复制到独立的 `QTemporaryDir`；取消相关操作前后逐字节比较四份 CSV。

| 用例 | 验证结果 |
| --- | --- |
| 首次点击 B003 | 正常选中并回填，库存为真实值 1 |
| 再次点击 B003 的另一列 | 行选择与当前索引清除，表单清空、库存输入 0，摘要与按钮复位 |
| 未保存编辑后取消 | 新书编号、未保存书名和库存输入清空，真实 CSV 无变化；重新选择库存仍为 1 |
| 第三次点击、改选 B004 | 再次正常选择；点击不同条目直接切换 |
| 连续编号查找 | 保持 B003 选中，随后鼠标再次点击可取消 |
| 键盘选择后鼠标取消 | 方向键移动到 B004 后，点击已选行仍正确复位 |
| 快速重复点击 | 按下/释放与双击按下/释放均按次切换，无回弹；三页都覆盖 |
| 筛选与空表 | 筛选出 B004 后按业务编号判断；取消保留关键词与单行结果；空表点击不清除通过编号查找的编辑对象，也不崩溃 |
| 右键、表头、滚动条 | 不触发图书复位；Goodbooks 实际可见滚动条操作保留编辑对象 |
| 推荐取消 | 原结果与四位小数保留；未提交推荐用户、Top K、评分用户/图书/分数 3 保留；摘要仍是有效 U0 的 2 条结果 |
| 推荐刷新后点击 | 重新生成结果后首次点击正常选择，第二次可取消，无旧点击状态 |
| 借阅 R001 取消 | 清空归还编号、选择及当前索引，恢复中性摘要与记录数量；借阅用户 U2、图书 B003 保留；没有执行归还 |
| 按下期间刷新、随后释放 | 清除过期点击状态，刷新后的 B003 保持原有自动选中，不误触取消 |
| 保存、新增后定位 | 连续保存仍选中 B003；新增 B101 后自动选中新行，再次鼠标点击才取消 |
| 取消后的刷新与重启 | 显示全部不会重新启用旧编辑对象；关闭后重新加载，库存、评分数量与未归还状态保持；有意保存/新增后的取消也不再写 CSV |
| 生命周期与原回归 | 多次构造、关闭、重启不再触发析构断言；原借还、评分、推荐、错误边界、1000×680 与模拟 1280×720 布局测试继续通过 |

静态核对：`styles/`、`ui.qrc`、原始 CSV/演示 CSV、业务算法源码及既有测试源码相对起点无 diff；已有中文讲解注释无缺失。`main` 保持原提交，本轮本地提交，不推送远端。

## 三页实际 Qt 截图

下列六张图片均来自本轮 Windows 原生 Qt 测试窗口。图书前后截图还覆盖了“填写未保存内容后取消”的复位结果。

| 页面 | 已选中 | 再次点击后 |
| --- | --- | --- |
| 图书 | [B003 已选中](selection-toggle/books-selected.png) | [表单清空、库存输入 0、保存与删除禁用](selection-toggle/books-reset.png) |
| 推荐 | [推荐行已选中](selection-toggle/recommend-selected.png) | [取消高亮、结果与输入保留](selection-toggle/recommend-reset.png) |
| 借还 | [R001 已选中](selection-toggle/borrow-selected.png) | [归还编号与摘要复位](selection-toggle/borrow-reset.png) |

复现截图：将 `QT_QPA_PLATFORM` 设为 `windows`，设置 `TOGGLE_SCREENSHOT_DIR` 为目标目录，运行 `build/UiSelectionToggle_Debug/selection_toggle_smoke.exe`。

新版主程序为 `build/UiSelectionToggle_Debug/BookRecommendation.exe`，也可在 Qt Creator 对当前分支重新构建并运行。已有旧构建窗口不会自动更新。本方案要求的实现和验证均已完成。
