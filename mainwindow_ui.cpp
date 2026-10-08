#include "mainwindow.h"
#include "booksearch.h"
#include "ui_mainwindow.h"

#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QStringListModel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
class RowDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
    }
};

QLabel *label(const QString &text, const char *role, const char *name = "")
{
    auto *widget = new QLabel(text);
    widget->setProperty("role", role);
    widget->setObjectName(QString::fromLatin1(name));
    return widget;
}

QVBoxLayout *card(QHBoxLayout *layout, bool sidebar = false)
{
    auto *frame = new QFrame;
    frame->setProperty("role", "card");
    if (sidebar)
        frame->setFixedWidth(318);
    layout->addWidget(frame, sidebar ? 0 : 1);
    auto *body = new QVBoxLayout(frame);
    body->setContentsMargins(16, 16, 16, 16);
    body->setSpacing(12);
    return body;
}

QLineEdit *input(const char *name, const QString &hint = {})
{
    auto *widget = new QLineEdit;
    widget->setObjectName(QString::fromLatin1(name));
    widget->setPlaceholderText(hint);
    widget->setMinimumHeight(36);
    return widget;
}

QSpinBox *number(const char *name, int minimum, int maximum, int value)
{
    auto *widget = new QSpinBox;
    widget->setObjectName(QString::fromLatin1(name));
    widget->setRange(minimum, maximum);
    widget->setValue(value);
    widget->setMinimumHeight(36);
    return widget;
}

QPushButton *button(const QString &text, const char *name, const char *role = "secondary")
{
    auto *widget = new QPushButton(text);
    widget->setObjectName(QString::fromLatin1(name));
    widget->setProperty("role", role);
    widget->setMinimumHeight(36);
    return widget;
}

void field(QVBoxLayout *layout, const QString &caption, QWidget *widget)
{
    auto *group = new QVBoxLayout;
    group->setSpacing(4);
    auto *captionLabel = label(caption, "muted");
    captionLabel->setBuddy(widget);
    group->addWidget(captionLabel);
    group->addWidget(widget);
    layout->addLayout(group);
}

QVBoxLayout *scrollingForm(QVBoxLayout *layout)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumSize(0, 0);
    auto *body = new QWidget;
    body->setObjectName(QStringLiteral("formBody"));
    auto *form = new QVBoxLayout(body);
    form->setContentsMargins(0, 0, 4, 0);
    form->setSpacing(10);
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    return form;
}

void configureTable(QTableWidget *table, const QStringList &headers,
                    const QList<int> &widths, int stretchColumn, bool ranks = false)
{
    table->setColumnCount(headers.size());
    table->setItemDelegate(new RowDelegate(table));
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setMinimumSize(0, 0);
    table->verticalHeader()->setVisible(ranks);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setMinimumHeight(36);
    table->horizontalHeader()->setStretchLastSection(false);
    for (int column = 0; column < headers.size(); ++column) {
        table->horizontalHeader()->setSectionResizeMode(column,
            column == stretchColumn ? QHeaderView::Stretch : QHeaderView::Fixed);
        if (column != stretchColumn)
            table->setColumnWidth(column, widths[column]);
    }
}

void complete(QLineEdit *edit, QStringListModel *model)
{
    auto *completer = new QCompleter(model, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    edit->setCompleter(completer);
}
}

MainWindow::MainWindow(const QString &dataDir, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), dataDir_(dataDir)
{
    ui->setupUi(this);
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    setWindowTitle(QStringLiteral("图书检索与个性化推荐系统"));
    QFile stylesheet(QStringLiteral(":/ui/app.qss"));
    if (stylesheet.open(QIODevice::ReadOnly))
        setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    const QSize available = screen()->availableGeometry().size() - QSize(32, 64);
    setMinimumSize(QSize(1000, 680).boundedTo(available));
    resize(QSize(1200, 780).boundedTo(available));

    auto *root = new QVBoxLayout(ui->centralwidget);
    root->setContentsMargins(20, 16, 20, 12);
    root->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    heading->setSpacing(6);
    heading->addWidget(label(QStringLiteral("图书检索与个性化推荐系统"), "systemTitle"));
    heading->addWidget(label(QStringLiteral("当前数据 · %1").arg(QDir(dataDir_).dirName()),
                             "muted", "datasetLabel"));
    header->addLayout(heading, 1);
    const QStringList captions{QStringLiteral("图书"), QStringLiteral("用户"),
                               QStringLiteral("评分"), QStringLiteral("借阅记录")};
    const char *names[]{"bookCount", "userCount", "ratingCount", "borrowCount"};
    for (int i = 0; i < 4; ++i) {
        auto *metric = new QVBoxLayout;
        metric->setContentsMargins(14, 0, 14, 0);
        metric->setSpacing(2);
        counts_[i] = label(QStringLiteral("0"), "metricValue", names[i]);
        counts_[i]->setAlignment(Qt::AlignCenter);
        auto *caption = label(captions[i], "muted");
        caption->setAlignment(Qt::AlignCenter);
        metric->addWidget(counts_[i]);
        metric->addWidget(caption);
        header->addLayout(metric);
    }
    root->addLayout(header);
    auto *tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("mainTabs"));
    tabs->addTab(buildBooksPage(), QStringLiteral("图书管理与检索"));
    tabs->addTab(buildRecommendPage(), QStringLiteral("评分与推荐"));
    tabs->addTab(buildBorrowPage(), QStringLiteral("借阅与归还"));
    root->addWidget(tabs, 1);

    auto *directoryButton = button(QStringLiteral("数据目录…"), "dataDirectoryButton");
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(directoryButton);
    connect(directoryButton, &QPushButton::clicked, this, &MainWindow::showDataDirectory);

    QString error;
    if (!library_.load(dataDir_, &error)) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        tabs->setEnabled(false);
        statusBar()->showMessage(QStringLiteral("数据读取失败"));
        return;
    }
    QStringList userIds;
    for (const User &user : library_.users())
        userIds.append(user.id);
    auto *users = new QStringListModel(userIds, this);
    for (auto *edit : {ratingUser_, recommendUser_, borrowUser_}) {
        complete(edit, users);
        if (!library_.users().isEmpty())
            edit->setText(library_.hasUser(QStringLiteral("U0"))
                              ? QStringLiteral("U0") : library_.users().first().id);
    }
    bookIds_ = new QStringListModel(this);
    for (auto *edit : {search_, ratingBook_, borrowBook_})
        complete(edit, bookIds_);
    if (!library_.books().all().isEmpty()) {
        const QString first = library_.books().all().first().id;
        ratingBook_->setText(library_.books().findById(QStringLiteral("B003"))
                                 ? QStringLiteral("B003") : first);
        borrowBook_->setText(library_.books().findById(QStringLiteral("B004"))
                                 ? QStringLiteral("B004") : first);
    }
    refreshTable();
    refreshBorrowTable();
    if (!library_.users().isEmpty())
        showRecommendations();
    statusBar()->showMessage(QStringLiteral("数据加载完成：%1 本图书、%2 条评分")
                                 .arg(library_.books().all().size()).arg(library_.ratings().size()));
}

QWidget *MainWindow::buildBooksPage()
{
    auto *page = new QWidget;
    auto *columns = new QHBoxLayout(page);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);
    auto *results = card(columns);
    results->addWidget(label(QStringLiteral("图书列表"), "pageTitle"));
    bookSummary_ = label({}, "muted", "bookSummary");
    results->addWidget(bookSummary_);
    auto *searchRow = new QHBoxLayout;
    search_ = input("bookSearch", QStringLiteral("图书编号，如 B001"));
    search_->setMaximumWidth(240);
    auto *searchButton = button(QStringLiteral("按编号查找"), "searchButton");
    searchRow->addWidget(search_, 1);
    searchRow->addWidget(searchButton);
    searchRow->addStretch();
    results->addLayout(searchRow);
    auto *keywordRow = new QHBoxLayout;
    keyword_ = input("keywordSearch", QStringLiteral("书名或作者关键词"));
    auto *keywordButton = button(QStringLiteral("关键词检索"), "keywordButton");
    auto *showAllButton = button(QStringLiteral("显示全部"), "showAllButton");
    keywordRow->addWidget(keyword_, 1);
    keywordRow->addWidget(keywordButton);
    keywordRow->addWidget(showAllButton);
    results->addLayout(keywordRow);
    booksEmpty_ = label(QStringLiteral("暂无匹配图书，请调整关键词或显示全部。"),
                        "empty", "booksEmpty");
    booksEmpty_->setWordWrap(true);
    results->addWidget(booksEmpty_);
    table_ = new QTableWidget;
    table_->setObjectName(QStringLiteral("booksTable"));
    configureTable(table_, {QStringLiteral("编号"), QStringLiteral("书名"),
                            QStringLiteral("作者"), QStringLiteral("分类"), QStringLiteral("库存")},
                   {80, 0, 130, 90, 60}, 1);
    results->addWidget(table_, 1);

    auto *actions = card(columns, true);
    actions->addWidget(label(QStringLiteral("图书信息"), "sectionTitle"));
    bookSelection_ = label(QStringLiteral("尚未选择图书"), "selection", "bookSelection");
    bookSelection_->setWordWrap(true);
    actions->addWidget(bookSelection_);
    auto *form = scrollingForm(actions);
    newId_ = input("newBookId", QStringLiteral("新增时填写"));
    field(form, QStringLiteral("新书编号"), newId_);
    auto *hint = label(QStringLiteral("新增时填写，现有编号不修改。"), "muted");
    hint->setWordWrap(true);
    form->addWidget(hint);
    title_ = input("bookTitle");
    author_ = input("bookAuthor");
    category_ = input("bookCategory");
    stock_ = number("bookStock", 0, 1000000, 0);
    field(form, QStringLiteral("书名"), title_);
    field(form, QStringLiteral("作者"), author_);
    field(form, QStringLiteral("分类"), category_);
    field(form, QStringLiteral("库存"), stock_);
    form->addStretch();
    auto *buttons = new QHBoxLayout;
    saveButton_ = button(QStringLiteral("保存修改"), "saveButton", "primary");
    saveButton_->setEnabled(false);
    auto *addButton = button(QStringLiteral("新增图书"), "addBookButton");
    buttons->addWidget(saveButton_, 1);
    buttons->addWidget(addButton, 1);
    actions->addLayout(buttons);
    deleteButton_ = button(QStringLiteral("删除当前图书"), "deleteBookButton", "danger");
    deleteButton_->setEnabled(false);
    actions->addWidget(deleteButton_);

    connect(searchButton, &QPushButton::clicked, this, [this] { selectBook(search_->text().trimmed()); });
    connect(search_, &QLineEdit::returnPressed, searchButton, &QPushButton::click);
    connect(keywordButton, &QPushButton::clicked, this, [this] {
        showBookRows(BookSearch::keywordRows(library_.books().all(), keyword_->text()));
        statusBar()->showMessage(QStringLiteral("关键词检索得到 %1 本图书").arg(table_->rowCount()));
    });
    connect(keyword_, &QLineEdit::returnPressed, keywordButton, &QPushButton::click);
    connect(showAllButton, &QPushButton::clicked, this, [this] {
        refreshTable();
        statusBar()->showMessage(QStringLiteral("已显示全部图书"));
    });
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        selectBook(table_->item(row, 0)->text());
    });
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addBook);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveSelected);
    connect(deleteButton_, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    return page;
}

QWidget *MainWindow::buildRecommendPage()
{
    auto *page = new QWidget;
    auto *columns = new QHBoxLayout(page);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);
    auto *results = card(columns);
    results->addWidget(label(QStringLiteral("推荐结果"), "pageTitle"));
    recommendSummary_ = label(QStringLiteral("选择用户并生成推荐"), "muted", "recommendSummary");
    results->addWidget(recommendSummary_);
    auto *toolbar = new QHBoxLayout;
    recommendUser_ = input("recommendUser", QStringLiteral("用户编号"));
    recommendUser_->setMaximumWidth(180);
    topK_ = number("topK", 1, 1000, 2);
    topK_->setFixedWidth(80);
    auto *recommendButton = button(QStringLiteral("重新推荐"), "recommendButton", "primary");
    toolbar->addWidget(label(QStringLiteral("用户"), "muted"));
    toolbar->addWidget(recommendUser_, 1);
    toolbar->addWidget(label(QStringLiteral("Top K"), "muted"));
    toolbar->addWidget(topK_);
    toolbar->addWidget(recommendButton);
    toolbar->addStretch();
    results->addLayout(toolbar);
    recommendEmpty_ = label(QStringLiteral("暂无可推荐图书\n请确认是否已评完所有图书，或候选图书库存为零。"),
                            "empty", "recommendEmpty");
    recommendEmpty_->setWordWrap(true);
    recommendEmpty_->hide();
    results->addWidget(recommendEmpty_);
    recommendTable_ = new QTableWidget;
    recommendTable_->setObjectName(QStringLiteral("recommendTable"));
    configureTable(recommendTable_, {QStringLiteral("编号"), QStringLiteral("书名"),
                                    QStringLiteral("分数"), QStringLiteral("来源")},
                   {80, 0, 90, 175}, 1, true);
    results->addWidget(recommendTable_, 1);
    auto *actions = card(columns, true);
    actions->addWidget(label(QStringLiteral("录入或更新评分"), "sectionTitle"));
    auto *hint = label(QStringLiteral("为图书评 1～5 分；重复提交会更新已有评分。"), "muted");
    hint->setWordWrap(true);
    actions->addWidget(hint);
    auto *form = scrollingForm(actions);
    ratingUser_ = input("ratingUser", QStringLiteral("用户编号"));
    ratingBook_ = input("ratingBook", QStringLiteral("图书编号"));
    ratingScore_ = number("ratingScore", 1, 5, 4);
    field(form, QStringLiteral("评分用户"), ratingUser_);
    field(form, QStringLiteral("图书编号"), ratingBook_);
    field(form, QStringLiteral("评分"), ratingScore_);
    form->addStretch();
    auto *rateButton = button(QStringLiteral("保存评分"), "rateButton", "primary");
    actions->addWidget(rateButton);
    auto *refreshHint = label(QStringLiteral("已评分图书不会再次进入该用户的推荐候选。\n评分用户与推荐用户相同时，保存后自动刷新推荐。"), "muted");
    refreshHint->setWordWrap(true);
    actions->addWidget(refreshHint);
    connect(rateButton, &QPushButton::clicked, this, &MainWindow::rateBook);
    connect(recommendButton, &QPushButton::clicked, this, &MainWindow::showRecommendations);
    connect(recommendUser_, &QLineEdit::returnPressed, recommendButton, &QPushButton::click);
    return page;
}

QWidget *MainWindow::buildBorrowPage()
{
    auto *page = new QWidget;
    auto *columns = new QHBoxLayout(page);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);
    auto *results = card(columns);
    results->addWidget(label(QStringLiteral("借阅记录"), "pageTitle"));
    borrowSummary_ = label({}, "muted", "borrowSummary");
    results->addWidget(borrowSummary_);
    borrowEmpty_ = label(QStringLiteral("暂无借阅记录，可在右侧借出图书。"), "empty", "borrowEmpty");
    borrowEmpty_->setWordWrap(true);
    results->addWidget(borrowEmpty_);
    borrowTable_ = new QTableWidget;
    borrowTable_->setObjectName(QStringLiteral("borrowTable"));
    configureTable(borrowTable_, {QStringLiteral("记录编号"), QStringLiteral("用户"),
                                 QStringLiteral("图书"), QStringLiteral("借出日期"),
                                 QStringLiteral("归还日期"), QStringLiteral("状态")},
                   {90, 65, 70, 115, 115, 0}, 5);
    results->addWidget(borrowTable_, 1);
    auto *actions = card(columns, true);
    actions->addWidget(label(QStringLiteral("借出与归还"), "sectionTitle"));
    auto *form = scrollingForm(actions);
    borrowUser_ = input("borrowUser", QStringLiteral("用户编号"));
    borrowBook_ = input("borrowBook", QStringLiteral("图书编号"));
    field(form, QStringLiteral("借阅用户"), borrowUser_);
    field(form, QStringLiteral("图书编号"), borrowBook_);
    auto *borrowButton = button(QStringLiteral("借出图书"), "borrowButton", "primary");
    form->addWidget(borrowButton);
    form->addSpacing(6);
    form->addWidget(label(QStringLiteral("归还图书"), "sectionTitle"));
    returnRecord_ = input("returnRecord", QStringLiteral("输入编号，或点击左侧记录"));
    field(form, QStringLiteral("借阅编号"), returnRecord_);
    borrowSelection_ = label(QStringLiteral("尚未选择借阅记录"), "selection", "borrowSelection");
    borrowSelection_->setWordWrap(true);
    form->addWidget(borrowSelection_);
    auto *returnButton = button(QStringLiteral("归还图书"), "returnButton");
    form->addStretch();
    actions->addWidget(returnButton);
    auto *rule = label(QStringLiteral("借出与归还使用本机当天日期，允许当天归还。\n同一用户同一本书只能有一条未归还记录。"), "muted");
    rule->setWordWrap(true);
    actions->addWidget(rule);
    connect(borrowButton, &QPushButton::clicked, this, &MainWindow::borrowBook);
    connect(returnButton, &QPushButton::clicked, this, &MainWindow::returnBook);
    connect(borrowTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        returnRecord_->setText(borrowTable_->item(row, 0)->text());
    });
    connect(returnRecord_, &QLineEdit::textChanged, this, &MainWindow::updateBorrowSelection);
    return page;
}

void MainWindow::showDataDirectory()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("当前数据目录"));
    dialog.resize(620, 150);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->addWidget(label(QStringLiteral("当前使用以下数据目录，可选中并复制路径。"), "muted"));
    auto *path = input("dataDirectoryPath");
    path->setText(QDir::toNativeSeparators(QDir(dataDir_).absolutePath()));
    path->setReadOnly(true);
    path->selectAll();
    layout->addWidget(path);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}
