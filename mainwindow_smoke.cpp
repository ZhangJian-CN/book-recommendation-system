#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    QFile file(dir.filePath(QStringLiteral("books.csv")));
    if (!file.open(QIODevice::WriteOnly))
        return 2;
    file.write("book_id,title,author,category,stock\n"
               "B001,Old Title,Author,Computer,3\n");
    file.close();

    {
        MainWindow window(dir.path());
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("booksTable"));
        auto *search = window.findChild<QLineEdit *>(QStringLiteral("bookSearch"));
        auto *searchButton = window.findChild<QPushButton *>(QStringLiteral("searchButton"));
        auto *title = window.findChild<QLineEdit *>(QStringLiteral("bookTitle"));
        auto *stock = window.findChild<QSpinBox *>(QStringLiteral("bookStock"));
        auto *saveButton = window.findChild<QPushButton *>(QStringLiteral("saveButton"));
        if (!table || !search || !searchButton || !title || !stock || !saveButton
            || table->rowCount() != 1 || table->item(0, 1)->text() != QStringLiteral("Old Title"))
            return 3;
        search->setText(QStringLiteral("B001"));
        searchButton->click();
        if (title->text() != QStringLiteral("Old Title"))
            return 4;
        title->setText(QStringLiteral("New Title"));
        stock->setValue(4);
        saveButton->click();
        if (table->item(0, 1)->text() != QStringLiteral("New Title"))
            return 5;
    }
    MainWindow restarted(dir.path());
    auto *table = restarted.findChild<QTableWidget *>(QStringLiteral("booksTable"));
    return table && table->rowCount() == 1
                   && table->item(0, 1)->text() == QStringLiteral("New Title")
                   && table->item(0, 4)->text() == QStringLiteral("4") ? 0 : 6;
}
