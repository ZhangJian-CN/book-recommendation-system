#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QDir sample(QStringLiteral(SAMPLE_DATA_DIR));
    for (const QString &name : {QStringLiteral("books.csv"), QStringLiteral("users.csv"),
                                QStringLiteral("ratings.csv"),
                                QStringLiteral("borrow_records.csv")}) {
        if (!QFile::copy(sample.filePath(name), dir.filePath(name)))
            return 2;
    }

    {
        MainWindow window(dir.path());
        if (qEnvironmentVariableIsSet("BOOK_SCREENSHOT_PATH")) {
            if (qEnvironmentVariableIsSet("BOOK_SCREENSHOT_RECOMMEND"))
                window.findChild<QTabWidget *>()->setCurrentIndex(1);
            window.show();
            app.processEvents();
            window.grab().save(qEnvironmentVariable("BOOK_SCREENSHOT_PATH"));
        }
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("booksTable"));
        auto *search = window.findChild<QLineEdit *>(QStringLiteral("bookSearch"));
        auto *searchButton = window.findChild<QPushButton *>(QStringLiteral("searchButton"));
        auto *title = window.findChild<QLineEdit *>(QStringLiteral("bookTitle"));
        auto *stock = window.findChild<QSpinBox *>(QStringLiteral("bookStock"));
        auto *saveButton = window.findChild<QPushButton *>(QStringLiteral("saveButton"));
        auto *newId = window.findChild<QLineEdit *>(QStringLiteral("newBookId"));
        auto *addButton = window.findChild<QPushButton *>(QStringLiteral("addBookButton"));
        auto *deleteButton = window.findChild<QPushButton *>(QStringLiteral("deleteBookButton"));
        auto *keyword = window.findChild<QLineEdit *>(QStringLiteral("keywordSearch"));
        auto *keywordButton = window.findChild<QPushButton *>(QStringLiteral("keywordButton"));
        auto *showAllButton = window.findChild<QPushButton *>(QStringLiteral("showAllButton"));
        auto *rateButton = window.findChild<QPushButton *>(QStringLiteral("rateButton"));
        auto *recommendTable = window.findChild<QTableWidget *>(QStringLiteral("recommendTable"));
        if (!table || !search || !searchButton || !title || !stock || !saveButton
            || !newId || !addButton || !deleteButton || !keyword || !keywordButton
            || !showAllButton || !rateButton || !recommendTable || table->rowCount() != 4
            || recommendTable->rowCount() != 2)
            return 3;

        search->setText(QStringLiteral("B001"));
        searchButton->click();
        if (stock->value() != 3)
            return 4;
        stock->setValue(4);
        saveButton->click();
        newId->setText(QStringLiteral("B005"));
        title->setText(QStringLiteral("界面测试新书"));
        stock->setValue(1);
        addButton->click();
        if (table->rowCount() != 5)
            return 5;
        keyword->setText(QStringLiteral("测试新书"));
        keywordButton->click();
        if (table->rowCount() != 1 || table->item(0, 0)->text() != QStringLiteral("B005"))
            return 6;
        showAllButton->click();
        QTimer::singleShot(0, [&window] {
            for (auto *box : window.findChildren<QMessageBox *>()) {
                if (box->isVisible() && box->button(QMessageBox::Yes))
                    box->button(QMessageBox::Yes)->click();
            }
        });
        deleteButton->click();
        if (table->rowCount() != 4)
            return 7;
        rateButton->click();
        if (recommendTable->rowCount() != 1
            || recommendTable->item(0, 0)->text() != QStringLiteral("B004"))
            return 8;
    }

    MainWindow restarted(dir.path());
    auto *table = restarted.findChild<QTableWidget *>(QStringLiteral("booksTable"));
    auto *recommendTable = restarted.findChild<QTableWidget *>(QStringLiteral("recommendTable"));
    return table && recommendTable && table->rowCount() == 4
                   && table->item(0, 4)->text() == QStringLiteral("4")
                   && recommendTable->rowCount() == 1
                   && recommendTable->item(0, 0)->text() == QStringLiteral("B004") ? 0 : 9;
}
