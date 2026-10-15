#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "librarystore.h"

//声明窗口类

class QLineEdit;
class QSpinBox;
class QTableWidget;
class QLabel;
class QPushButton;
class QStringListModel;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 主窗口：mainwindow_ui.cpp 负责布局和信号连接，mainwindow.cpp 负责响应操作与显示结果。
// LibraryStore 处理业务和保存，RecommendationService 处理推荐计算。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &dataDir, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    // 三个业务页的创建入口；对应项目运行后的三个标签页。
    QWidget *buildBooksPage();
    QWidget *buildRecommendPage();
    QWidget *buildBorrowPage();
    void refreshSummary();
    void updateBookSelection();
    void updateBorrowSelection();
    void showDataDirectory();
    void showPageSummary();
    void showOperationMessage(const QString &message, int timeout = 5000);
    void refreshTable();
    void showBookRows(const QVector<int> &rows);
    // 编号查找与点击表格均进入此函数，再把实体字段填入编辑表单。
    void selectBook(const QString &id);
    void saveSelected();
    void addBook();
    void deleteSelected();
    // 主链路：rateBook -> LibraryStore::rate；showRecommendations -> RecommendationService。
    void rateBook();
    void showRecommendations();
    void refreshBorrowTable();
    void borrowBook();
    void returnBook();

    Ui::MainWindow *ui;
    LibraryStore library_;
    QString dataDir_;
    // 编辑对象按图书编号保存，不使用筛选后可能变化的表格显示行号。
    QString selectedId_;
    QString lastRecommendUser_;
    int lastRecommendCount_ = 0;
    // 控件指针在建页时赋值，响应函数通过它们读取输入或写入显示结果。
    QTableWidget *table_ = nullptr;
    QLineEdit *search_ = nullptr;
    QLineEdit *keyword_ = nullptr;
    QLineEdit *newId_ = nullptr;
    QLineEdit *title_ = nullptr;
    QLineEdit *author_ = nullptr;
    QLineEdit *category_ = nullptr;
    QSpinBox *stock_ = nullptr;
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
    QLabel *bookSummary_ = nullptr;
    QLabel *recommendSummary_ = nullptr;
    QLabel *borrowSummary_ = nullptr;
    QLabel *bookSelection_ = nullptr;
    QLabel *bookSelectionTitle_ = nullptr;
    QLabel *borrowSelection_ = nullptr;
    QLabel *booksEmpty_ = nullptr;
    QLabel *recommendEmpty_ = nullptr;
    QLabel *borrowEmpty_ = nullptr;
    QLabel *counts_[4] = {};
    QPushButton *saveButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QStringListModel *bookIds_ = nullptr;
};
#endif // MAINWINDOW_H
