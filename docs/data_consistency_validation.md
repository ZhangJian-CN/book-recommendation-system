# 数据一致性改进验证记录

日期：2026-10-22（Asia/Shanghai）。依据《BookRecommendation数据一致性改进方案-交执行师-20261022.md》执行。方案中的假设是本轮设计选择，不作为教师原话或评分。

## 基线与改动范围

开始分支 `feature/cold-start-popular-20261022`，本轮分支 `feature/data-consistency-20261022`，HEAD 仍为 `6bd24e94a8323603f841ea9cda757764419a0c04`。本轮不创建提交，HEAD 不能代表完整工作区交付。

开始时全部 270 个受跟踪/未忽略未跟踪文件均已复制至 `build/DataConsistency_Debug/baseline/source`，并记录逐文件 SHA-256、Git 状态和完整补丁。重新配置构建后的起始测试为 **9/9 通过**，不引用点评日志替代本次执行。

本轮修改已有文件：`mainwindow.h/.cpp`、`mainwindow_ui.cpp`、`librarystore.h/.cpp`、`main.cpp`、`CMakeLists.txt`、`README.md`，以及直接相关的 `bookrepository_smoke.cpp`、`borrow_smoke.cpp`、`hash_smoke.cpp`、`core_smoke.cpp`、`cold_start_smoke.cpp`、`selection_toggle_smoke.cpp`、`ui_polish_smoke.cpp`。新增锁测试、独立评估入口、本记录及效果记录、结果 CSV/JSON、关键截图。

`recommendation.h/.cpp` 与基线逐字节一致。原始 `data`、`demo_data/goodbooks`、`demo_data/large`、`demo_data/cold_start` 保持原值。哈希/KMP/CSV 格式与推荐排序未顺便重构，旧的冷启动截图和未跟踪交付文件保留。

## R1：草稿与库存

用 `Book editBaseline_` 记录选择或成功保存时的实体基准。借还同一册时，未手改库存同步最新值，书名/作者/分类草稿保留；手工库存和最新库存同时偏离基准时保留草稿、标明冲突，保存先拒绝写盘。

冲突对话框展示原库存、最新库存和草稿库存。选择“使用最新库存”只更新库存与库存基准，其他草稿保留，再由用户确认保存；“取消”不写盘、不清空草稿。手工修改仍是可借数量绝对设值，不解释为增量。

`ui_polish_smoke --editor-consistency` 在修复前失败于 `stock->value() == 1`；修复后 R1-A～G 通过，涵盖借出后改名、归还后改作者、三项文本草稿、手工库存 7 的冲突取消/处理、借还另一册、借出失败、字段校验失败及真实写盘失败。窗口销毁后重新加载 CSV，核对 B004 的书名/作者/分类、库存和活动/已归还记录。

截图：[文本草稿保留](consistency/draft-after-borrow-1000x680.png)、[列表与编辑库存均为 1](consistency/stock-and-draft-after-borrow-1000x680.png)、[库存冲突](consistency/stock-conflict.png)。窄窗口表单允许内部滚动，截图分别展示文本与库存，不截取或重绘控件内容。

## R2：实际操作对象

鼠标和键盘由 selectionModel 的实际选中行驱动，以第 0 列的业务 ID 切换对象。仅有 current index 不代表业务选择。表格重建、清空及恢复高亮时，在实际发信号的 selectionModel 上使用局部 [QSignalBlocker](https://doc.qt.io/qt-6/qsignalblocker.html)，同对象刷新及重复编号查询保留草稿。

主动切换不同图书会替换旧草稿，并在状态栏说明；再次鼠标点击选中行仍取消并清空编辑区，不写盘。筛选隐藏当前图书时保留“仍可编辑”摘要，且无另一册误导性高亮。借阅表的选择、归还编号、摘要和最终归还记录同步；未知手输编号清空旧高亮。

`selection_toggle_smoke --consistency` 在修复前失败于“Down 后编辑编号应为 B002”；修复后验证 Up/Down/Home/End/PageUp/PageDown 的实际选择、无选择后的首次方向键、编号重复查询、筛选第 0 行实际为 B004、未保存草稿提示，以及键盘选中 R003 后只归还 R003。最终重载断言只修改了 B002/B004，R001 仍未归还，B004 库存正确。原有鼠标取消、键盘焦点与像素背景断言继续参与回归。

## R3：完整库存范围

库存 QSpinBox 使用 `std::numeric_limits<int>::max()`；本机范围 0～2147483647。保留原有 `toInt(&ok)` 与非负校验，并在新增/保存前检查控件输入可接受性。[QSpinBox 官方范围接口](https://doc.qt.io/qt-6/qspinbox.html)

修复前库存范围用例失败；修复后 UI 与仓库验证 0、1、1000000、1000001、INT_MAX−1、INT_MAX 的加载、选择、改名/保存和重载。最大值 10 位文本和加减按钮在 1000×680 下可用，见 [截图](consistency/int-max-1000x680.png)。非法 CSV（负数、非数字、2147483648）明确拒绝且原字节不变，非法控件输入不成功保存为另一个库存。

`borrow_smoke` 验证零库存借出失败、INT_MAX 借出减一再归还恢复、INT_MAX 下归还失败，重载后库存及未归还状态保持不变。原借还失败回滚、pending 恢复和重复记录断言保留。

## R4：同目录单写入

在读取快照及 `recoverBorrowTransaction` 之前取得规范化目录内的 `.bookrecommendation.lock`，由 LibraryStore 持有到销毁。所有新增、修改、删除、评分、借出、归还入口都要求成功加载且持锁。main 的首次初始化调用同一业务层临界区；忽略锁文件判断空目录，部分数据不覆盖。

按 [Qt 长时 QLockFile 用法](https://doc.qt.io/qt-6/qlockfile.html) 设置 staleLockTime=0 并立即 tryLock。正常关闭自动释放；异常退出遗留锁交给 Qt 按进程机制识别，不删除真实运行目录的锁文件。该协议只保护遵守它的本地进程，不宣称共享盘、旧版程序或手工 CSV 编辑器并发安全。

`store_lock_smoke` 在修改前失败于“第二个对象必须加载失败”；修改后覆盖：

- 同进程同目录、相对/绝对路径互斥；先后写入都能在第三次加载中看到。
- 不同目录分别读写；同对象重载复用锁；成功切换释放旧锁，失败切换保留旧状态和锁。
- 首次加载失败释放候选锁；锁路径异常时无写权限；未加载对象的六个写入口全部拒绝。
- 被拒绝的实例不提前执行 pending 事务恢复。
- 子进程以 READY/REFUSED 握手确认占用，正常退出释放，异常退出后可重新加载。
- 两个子进程同时首次初始化空目录，只有一个写入者，最终四份样例完整且为 4/3/10/2。
- 单独运行一次 `--long-lock`，活跃实例占用约 30.986 秒后仍拒绝后来者，日志单独保留；常规 CTest 不反复等待这段时间。

UI 另验证“目录正在使用”的错误以及禁用业务页，见 [占用提示](consistency/directory-in-use.png)。相关重启测试先销毁或显式释放旧快照，再创建新实例，未关闭生产锁、未删除原断言。

## 构建、交付与重现

工具链：Qt 6.11.2、MinGW 13.1.0、CMake 3.30.5、Ninja；Debug 独立构建目录 `build/DataConsistency_Debug`，Release 测量目录 `build/DataConsistency_Release`。

```powershell
$env:PATH = 'D:/Qt/6.11.2/mingw_64/bin;D:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' -S . -B build/DataConsistency_Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' --build build/DataConsistency_Debug -j 4
& 'D:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build/DataConsistency_Debug -N
& 'D:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build/DataConsistency_Debug --output-on-failure --timeout 30
```

Release 使用相同配置，改目录并设置 `-DCMAKE_BUILD_TYPE=Release`。效果评估和参数见 [推荐效果记录](recommendation_evaluation.md)。

本轮源工作区的阶段回归为 **10/10 通过**（原 9 项保留，新增 store_lock_smoke）；记录见 `build/DataConsistency_Debug/final-ctest.txt`。最终从实际交付 source 的空构建目录重新配置、编译、`ctest -N` 和全量 CTest，结果仍为 **10/10 通过、0 失败、总耗时 9.55 秒**，日志在交付目录 `logs/ctest-delivery.txt` 和 XML 中。`search_benchmark` 与 `recommendation_evaluation` 独立运行，不计入功能测试数。

完整交付目录为 `build/ConsistencyDelivery_20261022/`：`source/` 包含全部受跟踪与未忽略未跟踪文件，`file-manifest.json` 提供完整文件集合与 SHA-256，`logs/` 保存实际交付目录的新配置/构建/登记/CTest 日志，`build-debug/` 与 `build-release/` 来自这个 source。最终交付核对结果记在目录根部 `delivery-verification.json`，不只引用 Git HEAD。源码压缩包及其哈希保存在同目录。

交付物验证使用独立的新构建目录，再次执行了小样例、冷启动测试与 Release 评估；训练、测试、逐用户结果、汇总四份 CSV 与源工作区结果逐字节相同。原生 Windows 平台还启动了这个新构建的主程序，分别验证全新目录初始化为 4/3/10/2、冷启动副本为 4/4/10/2，实际主窗口完成创建，正常关闭后锁均释放，见 `logs/production-startup.json`。

交付前逐文件确认源码与清单一致；最后补入实际测试结论时只更新两份 Markdown，所有编译及数据输入保持与已验证构建一致。源码压缩包包含完整 source、清单、日志和效果结果，ZIP 的完整性哈希单独保存，不包含可重建的中间构建文件。

关键日志及逐项结果保留于上述交付目录；截图均由实际 Qt Widgets 抓取，并已检查布局。自动操作和文件断言证明本轮实现状态，不代表教师已重新验收或成员已完成现场讲解。
