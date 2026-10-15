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
    QVector<int> rows;
    for (int i = 0; i < library_.books().all().size(); ++i)
        rows.append(i);
    showBookRows(rows);
    refreshSummary();
}

void MainWindow::refreshSummary()
{
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
    const Book *book = library_.books().findById(id);
    if (!book) {
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
    if (selectedId_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("尚未选择图书"),
                                 QStringLiteral("请先按编号查找或点击表格中的图书。"));
        return;
    }
    const Book book{selectedId_, title_->text().trimmed(), author_->text().trimmed(),
                    category_->text().trimmed(), stock_->value()};
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
    refreshSummary();
    statusBar()->showMessage(QStringLiteral("已保存 %1 对 %2 的评分").arg(userId, bookId));
}

void MainWindow::showRecommendations()
{
    const QString userId = recommendUser_->text().trimmed();
    if (!library_.hasUser(userId)) {
        statusBar()->showMessage(QStringLiteral("推荐失败：用户编号不存在"));
        QMessageBox::warning(this, QStringLiteral("推荐失败"),
                             QStringLiteral("用户编号 %1 不存在").arg(userId));
        return;
    }
    RecommendationService service(library_.books(), library_.users(), library_.ratings());
    const auto results = service.recommend(userId, topK_->value());
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
