#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "librarystore.h"

// 【曹振江重点：窗口接口、成员与图书页】
// mainwindow_ui.cpp 创建控件并连接信号，mainwindow.cpp 处理点击后的业务与显示。
// 阅读时先找到控件指针，再跟着 connect 找到处理函数，最后追到 LibraryStore/算法。

class QLineEdit; // 前置声明单行文本框类型；此处只保存指针，完整控件定义在 .cpp 中包含。
class QSpinBox; // 整数输入控件，用于库存、评分和推荐条数。
class QTableWidget; // 表格控件，用于展示图书、推荐结果与借阅记录。
class QLabel; // 文字/图片标签，用于标题、数量和选中摘要。
class QPushButton; // 按钮控件，点击后通过信号连接到业务响应函数。
class QStringListModel; // 字符串列表模型，给图书编号补全提供当前可用编号。

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow; // Ui 命名空间中的生成类，辅助 setupUi；与下面自定义的 MainWindow 窗口类不同。
}
QT_END_NAMESPACE

// 主窗口：mainwindow_ui.cpp 负责布局和信号连接，mainwindow.cpp 负责响应操作与显示结果。
// LibraryStore 处理业务和保存，RecommendationService 处理推荐计算。
class MainWindow : public QMainWindow // class 定义主窗口；public 继承 Qt 的 QMainWindow，保留其公开窗口接口。
{
    Q_OBJECT // Qt 元对象宏，使本类具有 Qt 元对象信息，支持其信号槽等机制。

public: // 公有区：外部可创建和销毁窗口；具体页面操作封装在私有函数中。
    explicit MainWindow(const QString &dataDir, QWidget *parent = nullptr); // 构造主窗口：记录 CSV 目录、创建页面和连接事件，再加载数据。
    ~MainWindow() override; // 析构窗口并释放 ui 辅助对象；override 表示重写基类虚函数。

signals:
    // 独立评估入口记录分段耗时；不改变推荐结果或持久化行为。
    void recommendationMeasured(qint64 matrixNs, qint64 computeNs, qint64 fillNs);

protected: // 受保护区：本类及派生类可使用；Qt 通过事件机制触发尺寸变化处理。
    void resizeEvent(QResizeEvent *event) override; // 窗口尺寸改变时更新表格几何布局，不修改业务数据。

private: // 私有区：内部页面、响应函数和控件状态不允许外部随意修改，由窗口统一协调。
    // 三个业务页的创建入口；对应项目运行后的三个标签页。
    QWidget *buildBooksPage(); // 创建图书列表、检索框及编辑表单，连接增删改查按钮，返回完整图书页。
    QWidget *buildRecommendPage(); // 创建评分与推荐页，具体计算交给推荐服务。
    QWidget *buildBorrowPage(); // 创建借还页，借还规则由 LibraryStore 校验。
    void refreshSummary(); // 刷新顶部四项数量与编号补全列表，让新增/删除后的统计保持一致。
    void updateBookSelection(); // 按 selectedId_ 更新图书摘要、表格高亮、保存和删除按钮状态。
    void updateBorrowSelection(); // 按借阅记录编号刷新归还对象的摘要与高亮。
    void resetBookSelection(); // 清空编辑对象、图书表单和视觉选择；不把库存框的 0 写回文件。
    void resetBorrowSelection(); // 清空归还选择，保留借出输入及真实记录。
    void showDataDirectory(); // 展示当前实际 CSV 目录，便于确认演示使用的可写副本。
    void showPageSummary(); // 根据当前标签页恢复状态栏摘要，避免显示其他页的过时选择信息。
    void showOperationMessage(const QString &message, int timeout = 5000); // 在状态栏显示操作反馈；timeout 是保留毫秒数。
    void refreshTable(); // 生成全部图书的实体下标并刷新列表、选中状态及统计。
    void showBookRows(const QVector<int> &rows); // 将指定实体数组下标转换成表格行，用于完整列表或关键词结果。
    // 图书页的一条主线：编号/点击 -> selectBook -> 编辑 -> saveSelected/addBook/deleteSelected。
    // 真正的数据由 library_ 管理；这些成员函数只读取输入、调用业务、反馈结果。
    // 编号查找与点击表格均进入此函数，再把实体字段填入编辑表单。
    void selectBook(const QString &id, bool reload = false); // 同编号保留草稿，保存成功后显式重载。
    bool hasBookDraft() const;
    void syncEditedStock(const QString &bookId); // 只同步未手改的库存，保留其他字段草稿。
    void updateStockNotice();
    void saveSelected(); // 读取当前表单并调用 updateBook，成功后刷新，失败显示业务层错误。
    void addBook(); // 使用新编号和表单字段创建 Book，调用业务层新增并保存。
    void deleteSelected(); // 用户确认后调用业务层检查引用并删除，成功才清空旧图书显示。
    // 主链路：rateBook -> LibraryStore::rate；showRecommendations -> RecommendationService。
    void rateBook(); // 保存评分后按最近成功展示的模式刷新，不读取未提交的用户草稿。
    void showRecommendations(); // 校验用户并调用推荐服务，将已排序结果填入推荐表。
    void showPopularRecommendations(); // 无需用户编号，主动显示全局热门榜。
    void refreshRecommendations(); // 复用当前模式、用户和 Top K，刷新内容及冷启动状态。
    void refreshBorrowTable(); // 显示最新借阅记录及状态，刷新借还页摘要。
    void borrowBook(); // 将借出输入和本地当天日期交给业务层，成功后刷新相关页面。
    void returnBook(); // 将记录编号和当天日期交给业务层归还，成功后刷新库存与借阅表。

    Ui::MainWindow *ui; // Designer 生成的界面辅助对象；实际三页控件主要在 mainwindow_ui.cpp 创建。
    LibraryStore library_; // 本窗口使用的业务数据和操作入口，不让控件直接读写 CSV。
    QString dataDir_; // 当前实际读写目录，由 main.cpp 解析启动参数后传入。
    // 编辑对象按图书编号保存，不使用筛选后可能变化的表格显示行号。
    QString selectedId_; // 当前编辑对象的编号；筛选和显示行号变化后，仍能定位同一本书。
    Book editBaseline_; // 选择或成功保存时的基准；刷新不重置草稿基准。
    QString lastRecommendUser_; // 最近一次成功推荐的用户，用于恢复推荐页摘要。
    int lastRecommendCount_ = 0; // 最近一次成功推荐的实际条数，不一定达到请求的 Top K。
    enum class RecommendMode { None, Personalized, ColdStartPopular, GlobalPopular };
    RecommendMode recommendMode_ = RecommendMode::None;
    int lastRecommendTopK_ = 2; // 最近一次成功请求的 K；自动刷新不提交输入框中的草稿。
    // 控件指针在建页时赋值，响应函数通过它们读取输入或写入显示结果。
    QTableWidget *table_ = nullptr; // 图书结果表；保存的是显示行，不能直接当作实体数组下标。
    QLineEdit *search_ = nullptr; // 编号查找框，输入 id 后进入哈希查询。
    QLineEdit *keyword_ = nullptr; // 书名/作者关键词框，输入模式串后进入 KMP。
    QLineEdit *newId_ = nullptr; // 仅新增图书时使用的编号，与现有图书的 selectedId_ 分开。
    QLineEdit *title_ = nullptr; // 图书书名编辑框。
    QLineEdit *author_ = nullptr; // 图书作者编辑框。
    QLineEdit *category_ = nullptr; // 图书分类编辑框。
    QSpinBox *stock_ = nullptr; // 当前表单库存值；只有保存成功才改变持久化数据。
    QLabel *stockNotice_ = nullptr;
    // 其他两页：评分/推荐输入、借还输入及对应结果表。
    QLineEdit *ratingUser_ = nullptr;
    QLineEdit *ratingBook_ = nullptr;
    QSpinBox *ratingScore_ = nullptr;
    QLineEdit *recommendUser_ = nullptr;
    QSpinBox *topK_ = nullptr;
    QTableWidget *recommendTable_ = nullptr;
    QTableWidget *borrowTable_ = nullptr;
    QLineEdit *borrowUser_ = nullptr;
    QLineEdit *borrowBook_ = nullptr;
    QLineEdit *returnRecord_ = nullptr;
    QLabel *bookSummary_ = nullptr; // 当前图书列表“显示数 / 总数”摘要。
    QLabel *recommendSummary_ = nullptr;
    QLabel *recommendHint_ = nullptr; // 当前用户状态、排序规则和分数含义。
    QLabel *recommendRule_ = nullptr; // 当前模式的候选范围说明。
    QLabel *borrowSummary_ = nullptr;
    QLabel *bookSelection_ = nullptr; // 选中图书的编号、作者及筛选状态说明。
    QLabel *bookSelectionTitle_ = nullptr; // 选中图书的书名；未选择时显示引导文字。
    QLabel *borrowSelection_ = nullptr;
    QLabel *booksEmpty_ = nullptr; // 没有命中图书时显示的空结果提示。
    QLabel *recommendEmpty_ = nullptr;
    QLabel *borrowEmpty_ = nullptr;
    QLabel *counts_[4] = {}; // 顶部图书、用户、评分、借阅数量，顺序与 refreshSummary 一致。
    QPushButton *saveButton_ = nullptr; // 有有效图书编辑对象时才允许保存修改。
    QPushButton *deleteButton_ = nullptr; // 有有效选择时可发起删除，业务层仍要检查引用。
    QStringListModel *bookIds_ = nullptr; // 为图书编号输入提供补全，增删后由 refreshSummary 更新。
};
#endif // MAINWINDOW_H
