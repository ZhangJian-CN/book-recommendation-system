# 演示数据

`large` 是完全合成的数据，运行 `python tools/generate_large_demo.py` 可重新生成。

`goodbooks` 的书名、作者与 800 条评分来自公开 Goodbooks-10k 数据集；显示名、分类占位值、库存和 10 条借阅记录不是原数据。来源、许可、筛选规则及重建命令见 [goodbooks/SOURCE.md](goodbooks/SOURCE.md)。原始 ZIP 不纳入仓库。

两套目录各含 100 本图书、40 名用户、800 条评分和 10 条借阅记录。课程展示优先用 `goodbooks`；`large` 保留用于完全合成、可控的测试。最初的四本书小样例仍在仓库根目录的 `data` 中，用于推荐手算核对。

演示前把所用目录中的**四份业务 CSV**复制到另一个可写目录，再用 `--data-dir <该目录>` 启动程序。程序会写回借还、评分和图书修改，不要直接把仓库中的原始样例目录用于交互演示。

## 零评分新用户与热门榜单

`cold_start` 保留 `data` 的四本书、10 条评分和两条借阅记录，仅增加已登记且零评分的 `UCOLD,新用户`，初始规模为 **4 本图书 / 4 名用户 / 10 条评分 / 2 条借阅记录**。

Top 4 热门榜依次为 B001（3 人，3.6667）、B002（3 人，2.3333）、B003（2 人，3.0000）、B004（2 人，3.0000）。评分人数优先，人数相同再比较原始平均分，最后按编号升序。只纳入有评分且库存大于零的图书。

在项目根目录用 PowerShell 创建独立演示副本并启动：

```powershell
$env:PATH = 'D:/Qt/6.11.2/mingw_64/bin;D:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
$coldDemoDir = Join-Path $env:TEMP ('BookRecommendation-cold-start-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $coldDemoDir | Out-Null
Copy-Item -Path './demo_data/cold_start/*.csv' -Destination $coldDemoDir
& './build/ColdStartPopular_Debug/BookRecommendation.exe' --data-dir $coldDemoDir
```

重启验证沿用 `$coldDemoDir`；新一轮初始演示创建新副本。操作步骤见 [新用户热门榜演示](../docs/demo_rehearsal.md#新用户冷启动与热门榜单2026-10-22)，真实验证和截图见 [冷启动验证记录](../docs/cold_start_validation.md)。
