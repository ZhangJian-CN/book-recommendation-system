#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "bookrepository.h"

class QLineEdit;
class QSpinBox;
class QTableWidget;

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
    void refreshTable();
    void selectBook(const QString &id);
    void saveSelected();

    Ui::MainWindow *ui;
    BookRepository books_;
    QString booksPath_;
    QString selectedId_;
    QTableWidget *table_ = nullptr;
    QLineEdit *search_ = nullptr;
    QLineEdit *title_ = nullptr;
    QLineEdit *author_ = nullptr;
    QLineEdit *category_ = nullptr;
    QSpinBox *stock_ = nullptr;
};
#endif // MAINWINDOW_H
