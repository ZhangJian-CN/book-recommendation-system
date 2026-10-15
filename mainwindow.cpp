// 界面操作流程为“读取输入 -> 调用业务层 -> 失败提示/成功刷新”。处理各种操作。
// 评分和推荐的主要入口是 rateBook、showRecommendations；这里不重复实现核心算法。
#include "mainwindow.h"
#include "recommendation.h"
#include "ui_mainwindow.h"

#include <QColor>
#include <QDate>
#include <QStringListModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showBookRows(const QVector<int> &rows)
{
    // rows 是原图书数组下标，displayRow 是当前筛选表格的行号，两者可能不同。
    // 例：只检索到 B004 时 rows=[3]，它显示在表格第 0 行，仍读取实体数组第 3 项。
    table_->clearSelection();
    table_->setRowCount(rows.size());
    for (int displayRow = 0; displayRow < rows.size(); ++displayRow) {
        const Book &book = library_.books().all()[rows[displayRow]];
        const QStringList values{book.id, book.title, book.author, book.category,
                                 QString::number(book.stock)};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setToolTip(values[column]);
            if (column == 0 || column == 4)
                item->setTextAlignment(Qt::AlignCenter);
            table_->setItem(displayRow, column, item);
        }
    }
    bookSummary_->setText(QStringLiteral("显示 %1 / %2 本图书 · 点击记录编辑")
                              .arg(rows.size()).arg(library_.books().all().size()));
    booksEmpty_->setVisible(rows.isEmpty());
    updateBookSelection();
}

void MainWindow::refreshTable()
{
    // 生成所有实体的数组下标，因此此函数显示完整列表，保存图书后也会恢复完整列表。
    QVector<int> rows;
    for (int i = 0; i < library_.books().all().size(); ++i)
        rows.append(i);
    showBookRows(rows);
    refreshSummary();
}

void MainWindow::refreshSummary()
{
    // 顶部四个数量从内存记录数产生；图书增删后同步补全列表，避免建议已删除编号。
    const qsizetype values[]{library_.books().all().size(), library_.users().size(),
                             library_.ratings().size(), library_.borrowRecords().size()};
    for (int i = 0; i < 4; ++i)
        counts_[i]->setText(QString::number(values[i]));
    QStringList ids;
    for (const Book &book : library_.books().all())
        ids.append(book.id);
    if (bookIds_)
        bookIds_->setStringList(ids);
}

void MainWindow::updateBookSelection()
{
    // 是否允许保存/删除取决于 selectedId_ 是否仍存在，不取决于筛选表格是否有选中行。
    const bool selected = library_.books().findById(selectedId_) != nullptr;
    saveButton_->setEnabled(selected);
    deleteButton_->setEnabled(selected);
    if (!selected) {
        bookSelection_->setText(QStringLiteral("尚未选择图书"));
        return;
    }
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (table_->item(row, 0)->text() == selectedId_) {
            table_->selectRow(row);
            bookSelection_->setText(QStringLiteral("当前图书 · %1").arg(selectedId_));
            return;
        }
    }
    bookSelection_->setText(QStringLiteral("当前图书 · %1\n不在筛选结果中，仍可编辑此图书。")
                                .arg(selectedId_));
}

void MainWindow::selectBook(const QString &id)
{
    // 按编号经哈希索引找到图书，再填入编辑表单；selectedId_ 跟踪真正的编辑对象。
    // 调用链：按编号按钮 -> selectBook -> BookRepository::findById -> BookIndex::rowForId。
    const Book *book = library_.books().findById(id);
    if (!book) {
        // 未找到就清空编辑对象和旧字段，避免用户误以为仍可修改刚才那本书。
        selectedId_.clear();
        table_->clearSelection();
        title_->clear();
        author_->clear();
        category_->clear();
        stock_->setValue(0);
        updateBookSelection();
        statusBar()->showMessage(QStringLiteral("未找到图书编号：%1").arg(id), 5000);
        return;
    }
    selectedId_ = id;
    newId_->clear();
    search_->setText(id);
    title_->setText(book->title);
    author_->setText(book->author);
    category_->setText(book->category);
    stock_->setValue(book->stock);
    table_->clearSelection();
    updateBookSelection();
    statusBar()->showMessage(QStringLiteral("已选中图书 %1").arg(id));
}

void MainWindow::saveSelected()
{
    // 使用已选编号与表单字段调用 updateBook；只有业务层保存成功后才刷新表格。
    // 调用链：保存按钮 -> saveSelected -> LibraryStore::updateBook -> BookRepository::save -> Csv::write。
    if (selectedId_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("尚未选择图书"),
                                 QStringLiteral("请先按编号查找或点击表格中的图书。"));
        return;
    }
    const Book book{selectedId_, title_->text().trimmed(), author_->text().trimmed(),
                    category_->text().trimmed(), stock_->value()};
    // &error 将错误文本交回界面；失败时保留表单输入，用户可修正后再次保存。
    QString error;
    if (!library_.updateBook(book, &error)) {
        statusBar()->showMessage(QStringLiteral("保存失败"));
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return;
    }
    refreshTable();
    selectBook(selectedId_);
    statusBar()->showMessage(QStringLiteral("已保存图书 %1").arg(selectedId_));
}

void MainWindow::addBook()
{
    // 使用新增编号构造 Book，业务层检查重复编号和字段，并写回 books.csv。
    // 新增用 newId_，修改用 selectedId_；二者分开，避免输入新编号时误改已有书。
    const Book book{newId_->text().trimmed(), title_->text().trimmed(),
                    author_->text().trimmed(), category_->text().trimmed(), stock_->value()};
    QString error;
    if (!library_.addBook(book, &error)) {
        statusBar()->showMessage(QStringLiteral("新增失败"));
        QMessageBox::warning(this, QStringLiteral("新增失败"), error);
        return;
    }
    refreshTable();
    selectBook(book.id);
    statusBar()->showMessage(QStringLiteral("已新增并保存图书 %1").arg(book.id));
}

void MainWindow::deleteSelected()
{
    // 用户确认后交给业务层检查引用限制，删除成功再清空选择并刷新界面。
    // 用户点“是”只表示确认操作，业务层仍可因评分或借阅历史引用拒绝删除。
    if (selectedId_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("尚未选择图书"),
                                 QStringLiteral("请先选择要删除的图书。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定删除图书 %1？").arg(selectedId_))
        != QMessageBox::Yes)
        return;
    const QString id = selectedId_;
    QString error;
    if (!library_.deleteBook(id, &error)) {
        statusBar()->showMessage(QStringLiteral("删除失败"));
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
        return;
    }
    selectedId_.clear();
    search_->clear();
    title_->clear();
    author_->clear();
    category_->clear();
    stock_->setValue(0);
    refreshTable();
    statusBar()->showMessage(QStringLiteral("已删除并保存图书 %1").arg(id));
}

void MainWindow::rateBook()
{
    // 评分先写回 ratings.csv；评分用户与当前推荐用户相同时自动重算推荐。
    // 调用链：保存评分按钮 -> rateBook -> LibraryStore::rate -> saveRatings -> Csv::write。
    // 评分记录是持久化数据；评分矩阵是下一次推荐时根据记录建立的计算结构。
    const QString userId = ratingUser_->text().trimmed();
    const QString bookId = ratingBook_->text().trimmed();
    QString error;
    if (!library_.rate(userId, bookId, ratingScore_->value(), &error)) {
        statusBar()->showMessage(QStringLiteral("评分失败"));
        QMessageBox::warning(this, QStringLiteral("评分失败"), error);
        return;
    }
    if (recommendUser_->text().trimmed() == userId)
        showRecommendations();
    // 当前自动刷新条件是用户编号相同；其他用户的评分保存后可点击“重新推荐”重算。
    refreshSummary();
    statusBar()->showMessage(QStringLiteral("已保存 %1 对 %2 的评分").arg(userId, bookId));
}

void MainWindow::showRecommendations()
{
    // 新建服务会使用最新评分重建矩阵，随后调用协同过滤和 Top K；界面只展示结果。
    // 调用链：重新推荐按钮 -> showRecommendations -> 构造 RecommendationService -> recommend。
    const QString userId = recommendUser_->text().trimmed();
    if (!library_.hasUser(userId)) {
        statusBar()->showMessage(QStringLiteral("推荐失败：用户编号不存在"));
        QMessageBox::warning(this, QStringLiteral("推荐失败"),
                             QStringLiteral("用户编号 %1 不存在").arg(userId));
        return;
    }
    RecommendationService service(library_.books(), library_.users(), library_.ratings());
    // topK_ 是用户希望展示的条数；结果已在业务算法中排好序，界面不再修改排名。
    const auto results = service.recommend(userId, topK_->value());
    // 分数保留四位小数，并注明来源，便于现场核对手算预测与热门回退。
    // 原始小样例 U0 应显示 B003=3.8889、B004=2.1111；给 B003 评分后它退出候选。
    recommendTable_->setRowCount(results.size());
    for (int row = 0; row < results.size(); ++row) {
        const Recommendation &result = results[row];
        const QString source = result.source == RecommendationSource::Collaborative
                                   ? QStringLiteral("协同过滤预测")
                                   : QStringLiteral("热门回退（平均分）");
        const QStringList values{result.bookId, result.title,
                                 QString::number(result.score, 'f', 4), source};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setToolTip(values[column]);
            if (column == 0 || column == 2)
                item->setTextAlignment(Qt::AlignCenter);
            if (column == 3)
                item->setForeground(QColor(result.source == RecommendationSource::Collaborative
                                               ? "#0F766E" : "#92400E"));
            recommendTable_->setItem(row, column, item);
        }
    }
    recommendSummary_->setText(QStringLiteral("用户 %1 · 共 %2 条推荐 · 分数保留四位小数")
                                   .arg(userId).arg(results.size()));
    recommendEmpty_->setVisible(results.isEmpty());
    statusBar()->showMessage(QStringLiteral("用户 %1 的 Top %2 推荐已更新")
                                 .arg(userId).arg(results.size()));
}

void MainWindow::refreshBorrowTable()
{
    // 空归还日期仅在界面显示为“—”，原 CSV 中仍是空字段，显示格式不改变保存格式。
    borrowTable_->setRowCount(library_.borrowRecords().size());
    for (int row = 0; row < library_.borrowRecords().size(); ++row) {
        const BorrowRecord &record = library_.borrowRecords()[row];
        const QStringList values{record.id, record.userId, record.bookId,
                                 record.borrowDate, record.returnDate.isEmpty()
                                                        ? QStringLiteral("—") : record.returnDate,
                                 record.status == QStringLiteral("borrowed")
                                     ? QStringLiteral("未归还") : QStringLiteral("已归还")};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setToolTip(values[column]);
            item->setTextAlignment(Qt::AlignCenter);
            if (column == 5) {
                const bool borrowed = record.status == QStringLiteral("borrowed");
                item->setForeground(QColor(borrowed ? "#92400E" : "#0F766E"));
                item->setBackground(QColor(borrowed ? "#FFF4DE" : "#E6F4F1"));
            }
            borrowTable_->setItem(row, column, item);
        }
    }
    borrowSummary_->setText(QStringLiteral("共 %1 条记录 · 点击记录填写归还编号")
                                .arg(library_.borrowRecords().size()));
    borrowEmpty_->setVisible(library_.borrowRecords().isEmpty());
    updateBorrowSelection();
    refreshSummary();
}

void MainWindow::updateBorrowSelection()
{
    const QString id = returnRecord_->text().trimmed();
    borrowTable_->clearSelection();
    for (int row = 0; row < library_.borrowRecords().size(); ++row) {
        const BorrowRecord &record = library_.borrowRecords()[row];
        if (record.id == id) {
            borrowSelection_->setText(QStringLiteral("%1 · %2\n用户 %3 / 图书 %4")
                .arg(id, record.status == QStringLiteral("borrowed")
                             ? QStringLiteral("未归还") : QStringLiteral("已归还"),
                     record.userId, record.bookId));
            borrowTable_->selectRow(row);
            return;
        }
    }
    borrowSelection_->setText(id.isEmpty() ? QStringLiteral("尚未选择借阅记录")
                                           : QStringLiteral("未找到借阅记录 · %1").arg(id));
}

void MainWindow::borrowBook()
{
    // 业务层成功借出后更新图书库存、借阅表及推荐，库存为零的书随重算移出候选。
    QString recordId;
    QString error;
    const QString date = QDate::currentDate().toString(Qt::ISODate);
    if (!library_.borrowBook(borrowUser_->text().trimmed(), borrowBook_->text().trimmed(),
                             date, &recordId, &error)) {
        statusBar()->showMessage(QStringLiteral("借出失败"));
        QMessageBox::warning(this, QStringLiteral("借出失败"), error);
        return;
    }
    returnRecord_->setText(recordId);
    refreshTable();
    refreshBorrowTable();
    showRecommendations();
    statusBar()->showMessage(QStringLiteral("借出成功：%1，记录编号 %2").arg(date, recordId));
}

void MainWindow::returnBook()
{
    // 归还成功后刷新库存和记录，并重新计算当前推荐用户的结果。
    QString error;
    const QString date = QDate::currentDate().toString(Qt::ISODate);
    const QString recordId = returnRecord_->text().trimmed();
    if (!library_.returnBook(recordId, date, &error)) {
        statusBar()->showMessage(QStringLiteral("归还失败"));
        QMessageBox::warning(this, QStringLiteral("归还失败"), error);
        return;
    }
    refreshTable();
    refreshBorrowTable();
    showRecommendations();
    statusBar()->showMessage(QStringLiteral("归还成功：%1，记录编号 %2").arg(date, recordId));
}
