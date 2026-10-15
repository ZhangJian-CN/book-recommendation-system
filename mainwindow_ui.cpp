// Qt 主界面。mainwindow.ui 提供基础窗口外壳，三个业务页由本文件的 C++ 创建。创建主界面。
// 本文件 connect 信号到操作函数；具体业务响应及表格刷新在 mainwindow.cpp。
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
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QResizeEvent>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QStringListModel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
class ResultsTable : public QTableWidget
{
public:
    ResultsTable(int minimumRows, int maximumRows)
        : minimumRows_(minimumRows), maximumRows_(maximumRows)
    {
        connect(model(), &QAbstractItemModel::rowsInserted, this, [this] { clearClickState(); updateGeometry(); });
        connect(model(), &QAbstractItemModel::rowsRemoved, this, [this] { clearClickState(); updateGeometry(); });
        connect(model(), &QAbstractItemModel::modelReset, this, &ResultsTable::clearClickState);
        connect(model(), &QAbstractItemModel::dataChanged, this, &ResultsTable::clearClickState);
        connect(horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this] { updateGeometry(); });
    }
    ~ResultsTable() override
    {
        // QTableWidget 的析构会清空模型；此时派生类的鼠标状态已结束生命周期。
        disconnect(model(), nullptr, this, nullptr);
    }

    QSize sizeHint() const override
    {
        // 空结果已有独立提示，仅留一行视口承载键盘焦点线；不向模型补空行。
        const int capacity = rowCount() == 0 ? 1 : qBound(minimumRows_, rowCount(), maximumRows_);
        const int scroll = horizontalScrollBar()->maximum() > 0 ? horizontalScrollBar()->sizeHint().height() : 0;
        const int header = qMax(horizontalHeader()->minimumHeight(), horizontalHeader()->sizeHint().height());
        return {QTableWidget::sizeHint().width(), header
            + capacity * verticalHeader()->defaultSectionSize() + 2 * frameWidth() + scroll};
    }
    QSize minimumSizeHint() const override { return {0, 0}; }

    bool isLeftRowClick(int row) const
    {
        const auto *id = item(row, 0);
        return id && !pressedId_.isEmpty() && id->text() == pressedId_;
    }
    bool wasSelectedBeforePress() const { return selectedBeforePress_; }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        clearClickState();
        const QModelIndex hit = indexAt(event->position().toPoint());
        if (event->button() == Qt::LeftButton && hit.isValid()) {
            // 必须先取状态再交给 Qt；cellClicked 发出时，第一次点击也已被 Qt 选中。
            pressedId_ = item(hit.row(), 0)->text();
            selectedBeforePress_ = selectionModel()->isRowSelected(hit.row(), hit.parent());
        }
        QTableWidget::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QTableWidget::mouseReleaseEvent(event);
        clearClickState();
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        // 快速第二次按下仍按一次普通点击处理，释放时只走一次 cellClicked。
        mousePressEvent(event);
    }
    void focusInEvent(QFocusEvent *event) override
    {
        QTableWidget::focusInEvent(event);
        viewport()->update();
    }
    void focusOutEvent(QFocusEvent *event) override
    {
        QTableWidget::focusOutEvent(event);
        viewport()->update();
    }
    void paintEvent(QPaintEvent *event) override
    {
        QTableWidget::paintEvent(event);
        if (hasFocus() && rowCount() == 0) {
            QPainter painter(viewport());
            painter.fillRect(QRect(0, 0, qMin(48, viewport()->width()), 2), QColor("#245C4D"));
        }
    }

private:
    void clearClickState()
    {
        pressedId_.clear();
        selectedBeforePress_ = false;
    }

    QString pressedId_;
    bool selectedBeforePress_ = false;
    int minimumRows_;
    int maximumRows_;
};

class FormScrollArea : public QScrollArea
{
public:
    QSize sizeHint() const override
    {
        return widget() ? widget()->sizeHint() + QSize(0, 2 * frameWidth()) : QScrollArea::sizeHint();
    }
};

class ElidedLabel : public QLabel
{
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override { return {0, fontMetrics().height()}; }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setPen(palette().color(foregroundRole()));
        painter.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};

class RowDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const auto *table = qobject_cast<QTableWidget *>(parent());
        const bool category = table->objectName() == QStringLiteral("booksTable") && index.column() == 3;
        const bool badge = category || (table->objectName() == QStringLiteral("recommendTable") && index.column() == 3)
                            || (table->objectName() == QStringLiteral("borrowTable") && index.column() == 5);
        if (!badge) {
            QStyledItemDelegate::paint(painter, option, index);
        } else {
            QStyleOptionViewItem item(option);
            initStyleOption(&item, index);
            const QString text = item.text;
            item.text.clear();
            item.backgroundBrush = Qt::NoBrush;
            table->style()->drawControl(QStyle::CE_ItemViewItem, &item, painter, table);
            const bool amber = text == QStringLiteral("未归还") || text.startsWith(QStringLiteral("热门回退"));
            QFont font = item.font;
            font.setPixelSize(12);
            const QFontMetrics metrics(font);
            const int width = qMin(metrics.horizontalAdvance(text) + 16, option.rect.width() - 24);
            QRect pill(option.rect.left() + 12, option.rect.center().y() - 12, width, 24);
            if (category || table->objectName() == QStringLiteral("borrowTable"))
                pill.moveCenter(option.rect.center());
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(category ? "#F0F3EE" : amber ? "#FBF0DC" : "#EAF2ED"));
            painter->drawRoundedRect(pill, 5, 5);
            painter->setFont(font);
            painter->setPen(QColor(category ? "#647068" : amber ? "#8A5418" : "#245C4D"));
            painter->drawText(pill.adjusted(8, 0, -8, 0), Qt::AlignCenter,
                              metrics.elidedText(text, Qt::ElideRight, width - 16));
            painter->restore();
        }
        painter->save();
        painter->setPen(QColor("#EDF0EA"));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        if (index.column() == 0 && (option.state & QStyle::State_Selected))
            painter->fillRect(QRect(option.rect.left(), option.rect.top(), 2, option.rect.height()),
                              QColor(table->hasFocus() ? "#245C4D" : "#91B19E"));
        painter->restore();
    }

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
    if (!sidebar) {
        frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        layout->setAlignment(frame, Qt::AlignTop);
    }
    auto *body = new QVBoxLayout(frame);
    body->setContentsMargins(18, 18, 18, 18);
    body->setSpacing(12);
    return body;
}

QLineEdit *input(const char *name, const QString &hint = {})
{
    // name 设置为 objectName，便于测试按名称找到控件；hint 是空输入时的提示文字。
    auto *widget = new QLineEdit;
    widget->setObjectName(QString::fromLatin1(name));
    widget->setPlaceholderText(hint);
    widget->setMinimumHeight(36);
    if (QString::fromLatin1(name) == QStringLiteral("bookSearch")
        || QString::fromLatin1(name) == QStringLiteral("keywordSearch"))
        widget->addAction(QIcon(QStringLiteral(":/ui/icons/search.png")), QLineEdit::LeadingPosition);
    return widget;
}

QSpinBox *number(const char *name, int minimum, int maximum, int value)
{
    // 数字框限定范围并设置初值；业务层仍再次校验，不能只依赖界面输入限制。
    auto *widget = new QSpinBox;
    widget->setObjectName(QString::fromLatin1(name));
    widget->setRange(minimum, maximum);
    widget->setButtonSymbols(QAbstractSpinBox::PlusMinus);
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
    auto *scroll = new FormScrollArea;
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
    // 所有业务表格统一为整行单选、禁止直接编辑；表单保存才能进入业务校验和持久化。
    table->setColumnCount(headers.size());
    table->setItemDelegate(new RowDelegate(table));
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setTabKeyNavigation(false);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setMinimumSize(0, 0);
    table->setFrameShape(QFrame::NoFrame);
    table->verticalHeader()->setVisible(ranks);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setMinimumHeight(36);
    table->horizontalHeader()->setStretchLastSection(false);
    for (int column = 0; column < headers.size(); ++column) {
        const bool centered = table->objectName() == QStringLiteral("borrowTable")
            || column == 0 || (table->objectName() == QStringLiteral("booksTable") && column >= 3)
            || (ranks && column == 2);
        table->horizontalHeaderItem(column)->setTextAlignment(centered ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignVCenter);
    }
    for (int column = 0; column < headers.size(); ++column) {
        table->horizontalHeader()->setSectionResizeMode(column,
            column == stretchColumn ? QHeaderView::Stretch : QHeaderView::Fixed);
        if (column != stretchColumn)
            table->setColumnWidth(column, widths[column]);
    }
}

void complete(QLineEdit *edit, QStringListModel *model)
{
    // 输入补全由实际编号列表提供建议，只帮助选编号，不替代业务层存在性检查。
    auto *completer = new QCompleter(model, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    edit->setCompleter(completer);
    completer->popup()->setObjectName(QStringLiteral("idCompletionPopup"));
    completer->popup()->setFont(edit->font());
    completer->popup()->setStyleSheet(edit->window()->styleSheet());
}

QLabel *elided(const QString &text, const char *role, const char *name)
{
    auto *widget = new ElidedLabel(text);
    widget->setObjectName(QString::fromLatin1(name));
    widget->setProperty("role", role);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    return widget;
}

void pageHeading(QVBoxLayout *layout, const QString &text, QLabel *summary)
{
    auto *row = new QHBoxLayout;
    row->addWidget(label(text, "pageTitle"));
    row->addWidget(summary);
    row->addStretch();
    layout->addLayout(row);
}

QWidget *separator()
{
    auto *line = new QFrame;
    line->setProperty("role", "separator");
    line->setFixedHeight(1);
    return line;
}
}

MainWindow::MainWindow(const QString &dataDir, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), dataDir_(dataDir)
{
    // 初始化窗口外壳和样式，再创建图书、评分推荐、借还三个标签页。
    // ui->setupUi 建立 .ui 中的基础窗口；下面的布局和控件构成实际三页业务界面。
    ui->setupUi(this);
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    setWindowTitle(QStringLiteral("图书检索与个性化推荐系统"));
    // :/ui/... 是编译进程序的 Qt 资源路径，与磁盘上的 CSV 数据目录不同。
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
    auto *brand = new QLabel;
    brand->setPixmap(QIcon(QStringLiteral(":/ui/icons/brand.png")).pixmap(QSize(36, 36), devicePixelRatioF()));
    brand->setFixedSize(36, 36);
    header->addWidget(brand);
    header->setSpacing(12);
    auto *heading = new QVBoxLayout;
    heading->setSpacing(6);
    heading->addWidget(label(QStringLiteral("图书检索与个性化推荐系统"), "systemTitle"));
    auto *directory = elided(QStringLiteral("当前数据 · %1").arg(QDir(dataDir_).dirName()), "muted", "datasetLabel");
    directory->setToolTip(QDir::toNativeSeparators(QDir(dataDir_).absolutePath()));
    heading->addWidget(directory);
    header->addLayout(heading, 1);
    const QStringList captions{QStringLiteral("图书"), QStringLiteral("用户"),
                               QStringLiteral("评分"), QStringLiteral("借阅记录")};
    const char *names[]{"bookCount", "userCount", "ratingCount", "borrowCount"};
    auto *metrics = new QFrame;
    metrics->setObjectName(QStringLiteral("metrics"));
    auto *metricRow = new QHBoxLayout(metrics);
    metricRow->setContentsMargins(10, 8, 10, 8);
    metricRow->setSpacing(12);
    for (int i = 0; i < 4; ++i) {
        if (i) {
            auto *line = new QFrame;
            line->setProperty("role", "metricSeparator");
            line->setFixedSize(1, 22);
            metricRow->addWidget(line);
        }
        auto *metric = new QHBoxLayout;
        metric->setSpacing(6);
        counts_[i] = label(QStringLiteral("0"), "metricValue", names[i]);
        counts_[i]->setAlignment(Qt::AlignCenter);
        auto *caption = label(captions[i], "muted");
        caption->setAlignment(Qt::AlignCenter);
        metric->addWidget(counts_[i]);
        metric->addWidget(caption);
        metricRow->addLayout(metric);
    }
    header->addWidget(metrics);
    root->addLayout(header);
    // QTabWidget 组织三页；build...Page 返回 QWidget，每页只负责自己的交互入口。
    auto *tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("mainTabs"));
    tabs->addTab(buildBooksPage(), QStringLiteral("图书管理与检索"));
    tabs->addTab(buildRecommendPage(), QStringLiteral("评分与推荐"));
    tabs->addTab(buildBorrowPage(), QStringLiteral("借阅与归还"));
    tabs->setIconSize(QSize(16, 16));
    tabs->setTabIcon(0, QIcon(QStringLiteral(":/ui/icons/book.png")));
    tabs->setTabIcon(1, QIcon(QStringLiteral(":/ui/icons/list.png")));
    tabs->setTabIcon(2, QIcon(QStringLiteral(":/ui/icons/swap.png")));
    root->addWidget(tabs, 1);

    auto *directoryButton = button(QStringLiteral("数据目录…"), "dataDirectoryButton");
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(directoryButton);
    connect(directoryButton, &QPushButton::clicked, this, &MainWindow::showDataDirectory);
    connect(tabs, &QTabWidget::currentChanged, this, [this] { showPageSummary(); });
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString &message) {
        if (message.isEmpty()) {
            showPageSummary();
            return;
        }
        auto *bar = statusBar();
        bar->setProperty("error", message.contains(QStringLiteral("失败"))
                                     || message.contains(QStringLiteral("未找到"))
                                     || message.contains(QStringLiteral("不存在")));
        bar->style()->unpolish(bar);
        bar->style()->polish(bar);
    });

    // 所有页面建立后统一加载四份 CSV；失败时显示原因并禁用业务页。
    // 调用链：MainWindow 构造 -> LibraryStore::load -> BookRepository::load / Csv::read。
    QString error;
    if (!library_.load(dataDir_, &error)) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        tabs->setEnabled(false);
        statusBar()->showMessage(QStringLiteral("数据读取失败"));
        return;
    }
    // 编号补全和默认值从实际数据产生；优先使用小样例 U0，否则选择首个有效用户。
    // Goodbooks 的用户是 U00～U39，不能把小样例 U0 硬套到另一套数据中。
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
    // 把加载成功的内存数据展示出来；首次进入时即生成当前用户的推荐。
    // 页面此时已建立，控件指针均已赋值，可安全填入图书、借阅和推荐数据。
    refreshTable();
    refreshBorrowTable();
    if (!library_.users().isEmpty())
        showRecommendations();
    showOperationMessage(QStringLiteral("数据加载完成：%1 本图书、%2 条评分")
                                 .arg(library_.books().all().size()).arg(library_.ratings().size()));
}

QWidget *MainWindow::buildBooksPage()
{
    // 图书页：左侧列表与检索，右侧表单；表格禁止直接编辑，修改通过业务层校验保存。
    auto *page = new QWidget;
    auto *columns = new QHBoxLayout(page);
    columns->setContentsMargins(0, 16, 0, 0);
    columns->setSpacing(20);
    auto *results = card(columns);
    bookSummary_ = label({}, "count", "bookSummary");
    pageHeading(results, QStringLiteral("图书列表"), bookSummary_);
    results->addWidget(label(QStringLiteral("点击记录选择，再次点击取消并清空编辑区"), "muted"));
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
    booksEmpty_ = label(QStringLiteral("<img src=\":/ui/icons/search.png\" width=\"16\" height=\"16\">　暂无匹配图书，请调整关键词或显示全部。"),
                        "empty", "booksEmpty");
    booksEmpty_->setWordWrap(true);
    results->addWidget(booksEmpty_);
    auto *bookTable = new ResultsTable(5, 9);
    table_ = bookTable;
    table_->setObjectName(QStringLiteral("booksTable"));
    configureTable(table_, {QStringLiteral("编号"), QStringLiteral("书名"),
                            QStringLiteral("作者"), QStringLiteral("分类"), QStringLiteral("库存")},
                   {80, 0, 130, 90, 60}, 1);
    results->addWidget(table_, 1);

    auto *actions = card(columns, true);
    actions->addWidget(label(QStringLiteral("图书信息"), "sectionTitle"));
    auto *selection = new QFrame;
    selection->setObjectName(QStringLiteral("bookSelectionCard"));
    selection->setProperty("selected", false);
    auto *selectionLayout = new QHBoxLayout(selection);
    selectionLayout->setContentsMargins(10, 10, 10, 10);
    auto *bookIcon = new QLabel;
    bookIcon->setPixmap(QIcon(QStringLiteral(":/ui/icons/book.png")).pixmap(QSize(26, 26), devicePixelRatioF()));
    selectionLayout->addWidget(bookIcon);
    auto *selectionText = new QVBoxLayout;
    selectionText->setSpacing(3);
    bookSelectionTitle_ = elided(QStringLiteral("尚未选择图书"), "selectedTitle", "bookSelectionTitle");
    selectionText->addWidget(bookSelectionTitle_);
    bookSelection_ = label(QStringLiteral("按编号查找或点击列表"), "muted", "bookSelection");
    bookSelection_->setTextFormat(Qt::PlainText);
    bookSelection_->setWordWrap(true);
    selectionText->addWidget(bookSelection_);
    selectionLayout->addLayout(selectionText, 1);
    actions->addWidget(selection);
    auto *form = scrollingForm(actions);
    newId_ = input("newBookId", QStringLiteral("如 B005"));
    field(form, QStringLiteral("新书编号（仅新增时填写）"), newId_);
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
    actions->addSpacing(4);
    actions->addWidget(separator());
    deleteButton_ = button(QStringLiteral("删除当前图书"), "deleteBookButton", "danger");
    deleteButton_->setEnabled(false);
    actions->addWidget(deleteButton_);

    // 信号与槽：按钮 clicked/输入框回车触发操作；编号查找最终调用 BookRepository 的哈希索引。
    // connect 指定“哪个控件的哪个信号，触发哪个接收对象的处理函数”。
    // [this] 的 lambda 捕获当前窗口，可读取 search_ 并调用 selectBook。
    connect(searchButton, &QPushButton::clicked, this, [this] { selectBook(search_->text().trimmed()); });
    connect(search_, &QLineEdit::returnPressed, searchButton, &QPushButton::click);
    connect(keywordButton, &QPushButton::clicked, this, [this] {
        // 关键词检索调用 KMP，返回图书数组下标；showBookRows 负责转换为表格显示行。
        // 本操作只筛选显示内容，不改图书实体，也不写入 CSV。
        showBookRows(BookSearch::keywordRows(library_.books().all(), keyword_->text()));
        showOperationMessage(QStringLiteral("关键词检索得到 %1 本图书").arg(table_->rowCount()));
    });
    connect(keyword_, &QLineEdit::returnPressed, keywordButton, &QPushButton::click);
    connect(showAllButton, &QPushButton::clicked, this, [this] {
        refreshTable();
        showOperationMessage(QStringLiteral("已显示全部图书"));
    });
    connect(table_, &QTableWidget::cellClicked, this, [this, bookTable](int row, int) {
        if (!bookTable->isLeftRowClick(row))
            return;
        if (bookTable->wasSelectedBeforePress())
            resetBookSelection();
        else
            selectBook(table_->item(row, 0)->text());
    });
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addBook);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveSelected);
    connect(deleteButton_, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    return page;
}

QWidget *MainWindow::buildRecommendPage()
{
    // 推荐页：左侧用户、Top K 与结果，右侧录入 1～5 分；计算和保存均由独立模块处理。
    auto *page = new QWidget;
    auto *columns = new QHBoxLayout(page);
    columns->setContentsMargins(0, 16, 0, 0);
    columns->setSpacing(20);
    auto *results = card(columns);
    recommendSummary_ = label({}, "count", "recommendSummary");
    pageHeading(results, QStringLiteral("推荐结果"), recommendSummary_);
    results->addWidget(label(QStringLiteral("按预测分降序展示 · 保留四位小数"), "muted"));
    auto *toolbar = new QHBoxLayout;
    recommendUser_ = input("recommendUser", QStringLiteral("用户编号"));
    recommendUser_->setMaximumWidth(150);
    topK_ = number("topK", 1, 1000, 2);
    topK_->setFixedWidth(136);
    auto *recommendButton = button(QStringLiteral("重新推荐"), "recommendButton", "primary");
    toolbar->addWidget(label(QStringLiteral("用户"), "muted"));
    toolbar->addWidget(recommendUser_, 1);
    toolbar->addWidget(label(QStringLiteral("Top K"), "muted"));
    toolbar->addWidget(topK_);
    toolbar->addWidget(recommendButton);
    toolbar->addStretch();
    results->addLayout(toolbar);
    recommendEmpty_ = label(QStringLiteral("<img src=\":/ui/icons/list.png\" width=\"16\" height=\"16\">　暂无可推荐图书<br>请确认是否已评完所有图书，或候选图书库存为零。"),
                            "empty", "recommendEmpty");
    recommendEmpty_->setWordWrap(true);
    recommendEmpty_->hide();
    results->addWidget(recommendEmpty_);
    auto *recommendTable = new ResultsTable(3, 8);
    recommendTable_ = recommendTable;
    recommendTable_->setObjectName(QStringLiteral("recommendTable"));
    configureTable(recommendTable_, {QStringLiteral("编号"), QStringLiteral("书名"),
                                    QStringLiteral("分数"), QStringLiteral("来源")},
                   {80, 0, 90, 175}, 1, true);
    results->addWidget(recommendTable_, 1);
    connect(recommendTable_, &QTableWidget::cellClicked, this, [this, recommendTable](int row, int) {
        if (!recommendTable->isLeftRowClick(row))
            return;
        if (recommendTable->wasSelectedBeforePress())
            recommendTable_->selectionModel()->clear();
        showPageSummary();
    });
    auto *sidebar = new QVBoxLayout;
    sidebar->setSpacing(16);
    columns->addLayout(sidebar);
    auto *cardRow = new QHBoxLayout;
    auto *actions = card(cardRow, true);
    actions->parentWidget()->setObjectName(QStringLiteral("ratingCard"));
    sidebar->addLayout(cardRow);
    actions->addWidget(label(QStringLiteral("录入或更新评分"), "sectionTitle"));
    auto *hint = label(QStringLiteral("为指定用户记录 1～5 分的评分"), "muted");
    hint->setWordWrap(true);
    actions->addWidget(hint);
    auto *form = scrollingForm(actions);
    ratingUser_ = input("ratingUser", QStringLiteral("用户编号"));
    ratingBook_ = input("ratingBook", QStringLiteral("图书编号"));
    ratingScore_ = number("ratingScore", 1, 5, 4);
    field(form, QStringLiteral("评分用户"), ratingUser_);
    field(form, QStringLiteral("图书编号"), ratingBook_);
    field(form, QStringLiteral("评分"), ratingScore_);
    auto *rateButton = button(QStringLiteral("保存评分"), "rateButton", "primary");
    actions->addSpacing(4);
    actions->addWidget(rateButton);
    auto *updateHint = label(QStringLiteral("重复提交会更新这位用户已有的评分。"), "muted");
    updateHint->setWordWrap(true);
    actions->addWidget(updateHint);
    auto *guidance = new QWidget;
    guidance->setObjectName(QStringLiteral("recommendGuidance"));
    auto *guidanceLayout = new QVBoxLayout(guidance);
    guidanceLayout->setContentsMargins(18, 0, 18, 0);
    guidanceLayout->setSpacing(6);
    guidanceLayout->addWidget(label(QStringLiteral("推荐说明"), "guidanceTitle"));
    auto *candidateHint = label(QStringLiteral("<p style='line-height:120%; margin-top:0; margin-bottom:0'>已评分图书会退出该用户的推荐候选。</p>"), "guidance");
    candidateHint->setTextFormat(Qt::RichText);
    candidateHint->setWordWrap(true);
    guidanceLayout->addWidget(candidateHint);
    guidanceLayout->addSpacing(6);
    auto *refreshHint = label(QStringLiteral("<p style='line-height:120%; margin-top:0; margin-bottom:0'>评分用户与推荐用户相同时，<br>保存后自动刷新推荐结果。</p>"), "guidance");
    refreshHint->setTextFormat(Qt::RichText);
    refreshHint->setWordWrap(true);
    guidanceLayout->addWidget(refreshHint);
    sidebar->addWidget(guidance);
    sidebar->addStretch(1);
    // 保存评分 -> rateBook；重新推荐 -> showRecommendations -> RecommendationService。
    // rateBook 负责保存评分并按条件刷新；showRecommendations 重建矩阵后重新计算结果。
    connect(rateButton, &QPushButton::clicked, this, &MainWindow::rateBook);
    connect(recommendButton, &QPushButton::clicked, this, &MainWindow::showRecommendations);
    connect(recommendUser_, &QLineEdit::returnPressed, recommendButton, &QPushButton::click);
    return page;
}

QWidget *MainWindow::buildBorrowPage()
{
    // 借还页连接业务层已有接口；日期由操作函数取本机当天日期，表格展示保存后的记录。
    auto *page = new QWidget;
    auto *columns = new QHBoxLayout(page);
    columns->setContentsMargins(0, 16, 0, 0);
    columns->setSpacing(20);
    auto *results = card(columns);
    borrowSummary_ = label({}, "count", "borrowSummary");
    pageHeading(results, QStringLiteral("借阅记录"), borrowSummary_);
    results->addWidget(label(QStringLiteral("点击记录填写归还编号"), "muted"));
    borrowEmpty_ = label(QStringLiteral("<img src=\":/ui/icons/swap.png\" width=\"16\" height=\"16\">　暂无借阅记录，可在右侧借出图书。"), "empty", "borrowEmpty");
    borrowEmpty_->setWordWrap(true);
    results->addWidget(borrowEmpty_);
    auto *borrowTable = new ResultsTable(4, 9);
    borrowTable_ = borrowTable;
    borrowTable_->setObjectName(QStringLiteral("borrowTable"));
    configureTable(borrowTable_, {QStringLiteral("记录编号"), QStringLiteral("用户"),
                                 QStringLiteral("图书"), QStringLiteral("借出日期"),
                                 QStringLiteral("归还日期"), QStringLiteral("状态")},
                   {90, 65, 70, 115, 115, 0}, 5);
    results->addWidget(borrowTable_, 1);
    auto *actions = card(columns, true);
    actions->parentWidget()->setObjectName(QStringLiteral("borrowCard"));
    actions->parentWidget()->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    columns->setAlignment(actions->parentWidget(), Qt::AlignTop);
    actions->addWidget(label(QStringLiteral("<img src=\":/ui/icons/swap.png\" width=\"16\" height=\"16\">　借出图书"), "sectionTitle"));
    auto *form = scrollingForm(actions);
    borrowUser_ = input("borrowUser", QStringLiteral("用户编号"));
    borrowBook_ = input("borrowBook", QStringLiteral("图书编号"));
    field(form, QStringLiteral("借阅用户"), borrowUser_);
    field(form, QStringLiteral("图书编号"), borrowBook_);
    auto *borrowButton = button(QStringLiteral("借出图书"), "borrowButton", "primary");
    form->addWidget(borrowButton);
    auto *dateRule = label(QStringLiteral("借出和归还使用今天的日期。"), "muted");
    form->addWidget(dateRule);
    form->addSpacing(4);
    form->addWidget(separator());
    form->addSpacing(4);
    form->addWidget(label(QStringLiteral("<img src=\":/ui/icons/swap.png\" width=\"16\" height=\"16\">　归还图书"), "sectionTitle"));
    returnRecord_ = input("returnRecord", QStringLiteral("输入编号，或点击左侧记录"));
    field(form, QStringLiteral("借阅编号"), returnRecord_);
    borrowSelection_ = label(QStringLiteral("尚未选择借阅记录"), "selection", "borrowSelection");
    borrowSelection_->setWordWrap(true);
    form->addWidget(borrowSelection_);
    auto *returnButton = button(QStringLiteral("归还图书"), "returnButton");
    form->addSpacing(2);
    form->addWidget(returnButton);
    auto *rule = label(QStringLiteral("允许当天归还；同一本书归还后可再次借阅。"), "muted");
    rule->setWordWrap(true);
    form->addWidget(rule);
    connect(borrowButton, &QPushButton::clicked, this, &MainWindow::borrowBook);
    connect(returnButton, &QPushButton::clicked, this, &MainWindow::returnBook);
    connect(borrowTable_, &QTableWidget::cellClicked, this, [this, borrowTable](int row, int) {
        if (!borrowTable->isLeftRowClick(row))
            return;
        if (borrowTable->wasSelectedBeforePress())
            resetBorrowSelection();
        else
            returnRecord_->setText(borrowTable_->item(row, 0)->text());
    });
    connect(returnRecord_, &QLineEdit::textChanged, this, &MainWindow::updateBorrowSelection);
    connect(returnRecord_, &QLineEdit::textChanged, this, [this] {
        if (findChild<QTabWidget *>(QStringLiteral("mainTabs"))->currentIndex() == 2)
            showPageSummary();
    });
    return page;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    for (auto *table : {table_, recommendTable_, borrowTable_})
        if (table) {
            table->verticalHeader()->setDefaultSectionSize(height() < 720 ? 40 : 44);
            table->updateGeometry();
        }
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
