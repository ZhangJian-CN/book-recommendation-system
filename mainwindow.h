#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "librarystore.h"

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &dataDir, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    QWidget *buildBooksPage();
    QWidget *buildRecommendPage();
    QWidget *buildBorrowPage();
    void refreshSummary();
    void updateBookSelection();
    void updateBorrowSelection();
    void showDataDirectory();
    void refreshTable();
    void showBookRows(const QVector<int> &rows);
    void selectBook(const QString &id);
    void saveSelected();
    void addBook();
    void deleteSelected();
    void rateBook();
    void showRecommendations();
    void refreshBorrowTable();
    void borrowBook();
    void returnBook();

    Ui::MainWindow *ui;
    LibraryStore library_;
    QString dataDir_;
    QString selectedId_;
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
