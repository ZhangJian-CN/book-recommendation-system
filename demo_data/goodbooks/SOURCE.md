# Goodbooks-10k 演示子集来源

本目录的图书书名、作者和评分取自 [zygmuntz/goodbooks-10k](https://github.com/zygmuntz/goodbooks-10k) 的 [v1.0 Release](https://github.com/zygmuntz/goodbooks-10k/releases/tag/v1.0)。原数据由该项目整理自 Goodreads。原仓库声明采用 [CC BY-SA 4.0](https://github.com/zygmuntz/goodbooks-10k/blob/master/LICENSE)；本目录对原数据作了筛选和编号转换，保留来源署名与许可链接，改编的数据同样按 CC BY-SA 4.0 提供。

从该 Release 下载 `books.zip` 和 `ratings.zip` 后运行：

```text
python tools/extract_goodbooks_demo.py <两个 ZIP 所在目录>
```

脚本核对原文件的 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| `books.zip` | `1e5e29f4037f2c4ccd99cb5f10d07316ec0e3c0fcefbcdf9698585c418f7bd4f` |
| `ratings.zip` | `2dd0ea8e800860279d7305e88a481ff12090fe5bb54b1321a3854a3b651ebd2e` |

转换规则：取原数据编号 1～100 的图书；在这些图书上按评分数选出前 40 名用户。先保留 3 本全部 40 人都评过的共同图书，再覆盖其余图书，最后每人补足 20 条真实评分，总计 800 条。评分值不修改。原图书编号 1 对应 `B001`，依此类推；源用户编号与 `U00`～`U39` 的映射在 `source_user_ids.csv`。此文件是来源映射，不是程序读取的第五份业务 CSV。

以下字段**不是原数据**：`users.csv` 的显示名为“匿名读者”；原数据没有本系统所需的分类、可借库存和图书馆借阅记录，因此分类统一填“未分类”，每册模拟初始可借 3 本、5 条未归还记录对应的库存减至 2，`borrow_records.csv` 的 10 条记录及日期均为模拟。演示和报告应表述为“公开图书与匿名评分数据，模拟库存和借阅记录”，不要称四份 CSV 全部为真实业务数据。
