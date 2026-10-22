// 【曹振江重点：图书页响应函数】
// 建议先读 showBookRows/selectBook，再读 saveSelected/addBook/deleteSelected。
// 界面操作流程为“读取输入 -> 调用业务层 -> 失败提示/成功刷新”，不在这里重写哈希或 CSV。
// 评分和推荐的主要入口是 rateBook、showRecommendations；这里不重复实现核心算法。
#include "mainwindow.h"
#include "recommendation.h"
#include "ui_mainwindow.h"

#include <QColor>
#include <QDate>
#include <QHeaderView>
#include <QElapsedTimer>
#include <QStyle>
#include <QStringListModel>
#include <QLabel>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>

MainWindow::~MainWindow()
{
    // ui 是生成的界面辅助对象，需要释放；窗口的子控件由 Qt 的父子关系管理生命周期。
    delete ui;
}

void MainWindow::showOperationMessage(const QString &message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

void MainWindow::showPageSummary()
{
    // 公共显示逻辑：当前标签页决定状态栏摘要，防止切到推荐页还显示旧的图书选择提示。
    auto *tabs = findChild<QTabWidget *>(QStringLiteral("mainTabs"));
    if (!tabs || !tabs->isEnabled())
        return;
    QString summary;
    if (tabs->currentIndex() == 0) {
        summary = library_.books().findById(selectedId_)
            ? QStringLiteral("已选中图书 %1").arg(selectedId_)
            : QStringLiteral("图书列表 · 显示 %1 / %2 本").arg(table_->rowCount()).arg(library_.books().all().size());
    } else if (tabs->currentIndex() == 1) {
        if (recommendMode_ == RecommendMode::GlobalPopular)
            summary = QStringLiteral("热门榜单 · %1 条 · 按评分人数优先排序").arg(lastRecommendCount_);
        else if (recommendMode_ == RecommendMode::ColdStartPopular)
            summary = QStringLiteral("用户 %1 暂无评分 · 热门榜单 %2 条").arg(lastRecommendUser_).arg(lastRecommendCount_);
        else
            summary = recommendMode_ == RecommendMode::None ? QStringLiteral("尚未生成推荐")
                : QStringLiteral("用户 %1 · %2 条推荐").arg(lastRecommendUser_).arg(lastRecommendCount_);
    } else {
        summary = QStringLiteral("借阅记录 · %1 条").arg(library_.borrowRecords().size());
        for (const BorrowRecord &record : library_.borrowRecords()) {
            if (record.id == returnRecord_->text().trimmed()) {
                summary = QStringLiteral("借阅记录 %1 · %2").arg(record.id,
                    record.status == QStringLiteral("borrowed") ? QStringLiteral("未归还") : QStringLiteral("已归还"));
                break;
            }
        }
    }
    statusBar()->showMessage(summary);
}

void MainWindow::showBookRows(const QVector<int> &rows)
{
    const QSignalBlocker selectionBlock(table_->selectionModel());
    // 【图书结果展示】rows 由“显示全部”或 KMP 产生，里面的值必须是实体数组的有效下标。
    // table_ 的一行只是展示结果；不要将它的行号直接拿去修改 library_.books().all()。
    // rows 是原图书数组下标，displayRow 是当前筛选表格的行号，两者可能不同。
    // 例：只检索到 B004 时 rows=[3]，它显示在表格第 0 行，仍读取实体数组第 3 项。
    table_->clearSelection();
    table_->setRowCount(rows.size());
    for (int displayRow = 0; displayRow < rows.size(); ++displayRow) { // 外层逐个生成显示行；次数是命中数，而非必定为全库数量。
        const Book &book = library_.books().all()[rows[displayRow]]; // 先用 rows 还原实体下标，再借用对应图书。
        const QStringList values{book.id, book.title, book.author, book.category,
                                 QString::number(book.stock)};
        for (int column = 0; column < values.size(); ++column) { // 内层把同一本书的五项字段按列顺序填进这一行。
            // 按固定列序生成单元格；tooltip 保留完整文字，长书名在表格中可以省略显示。
            auto *item = new QTableWidgetItem(values[column]);
            item->setToolTip(values[column]);
            if (column == 0 || column == 3 || column == 4)
                item->setTextAlignment(Qt::AlignCenter);
            item->setForeground(QColor(column == 0 ? "#647068" : column == 2 ? "#5D6B62" : "#22352C"));
            if (column == 0 || column == 1 || column == 4) {
                auto font = table_->font();
                if (column != 1)
                    font.setFamily(QStringLiteral("Segoe UI"));
                font.setWeight(column == 1 ? QFont::Medium : column == 4 ? QFont::DemiBold : QFont::Normal);
                item->setFont(font);
            }
            table_->setItem(displayRow, column, item); // 将新单元格交给表格管理；完成的是显示映射，不是保存 CSV。
        }
    }
    bookSummary_->setText(QStringLiteral("显示 %1 / %2 本")
                              .arg(rows.size()).arg(library_.books().all().size()));
    booksEmpty_->setVisible(rows.isEmpty());
    updateBookSelection();
}

void MainWindow::refreshTable()
{
    // 保存成功后重新从业务数据生成列表，而不是只把屏幕上的某个数字改成看起来成功。
    // 生成所有实体的数组下标，因此此函数显示完整列表，保存图书后也会恢复完整列表。
    QVector<int> rows;
    for (int i = 0; i < library_.books().all().size(); ++i) // 构造 0 到 N-1，表示当前完整实体数组的全部位置。
        rows.append(i); // 只生成整数下标，实际填充单元格复用 showBookRows。
    showBookRows(rows);
    refreshSummary();
}

void MainWindow::refreshSummary()
{
    // 这些数量来自已经加载/保存的内存实体；不是每次重新数 CSV 行，也不参与推荐分数计算。
    // 顶部四个数量从内存记录数产生；图书增删后同步补全列表，避免建议已删除编号。
    const qsizetype values[]{library_.books().all().size(), library_.users().size(),
                             library_.ratings().size(), library_.borrowRecords().size()};
    for (int i = 0; i < 4; ++i) // 四个统计标签与 values 中的图书、用户、评分、借阅数量一一对应。
        counts_[i]->setText(QString::number(values[i])); // 数量转成文字显示；不是当前筛选结果数。
    QStringList ids;
    for (const Book &book : library_.books().all()) // 收集最新图书编号，避免补全列表仍提示已经删除的书。
        ids.append(book.id); // 补全只需要编号，不复制整本书到模型。
    if (bookIds_)
        bookIds_->setStringList(ids);
}

void MainWindow::updateBookSelection()
{
    const QSignalBlocker selectionBlock(table_->selectionModel());
    table_->selectionModel()->clear();
    // 【业务选择与视觉选择】selectedId_ 是编辑对象，表格选中背景只是其在当前结果中的位置。
    // 图书暂时被关键词筛出列表时，仍可编辑已选实体；按钮状态以实体是否存在为准。
    // 是否允许保存/删除取决于 selectedId_ 是否仍存在，不取决于筛选表格是否有选中行。
    const bool selected = library_.books().findById(selectedId_) != nullptr;
    auto *selectionCard = bookSelection_->parentWidget();
    selectionCard->setProperty("selected", selected); // 动态属性让 QSS 选择不同的摘要卡样式。
    selectionCard->style()->unpolish(selectionCard);
    selectionCard->style()->polish(selectionCard);
    saveButton_->setEnabled(selected);
    deleteButton_->setEnabled(selected);
    if (!selected) {
        bookSelectionTitle_->setText(QStringLiteral("尚未选择图书"));
        bookSelectionTitle_->setToolTip({});
        bookSelection_->setText(QStringLiteral("按编号查找或点击列表"));
        bookSelection_->setToolTip({});
        return;
    }
    const Book *book = library_.books().findById(selectedId_);
    bookSelectionTitle_->setText(book->title);
    bookSelectionTitle_->setToolTip(book->title);
    const QString metadata = QStringLiteral("%1 · %2").arg(book->id, book->author);
    bookSelection_->setToolTip(metadata);
    for (int row = 0; row < table_->rowCount(); ++row) { // 遍历当前显示结果，查找编辑对象现在位于哪一行。
        if (table_->item(row, 0)->text() == selectedId_) { // 第一列保存业务编号，不用行号猜测是哪本书。
            table_->selectRow(row); // 将当前编辑对象高亮；实体与表单数据不在这句中修改。
            bookSelection_->setText(metadata);
            return;
        }
    }
    bookSelection_->setText(metadata + QStringLiteral("\n不在筛选结果中，仍可编辑此图书。"));
}

bool MainWindow::hasBookDraft() const
{
    return !selectedId_.isEmpty() && (title_->text() != editBaseline_.title
        || author_->text() != editBaseline_.author || category_->text() != editBaseline_.category
        || stock_->value() != editBaseline_.stock);
}

void MainWindow::updateStockNotice()
{
    const Book *current = library_.books().findById(selectedId_);
    const bool conflict = current && stock_->value() != editBaseline_.stock
        && current->stock != editBaseline_.stock;
    stockNotice_->setVisible(conflict);
    if (conflict)
        stockNotice_->setText(QStringLiteral("库存已变化：原 %1，最新 %2，草稿 %3。保存前需处理冲突。")
            .arg(editBaseline_.stock).arg(current->stock).arg(stock_->value()));
}

void MainWindow::syncEditedStock(const QString &bookId)
{
    if (selectedId_ != bookId)
        return;
    const Book *current = library_.books().findById(bookId);
    if (current && stock_->value() == editBaseline_.stock) {
        editBaseline_.stock = current->stock;
        stock_->setValue(current->stock);
    }
    updateStockNotice();
}

void MainWindow::selectBook(const QString &id, bool reload)
{
    // 【编号查询调用链】按钮或表格点击 -> 本函数 -> findById -> rowForId -> bucketForId。
    // const Book* 是仓库中的借用指针，这里只用于立即回填字段，不把它长期保存在窗口中。
    // 按编号经哈希索引找到图书，再填入编辑表单；selectedId_ 跟踪真正的编辑对象。
    // 调用链：按编号按钮 -> selectBook -> BookRepository::findById -> BookIndex::rowForId。
    const Book *book = library_.books().findById(id);
    if (!book) {
        // 未找到就清空编辑对象和旧字段，避免用户误以为仍可修改刚才那本书。
        selectedId_.clear();
        editBaseline_ = {};
        const QSignalBlocker selectionBlock(table_->selectionModel());
        table_->clearSelection();
        title_->clear();
        author_->clear();
        category_->clear();
        stock_->setValue(0);
        updateStockNotice();
        updateBookSelection();
        showOperationMessage(QStringLiteral("未找到图书编号：%1").arg(id), 5000);
        return;
    }
    if (selectedId_ == id && !reload) {
        updateBookSelection();
        showOperationMessage(QStringLiteral("已选中图书 %1，保留当前编辑输入").arg(id));
        return;
    }
    const bool discarded = selectedId_ != id && hasBookDraft();
    selectedId_ = id;
    editBaseline_ = *book;
    newId_->clear();
    search_->setText(id);
    title_->setText(book->title);
    author_->setText(book->author);
    category_->setText(book->category);
    stock_->setValue(book->stock);
    updateStockNotice();
    updateBookSelection();
    showOperationMessage(discarded ? QStringLiteral("已切换到图书 %1，上一对象的未保存输入已放弃").arg(id)
                                  : QStringLiteral("已选中图书 %1").arg(id));
}

void MainWindow::resetBookSelection()
{
    // 【再次点击归零】这里只解除编辑对象和清空表单；库存框显示 0 不等于把图书库存写成 0。
    // 没有调用 updateBook/save，所以已保存数据保持原值；未保存的表单输入会被清空。
    // 同时解除业务编辑对象、选中行和当前索引；仅复位界面，不修改真实图书或 CSV。
    selectedId_.clear();
    editBaseline_ = {};
    const QSignalBlocker selectionBlock(table_->selectionModel());
    table_->selectionModel()->clear();
    for (auto *edit : {search_, newId_, title_, author_, category_}) // 遍历这五个图书输入框，关键词框不在清空范围内。
        edit->clear(); // 清空控件文字，不调用图书修改或文件保存函数。
    stock_->setValue(0);
    updateStockNotice();
    updateBookSelection();
    showPageSummary();
}

void MainWindow::resetBorrowSelection()
{
    // 借还页简述：只解除归还对象，保留上方借出表单和真实借阅数据。
    returnRecord_->clear();
    const QSignalBlocker selectionBlock(borrowTable_->selectionModel());
    borrowTable_->selectionModel()->clear();
    showPageSummary();
}

void MainWindow::saveSelected()
{
    // 【修改与新增的区别】修改使用 selectedId_，新增使用 newId_；主键不能混用。
    // 处理顺序：检查选择 -> 收集 Book -> 调业务层 -> 失败提示 / 成功重建列表并定位该书。
    // 使用已选编号与表单字段调用 updateBook；只有业务层保存成功后才刷新表格。
    // 调用链：保存按钮 -> saveSelected -> LibraryStore::updateBook -> BookRepository::save -> Csv::write。
    if (selectedId_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("尚未选择图书"),
                                 QStringLiteral("请先按编号查找或点击表格中的图书。"));
        return;
    }
    if (!stock_->hasAcceptableInput()) {
        showOperationMessage(QStringLiteral("保存失败：库存输入无效"));
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("库存必须是 0～2147483647 的整数。"));
        return;
    }
    const Book *current = library_.books().findById(selectedId_);
    if (!current)
        return;
    if (stock_->value() != editBaseline_.stock && current->stock != editBaseline_.stock) {
        QMessageBox conflict(QMessageBox::Warning, QStringLiteral("库存冲突"),
            QStringLiteral("图书 %1 的库存已变化。\n原库存：%2\n最新库存：%3\n草稿库存：%4\n使用最新库存只更新库存输入；其他草稿保留，请确认后再次保存。")
                .arg(selectedId_).arg(editBaseline_.stock).arg(current->stock).arg(stock_->value()),
            QMessageBox::Cancel, this);
        auto *useCurrent = conflict.addButton(QStringLiteral("使用最新库存"), QMessageBox::AcceptRole);
        conflict.button(QMessageBox::Cancel)->setText(QStringLiteral("取消"));
        conflict.exec();
        if (conflict.clickedButton() == useCurrent) {
            editBaseline_.stock = current->stock;
            stock_->setValue(current->stock);
        }
        updateStockNotice();
        showOperationMessage(QStringLiteral("库存冲突：本次未保存，编辑草稿已保留"));
        return;
    }
    if (stock_->value() == editBaseline_.stock) {
        editBaseline_.stock = current->stock;
        stock_->setValue(current->stock);
    }
    const Book book{selectedId_, title_->text().trimmed(), author_->text().trimmed(),
                    category_->text().trimmed(), stock_->value()};
    // &error 将错误文本交回界面；失败时保留表单输入，用户可修正后再次保存。
    QString error;
    if (!library_.updateBook(book, &error)) {
        showOperationMessage(QStringLiteral("保存失败"));
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return;
    }
    refreshTable();
    selectBook(selectedId_, true);
    refreshRecommendations();
    showOperationMessage(QStringLiteral("已保存图书 %1").arg(selectedId_));
}

void MainWindow::addBook()
{
    if (!stock_->hasAcceptableInput()) {
        showOperationMessage(QStringLiteral("新增失败：库存输入无效"));
        QMessageBox::warning(this, QStringLiteral("新增失败"), QStringLiteral("库存必须是 0～2147483647 的整数。"));
        return;
    }
    // 参数由新编号及表单字段组成；编号重复、字段为空等失败原因由业务层返回，不自行绕过校验。
    // 使用新增编号构造 Book，业务层检查重复编号和字段，并写回 books.csv。
    // 新增用 newId_，修改用 selectedId_；二者分开，避免输入新编号时误改已有书。
    const Book book{newId_->text().trimmed(), title_->text().trimmed(),
                    author_->text().trimmed(), category_->text().trimmed(), stock_->value()};
    QString error;
    if (!library_.addBook(book, &error)) {
        showOperationMessage(QStringLiteral("新增失败"));
        QMessageBox::warning(this, QStringLiteral("新增失败"), error);
        return;
    }
    refreshTable();
    selectBook(book.id);
    refreshRecommendations();
    showOperationMessage(QStringLiteral("已新增并保存图书 %1").arg(book.id));
}

void MainWindow::deleteSelected()
{
    // 两个判断缺一不可：用户点击确认，并且业务层允许删除，才可以清空界面里的旧图书。
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
        showOperationMessage(QStringLiteral("删除失败"));
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
        return;
    }
    selectedId_.clear();
    search_->clear();
    editBaseline_ = {};
    title_->clear();
    author_->clear();
    category_->clear();
    stock_->setValue(0);
    refreshTable();
    refreshRecommendations();
    showOperationMessage(QStringLiteral("已删除并保存图书 %1").arg(id));
}

void MainWindow::rateBook()
{
    // 推荐侧联调入口：评分先保存，再按最近成功展示的用户/模式重算，不提交输入草稿。
    // 调用链：保存评分按钮 -> rateBook -> LibraryStore::rate -> saveRatings -> Csv::write。
    // 评分记录是持久化数据；评分矩阵是下一次推荐时根据记录建立的计算结构。
    const QString userId = ratingUser_->text().trimmed();
    const QString bookId = ratingBook_->text().trimmed();
    QString error;
    if (!library_.rate(userId, bookId, ratingScore_->value(), &error)) {
        showOperationMessage(QStringLiteral("评分失败"));
        QMessageBox::warning(this, QStringLiteral("评分失败"), error);
        return;
    }
    refreshRecommendations();
    refreshSummary();
    showOperationMessage(QStringLiteral("已保存 %1 对 %2 的评分").arg(userId, bookId));
}

void MainWindow::showRecommendations()
{
    // 新建服务会使用最新评分重建矩阵，随后调用协同过滤和 Top K；界面只展示结果。
    // 调用链：重新推荐按钮 -> showRecommendations -> 构造 RecommendationService -> recommend。
    const QString userId = recommendUser_->text().trimmed();
    if (!library_.hasUser(userId)) {
        showOperationMessage(QStringLiteral("推荐失败：用户编号不存在"));
        QMessageBox::warning(this, QStringLiteral("推荐失败"),
                             QStringLiteral("用户编号 %1 不存在").arg(userId));
        return;
    }
    lastRecommendUser_ = userId;
    lastRecommendTopK_ = topK_->value();
    recommendMode_ = RecommendMode::Personalized;
    refreshRecommendations();
    showPageSummary();
}

void MainWindow::showPopularRecommendations()
{
    // 浏览全局榜不校验或改写用户输入，也不创建用户。
    recommendMode_ = RecommendMode::GlobalPopular;
    lastRecommendTopK_ = topK_->value();
    refreshRecommendations();
    showPageSummary();
}

void MainWindow::refreshRecommendations()
{
    if (recommendMode_ == RecommendMode::None)
        return;
    QElapsedTimer phase;
    phase.start();
    RecommendationService service(library_.books(), library_.users(), library_.ratings());
    const qint64 matrixNs = phase.nsecsElapsed();
    phase.restart();
    const bool global = recommendMode_ == RecommendMode::GlobalPopular;
    if (!global)
        recommendMode_ = service.hasRatings(lastRecommendUser_)
            ? RecommendMode::Personalized : RecommendMode::ColdStartPopular;
    const bool popular = global || recommendMode_ == RecommendMode::ColdStartPopular;
    const auto results = global ? service.popularTopK(lastRecommendTopK_)
                                : service.recommend(lastRecommendUser_, lastRecommendTopK_);
    const qint64 computeNs = phase.nsecsElapsed();
    phase.restart();
    recommendHint_->setText(global
        ? QStringLiteral("热门榜单 · 按评分人数优先排序\n人数相同时按平均分、编号排序 · 平均分保留四位小数")
        : popular
            ? QStringLiteral("用户 %1 暂无评分，先为你推荐热门图书\n按评分人数、平均分、编号排序 · 平均分保留四位小数").arg(lastRecommendUser_)
            : QStringLiteral("优先协同预测，必要时热门回退 · 分数保留四位小数\n协同项按预测分排序；热门项按评分人数、平均分、编号排序"));
    recommendRule_->setText(popular
        ? QStringLiteral("热门榜单仅展示有评分且有可借库存的图书。\n全局榜不排除某个用户已评分的图书。")
        : QStringLiteral("个人推荐排除已评分及无库存图书。\n协同过滤显示预测分，热门回退显示平均分。"));
    recommendTable_->horizontalHeaderItem(2)->setText(popular ? QStringLiteral("平均分") : QStringLiteral("分数"));
    recommendTable_->horizontalHeaderItem(2)->setToolTip(popular
        ? QStringLiteral("已有评分的平均分；排序使用原始数值")
        : QStringLiteral("协同过滤预测：预测分；热门回退：平均分"));
    // 原有个人推荐也显示行号；热门模式的行号直接对应名次。
    recommendTable_->verticalHeader()->setVisible(true);
    // 分数保留四位小数，并注明来源，便于现场核对手算预测与热门回退。
    // 原始小样例 U0 应显示 B003=3.8889、B004=2.1111；给 B003 评分后它退出候选。
    QString selectedBook;
    if (!recommendTable_->selectedItems().isEmpty())
        selectedBook = recommendTable_->item(recommendTable_->selectedItems().first()->row(), 0)->text();
    recommendTable_->selectionModel()->clear();
    recommendTable_->setRowCount(results.size());
    for (int row = 0; row < results.size(); ++row) {
        const Recommendation &result = results[row];
        const QString source = result.source == RecommendationSource::Collaborative
                                   ? QStringLiteral("协同过滤预测")
                                   : popular ? QStringLiteral("热门榜单") : QStringLiteral("热门回退");
        const QStringList values{result.bookId, result.title,
                                 QString::number(result.score, 'f', 4), source,
                                 QString::number(result.ratingCount)};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setToolTip(values[column]);
            if (column == 0 || column == 2 || column == 4)
                item->setTextAlignment(Qt::AlignCenter);
            if (column == 3)
                item->setForeground(QColor(result.source == RecommendationSource::Collaborative
                                               ? "#245C4D" : "#8A5418"));
            else if (column == 0) {
                QFont font(QStringLiteral("Segoe UI"));
                font.setPixelSize(13);
                item->setFont(font);
                item->setForeground(QColor("#647068"));
            } else if (column == 2) {
                QFont font(QStringLiteral("Segoe UI"));
                font.setPixelSize(16);
                font.setWeight(QFont::DemiBold);
                item->setFont(font);
                item->setForeground(QColor("#245C4D"));
            }
            recommendTable_->setItem(row, column, item);
        }
        if (result.bookId == selectedBook)
            recommendTable_->selectRow(row);
    }
    lastRecommendCount_ = results.size();
    recommendSummary_->setText(global ? QStringLiteral("热门榜单 · %1 条").arg(results.size())
        : QStringLiteral("用户 %1 · %2 条").arg(lastRecommendUser_).arg(results.size()));
    recommendEmpty_->setText(popular
        ? QStringLiteral("暂无符合条件的热门图书，需有评分且有可借库存")
        : QStringLiteral("暂无可推荐图书\n请确认是否已评完所有图书，或候选图书库存为零。"));
    recommendEmpty_->setVisible(results.isEmpty());
    emit recommendationMeasured(matrixNs, computeNs, phase.nsecsElapsed());
}

void MainWindow::refreshBorrowTable()
{
    const QSignalBlocker selectionBlock(borrowTable_->selectionModel());
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
                item->setForeground(QColor(borrowed ? "#8A5418" : "#245C4D"));
                item->setBackground(QColor(borrowed ? "#FBF0DC" : "#EAF2ED"));
            } else {
                QFont font(QStringLiteral("Segoe UI"));
                font.setPixelSize(13);
                item->setFont(font);
                item->setForeground(QColor("#647068"));
            }
            borrowTable_->setItem(row, column, item);
        }
    }
    borrowSummary_->setText(QStringLiteral("%1 条")
                                .arg(library_.borrowRecords().size()));
    borrowEmpty_->setVisible(library_.borrowRecords().isEmpty());
    updateBorrowSelection();
    refreshSummary();
}

void MainWindow::updateBorrowSelection()
{
    const QSignalBlocker selectionBlock(borrowTable_->selectionModel());
    // 借还页简述：按记录编号定位归还对象，并展示它的用户、图书和归还状态。
    const QString id = returnRecord_->text().trimmed();
    borrowTable_->selectionModel()->clear();
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
        showOperationMessage(QStringLiteral("借出失败"));
        QMessageBox::warning(this, QStringLiteral("借出失败"), error);
        return;
    }
    returnRecord_->setText(recordId);
    syncEditedStock(borrowBook_->text().trimmed());
    refreshTable();
    refreshBorrowTable();
    refreshRecommendations();
    showOperationMessage(QStringLiteral("借出成功：%1，记录编号 %2").arg(date, recordId));
}

void MainWindow::returnBook()
{
    // 归还成功后刷新库存和记录，并重新计算当前推荐用户的结果。
    QString error;
    const QString date = QDate::currentDate().toString(Qt::ISODate);
    const QString recordId = returnRecord_->text().trimmed();
    QString affectedBook;
    for (const BorrowRecord &record : library_.borrowRecords()) {
        if (record.id == recordId) {
            affectedBook = record.bookId;
            break;
        }
    }
    if (!library_.returnBook(recordId, date, &error)) {
        showOperationMessage(QStringLiteral("归还失败"));
        QMessageBox::warning(this, QStringLiteral("归还失败"), error);
        return;
    }
    syncEditedStock(affectedBook);
    refreshTable();
    refreshBorrowTable();
    refreshRecommendations();
    showOperationMessage(QStringLiteral("归还成功：%1，记录编号 %2").arg(date, recordId));
}
