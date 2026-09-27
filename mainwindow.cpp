#include "mainwindow.h"
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
#include <QTableWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &dataDir, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , booksPath_(dataDir + QStringLiteral("/books.csv"))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("图书检索与推荐系统 · 图书闭环"));
    auto *layout = new QVBoxLayout(ui->centralwidget);
    auto *searchRow = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("bookSearch"));
    search_->setPlaceholderText(QStringLiteral("输入图书编号，如 B001"));
    auto *searchButton = new QPushButton(QStringLiteral("按编号查找"), this);
    searchButton->setObjectName(QStringLiteral("searchButton"));
    searchRow->addWidget(search_);
    searchRow->addWidget(searchButton);
    layout->addLayout(searchRow);

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
    layout->addWidget(table_);

    auto *form = new QFormLayout;
    title_ = new QLineEdit(this);
    title_->setObjectName(QStringLiteral("bookTitle"));
    author_ = new QLineEdit(this);
    category_ = new QLineEdit(this);
    stock_ = new QSpinBox(this);
    stock_->setObjectName(QStringLiteral("bookStock"));
    stock_->setRange(0, 1000000);
    form->addRow(QStringLiteral("书名"), title_);
    form->addRow(QStringLiteral("作者"), author_);
    form->addRow(QStringLiteral("分类"), category_);
    form->addRow(QStringLiteral("库存"), stock_);
    layout->addLayout(form);
    auto *saveButton = new QPushButton(QStringLiteral("保存当前图书修改"), this);
    saveButton->setObjectName(QStringLiteral("saveButton"));
    layout->addWidget(saveButton);

    connect(searchButton, &QPushButton::clicked, this, [this] {
        selectBook(search_->text().trimmed());
    });
    connect(search_, &QLineEdit::returnPressed, this, [this] {
        selectBook(search_->text().trimmed());
    });
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        selectBook(table_->item(row, 0)->text());
    });
    connect(saveButton, &QPushButton::clicked, this, [this] { saveSelected(); });

    QString error;
    if (!books_.load(booksPath_, &error)) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        saveButton->setEnabled(false);
        searchButton->setEnabled(false);
    } else {
        refreshTable();
        statusBar()->showMessage(QStringLiteral("已加载 %1 本图书，文件：%2")
                                     .arg(books_.all().size()).arg(booksPath_));
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::refreshTable()
{
    table_->setRowCount(books_.all().size());
    for (int row = 0; row < books_.all().size(); ++row) {
        const Book &book = books_.all()[row];
        const QStringList values{book.id, book.title, book.author, book.category,
                                 QString::number(book.stock)};
        for (int column = 0; column < values.size(); ++column)
            table_->setItem(row, column, new QTableWidgetItem(values[column]));
    }
}

void MainWindow::selectBook(const QString &id)
{
    const Book *book = books_.findById(id);
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
    search_->setText(id);
    title_->setText(book->title);
    author_->setText(book->author);
    category_->setText(book->category);
    stock_->setValue(book->stock);
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (table_->item(row, 0)->text() == id) {
            table_->selectRow(row);
            break;
        }
    }
    statusBar()->showMessage(QStringLiteral("已选中 %1；修改下方字段后点击保存").arg(id));
}

void MainWindow::saveSelected()
{
    const Book *current = books_.findById(selectedId_);
    if (!current) {
        QMessageBox::information(this, QStringLiteral("尚未选择图书"),
                                 QStringLiteral("请先按编号查找或点击表格中的图书。"));
        return;
    }
    const Book before = *current;
    Book after{selectedId_, title_->text().trimmed(), author_->text().trimmed(),
               category_->text().trimmed(), stock_->value()};
    QString error;
    if (!books_.update(after, &error)) {
        QMessageBox::warning(this, QStringLiteral("修改失败"), error);
        return;
    }
    if (!books_.save(booksPath_, &error)) {
        books_.update(before, &error);
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return;
    }
    refreshTable();
    selectBook(selectedId_);
    statusBar()->showMessage(QStringLiteral("已保存 %1 到 books.csv").arg(selectedId_));
}
