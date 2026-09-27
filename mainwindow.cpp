#include "mainwindow.h"
#include "booksearch.h"
#include "ui_mainwindow.h"

#include <QFormLayout>
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
    QString error;
    if (!library_.load(dataDir_, &error)) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        tabs->setEnabled(false);
    } else {
        refreshTable();
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
