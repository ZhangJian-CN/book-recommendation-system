#include "mainwindow.h"
#include "booksearch.h"
#include "recommendation.h"
#include "ui_mainwindow.h"

#include <QFormLayout>
#include <QDate>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &dataDir, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), dataDir_(dataDir)
{
    ui->setupUi(this);
    resize(1000, 720);
    setWindowTitle(QStringLiteral("图书检索与个性化推荐系统"));
    auto *root = new QVBoxLayout(ui->centralwidget);
    auto *tabs = new QTabWidget(this);
    root->addWidget(tabs);

    auto *booksPage = new QWidget(this);
    tabs->addTab(booksPage, QStringLiteral("图书管理与检索"));
    auto *booksLayout = new QVBoxLayout(booksPage);
    auto *searchRow = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("bookSearch"));
    search_->setPlaceholderText(QStringLiteral("图书编号，如 B001"));
    auto *searchButton = new QPushButton(QStringLiteral("按编号查找"), this);
    searchButton->setObjectName(QStringLiteral("searchButton"));
    searchRow->addWidget(search_);
    searchRow->addWidget(searchButton);
    booksLayout->addLayout(searchRow);

    auto *keywordRow = new QHBoxLayout;
    keyword_ = new QLineEdit(this);
    keyword_->setObjectName(QStringLiteral("keywordSearch"));
    keyword_->setPlaceholderText(QStringLiteral("书名或作者关键词"));
    auto *keywordButton = new QPushButton(QStringLiteral("关键词检索"), this);
    keywordButton->setObjectName(QStringLiteral("keywordButton"));
    auto *showAllButton = new QPushButton(QStringLiteral("显示全部"), this);
    showAllButton->setObjectName(QStringLiteral("showAllButton"));
    keywordRow->addWidget(keyword_);
    keywordRow->addWidget(keywordButton);
    keywordRow->addWidget(showAllButton);
    booksLayout->addLayout(keywordRow);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("booksTable"));
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("书名"),
                                       QStringLiteral("作者"), QStringLiteral("分类"),
                                       QStringLiteral("库存")});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    booksLayout->addWidget(table_);

    auto *form = new QFormLayout;
    newId_ = new QLineEdit(this);
    newId_->setObjectName(QStringLiteral("newBookId"));
    newId_->setPlaceholderText(QStringLiteral("仅新增时填写"));
    title_ = new QLineEdit(this);
    title_->setObjectName(QStringLiteral("bookTitle"));
    author_ = new QLineEdit(this);
    category_ = new QLineEdit(this);
    stock_ = new QSpinBox(this);
    stock_->setObjectName(QStringLiteral("bookStock"));
    stock_->setRange(0, 1000000);
    form->addRow(QStringLiteral("新书编号"), newId_);
    form->addRow(QStringLiteral("书名"), title_);
    form->addRow(QStringLiteral("作者"), author_);
    form->addRow(QStringLiteral("分类"), category_);
    form->addRow(QStringLiteral("库存"), stock_);
    booksLayout->addLayout(form);
    auto *bookButtons = new QHBoxLayout;
    auto *addButton = new QPushButton(QStringLiteral("新增图书"), this);
    addButton->setObjectName(QStringLiteral("addBookButton"));
    auto *saveButton = new QPushButton(QStringLiteral("保存当前图书修改"), this);
    saveButton->setObjectName(QStringLiteral("saveButton"));
    auto *deleteButton = new QPushButton(QStringLiteral("删除当前图书"), this);
    deleteButton->setObjectName(QStringLiteral("deleteBookButton"));
    bookButtons->addWidget(addButton);
    bookButtons->addWidget(saveButton);
    bookButtons->addWidget(deleteButton);
    booksLayout->addLayout(bookButtons);

    auto *recommendPage = new QWidget(this);
    tabs->addTab(recommendPage, QStringLiteral("评分与推荐"));
    auto *recommendLayout = new QVBoxLayout(recommendPage);
    recommendLayout->addWidget(new QLabel(QStringLiteral("录入或更新评分（1～5 分）"), this));
    auto *ratingForm = new QFormLayout;
    ratingUser_ = new QLineEdit(QStringLiteral("U0"), this);
    ratingUser_->setObjectName(QStringLiteral("ratingUser"));
    ratingBook_ = new QLineEdit(QStringLiteral("B003"), this);
    ratingBook_->setObjectName(QStringLiteral("ratingBook"));
    ratingScore_ = new QSpinBox(this);
    ratingScore_->setObjectName(QStringLiteral("ratingScore"));
    ratingScore_->setRange(1, 5);
    ratingScore_->setValue(4);
    ratingForm->addRow(QStringLiteral("用户编号"), ratingUser_);
    ratingForm->addRow(QStringLiteral("图书编号"), ratingBook_);
    ratingForm->addRow(QStringLiteral("评分"), ratingScore_);
    recommendLayout->addLayout(ratingForm);
    auto *rateButton = new QPushButton(QStringLiteral("保存评分"), this);
    rateButton->setObjectName(QStringLiteral("rateButton"));
    recommendLayout->addWidget(rateButton);

    auto *recommendRow = new QHBoxLayout;
    recommendUser_ = new QLineEdit(QStringLiteral("U0"), this);
    recommendUser_->setObjectName(QStringLiteral("recommendUser"));
    topK_ = new QSpinBox(this);
    topK_->setObjectName(QStringLiteral("topK"));
    topK_->setRange(1, 1000);
    topK_->setValue(2);
    auto *recommendButton = new QPushButton(QStringLiteral("重新推荐"), this);
    recommendButton->setObjectName(QStringLiteral("recommendButton"));
    recommendRow->addWidget(new QLabel(QStringLiteral("用户"), this));
    recommendRow->addWidget(recommendUser_);
    recommendRow->addWidget(new QLabel(QStringLiteral("Top K"), this));
    recommendRow->addWidget(topK_);
    recommendRow->addWidget(recommendButton);
    recommendLayout->addLayout(recommendRow);
    recommendTable_ = new QTableWidget(this);
    recommendTable_->setObjectName(QStringLiteral("recommendTable"));
    recommendTable_->setColumnCount(4);
    recommendTable_->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("书名"),
                                                QStringLiteral("分数"), QStringLiteral("来源")});
    recommendTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recommendTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    recommendLayout->addWidget(recommendTable_);

    auto *borrowPage = new QWidget(this);
    tabs->addTab(borrowPage, QStringLiteral("借阅与归还"));
    auto *borrowLayout = new QVBoxLayout(borrowPage);
    borrowLayout->addWidget(new QLabel(QStringLiteral("借出与归还使用本机当天日期；同一用户同一本书只能有一条未归还记录。"), this));
    auto *borrowForm = new QFormLayout;
    borrowUser_ = new QLineEdit(QStringLiteral("U0"), this);
    borrowUser_->setObjectName(QStringLiteral("borrowUser"));
    borrowBook_ = new QLineEdit(QStringLiteral("B004"), this);
    borrowBook_->setObjectName(QStringLiteral("borrowBook"));
    borrowForm->addRow(QStringLiteral("用户编号"), borrowUser_);
    borrowForm->addRow(QStringLiteral("图书编号"), borrowBook_);
    borrowLayout->addLayout(borrowForm);
    auto *borrowButton = new QPushButton(QStringLiteral("借出图书"), this);
    borrowButton->setObjectName(QStringLiteral("borrowButton"));
    borrowLayout->addWidget(borrowButton);
    auto *returnRow = new QHBoxLayout;
    returnRecord_ = new QLineEdit(this);
    returnRecord_->setObjectName(QStringLiteral("returnRecord"));
    returnRecord_->setPlaceholderText(QStringLiteral("输入借阅编号，或点击下方记录"));
    auto *returnButton = new QPushButton(QStringLiteral("归还图书"), this);
    returnButton->setObjectName(QStringLiteral("returnButton"));
    returnRow->addWidget(new QLabel(QStringLiteral("借阅编号"), this));
    returnRow->addWidget(returnRecord_);
    returnRow->addWidget(returnButton);
    borrowLayout->addLayout(returnRow);
    borrowTable_ = new QTableWidget(this);
    borrowTable_->setObjectName(QStringLiteral("borrowTable"));
    borrowTable_->setColumnCount(6);
    borrowTable_->setHorizontalHeaderLabels({QStringLiteral("记录编号"), QStringLiteral("用户"),
                                             QStringLiteral("图书"), QStringLiteral("借出日期"),
                                             QStringLiteral("归还日期"), QStringLiteral("状态")});
    borrowTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    borrowTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    borrowLayout->addWidget(borrowTable_);

    connect(searchButton, &QPushButton::clicked, this, [this] {
        selectBook(search_->text().trimmed());
    });
    connect(search_, &QLineEdit::returnPressed, searchButton, &QPushButton::click);
    connect(keywordButton, &QPushButton::clicked, this, [this] {
        showBookRows(BookSearch::keywordRows(library_.books().all(), keyword_->text()));
        statusBar()->showMessage(QStringLiteral("关键词检索得到 %1 本图书").arg(table_->rowCount()));
    });
    connect(keyword_, &QLineEdit::returnPressed, keywordButton, &QPushButton::click);
    connect(showAllButton, &QPushButton::clicked, this, [this] { refreshTable(); });
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        selectBook(table_->item(row, 0)->text());
    });
    connect(addButton, &QPushButton::clicked, this, [this] { addBook(); });
    connect(saveButton, &QPushButton::clicked, this, [this] { saveSelected(); });
    connect(deleteButton, &QPushButton::clicked, this, [this] { deleteSelected(); });
    connect(rateButton, &QPushButton::clicked, this, [this] { rateBook(); });
    connect(recommendButton, &QPushButton::clicked, this, [this] { showRecommendations(); });
    connect(borrowButton, &QPushButton::clicked, this, [this] { borrowBook(); });
    connect(returnButton, &QPushButton::clicked, this, [this] { returnBook(); });
    connect(borrowTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        returnRecord_->setText(borrowTable_->item(row, 0)->text());
    });

    QString error;
    if (!library_.load(dataDir_, &error)) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        tabs->setEnabled(false);
    } else {
        refreshTable();
        refreshBorrowTable();
        showRecommendations();
        statusBar()->showMessage(QStringLiteral("已加载 %1 本图书、%2 条评分；数据目录：%3")
                                     .arg(library_.books().all().size())
                                     .arg(library_.ratings().size()).arg(dataDir_));
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showBookRows(const QVector<int> &rows)
{
    table_->setRowCount(rows.size());
    for (int displayRow = 0; displayRow < rows.size(); ++displayRow) {
        const Book &book = library_.books().all()[rows[displayRow]];
        const QStringList values{book.id, book.title, book.author, book.category,
                                 QString::number(book.stock)};
        for (int column = 0; column < values.size(); ++column)
            table_->setItem(displayRow, column, new QTableWidgetItem(values[column]));
    }
}

void MainWindow::refreshTable()
{
    QVector<int> rows;
    for (int i = 0; i < library_.books().all().size(); ++i)
        rows.append(i);
    showBookRows(rows);
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
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (table_->item(row, 0)->text() == id) {
            table_->selectRow(row);
            break;
        }
    }
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
        QMessageBox::warning(this, QStringLiteral("评分失败"), error);
        return;
    }
    if (recommendUser_->text().trimmed() == userId)
        showRecommendations();
    statusBar()->showMessage(QStringLiteral("已保存 %1 对 %2 的评分").arg(userId, bookId));
}

void MainWindow::showRecommendations()
{
    const QString userId = recommendUser_->text().trimmed();
    if (!library_.hasUser(userId)) {
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
        for (int column = 0; column < values.size(); ++column)
            recommendTable_->setItem(row, column, new QTableWidgetItem(values[column]));
    }
    statusBar()->showMessage(QStringLiteral("用户 %1 的 Top %2 推荐已更新")
                                 .arg(userId).arg(results.size()));
}

void MainWindow::refreshBorrowTable()
{
    borrowTable_->setRowCount(library_.borrowRecords().size());
    for (int row = 0; row < library_.borrowRecords().size(); ++row) {
        const BorrowRecord &record = library_.borrowRecords()[row];
        const QStringList values{record.id, record.userId, record.bookId,
                                 record.borrowDate, record.returnDate,
                                 record.status == QStringLiteral("borrowed")
                                     ? QStringLiteral("未归还") : QStringLiteral("已归还")};
        for (int column = 0; column < values.size(); ++column)
            borrowTable_->setItem(row, column, new QTableWidgetItem(values[column]));
    }
}

void MainWindow::borrowBook()
{
    QString recordId;
    QString error;
    const QString date = QDate::currentDate().toString(Qt::ISODate);
    if (!library_.borrowBook(borrowUser_->text().trimmed(), borrowBook_->text().trimmed(),
                             date, &recordId, &error)) {
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
        QMessageBox::warning(this, QStringLiteral("归还失败"), error);
        return;
    }
    refreshTable();
    refreshBorrowTable();
    showRecommendations();
    statusBar()->showMessage(QStringLiteral("归还成功：%1，记录编号 %2").arg(date, recordId));
}
