#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>

#define CHECK(condition) do { if (!(condition)) { \
    qCritical() << "Line" << __LINE__ << ":" << #condition; return false; } } while (false)

namespace {
template <typename T> T *get(MainWindow &window, const char *name)
{
    auto *widget = window.findChild<T *>(QString::fromLatin1(name));
    if (!widget)
        qFatal("Missing widget: %s", name);
    return widget;
}

const QStringList csvNames{"books.csv", "users.csv", "ratings.csv", "borrow_records.csv"};

bool copyData(const QString &source, const QTemporaryDir &target)
{
    CHECK(target.isValid());
    for (const auto &name : csvNames)
        CHECK(QFile::copy(QDir(source).filePath(name), target.filePath(name)));
    return true;
}

QList<QByteArray> csvContents(const QTemporaryDir &directory)
{
    QList<QByteArray> result;
    for (const auto &name : csvNames) {
        QFile file(directory.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            qFatal("Cannot read test CSV");
        result.append(file.readAll());
    }
    return result;
}

void settle()
{
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}

void mouse(QWidget *widget, const QPoint &position, QEvent::Type type, Qt::MouseButton button)
{
    QMouseEvent event(type, position, widget->mapToGlobal(position), button,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : button, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

void click(QWidget *widget, const QPoint &position, Qt::MouseButton button = Qt::LeftButton,
           QEvent::Type press = QEvent::MouseButtonPress)
{
    mouse(widget, position, press, button);
    mouse(widget, position, QEvent::MouseButtonRelease, button);
    settle();
}

void clickCell(QTableWidget *table, int row, int column,
               QEvent::Type press = QEvent::MouseButtonPress)
{
    table->scrollToItem(table->item(row, column));
    settle();
    click(table->viewport(), table->visualItemRect(table->item(row, column)).center(), Qt::LeftButton, press);
}

bool capture(MainWindow &window, const QString &name)
{
    const QString directory = qEnvironmentVariable("TOGGLE_SCREENSHOT_DIR");
    if (directory.isEmpty())
        return true;
    CHECK(QDir().mkpath(directory));
    settle();
    CHECK(window.grab().save(QDir(directory).filePath(name + ".png")));
    return true;
}

bool hasNoSelection(QTableWidget *table)
{
    return !table->selectionModel()->hasSelection() && !table->currentIndex().isValid();
}

bool bookWasReset(MainWindow &window)
{
    CHECK(hasNoSelection(get<QTableWidget>(window, "booksTable")));
    for (const char *name : {"bookSearch", "newBookId", "bookTitle", "bookAuthor", "bookCategory"})
        CHECK(get<QLineEdit>(window, name)->text().isEmpty());
    CHECK(get<QSpinBox>(window, "bookStock")->value() == 0);
    CHECK(get<QLabel>(window, "bookSelectionTitle")->text() == QStringLiteral("尚未选择图书"));
    CHECK(!get<QPushButton>(window, "saveButton")->isEnabled());
    CHECK(!get<QPushButton>(window, "deleteBookButton")->isEnabled());
    CHECK(get<QPushButton>(window, "addBookButton")->isEnabled());
    CHECK(!window.statusBar()->currentMessage().contains(QStringLiteral("已选中图书")));
    return true;
}

QStringList values(QTableWidget *table)
{
    QStringList result;
    for (int row = 0; row < table->rowCount(); ++row)
        for (int column = 0; column < table->columnCount(); ++column)
            result.append(table->item(row, column)->text());
    return result;
}

bool sampleInteractions()
{
    QTemporaryDir directory;
    CHECK(copyData(QStringLiteral(SAMPLE_DATA_DIR), directory));
    const auto originalCsv = csvContents(directory);
    {
        MainWindow window(directory.path());
        window.show();
        window.activateWindow();
        settle();
        auto *tabs = get<QTabWidget>(window, "mainTabs");
        auto *books = get<QTableWidget>(window, "booksTable");
        auto *stock = get<QSpinBox>(window, "bookStock");
        auto *search = get<QLineEdit>(window, "bookSearch");
        auto *find = get<QPushButton>(window, "searchButton");
        auto *all = get<QPushButton>(window, "showAllButton");
        auto *keyword = get<QLineEdit>(window, "keywordSearch");
        auto *filter = get<QPushButton>(window, "keywordButton");

        clickCell(books, 2, 1); // First click selects B003, even though Qt selects during press.
        CHECK(search->text() == "B003" && stock->value() == 1);
        CHECK(books->selectionModel()->isRowSelected(2));
        CHECK(capture(window, "books-selected"));
        get<QLineEdit>(window, "newBookId")->setText("B999");
        get<QLineEdit>(window, "bookTitle")->setText(QStringLiteral("未保存的编辑"));
        stock->setValue(17);
        clickCell(books, 2, 4); // Another column of the same book must cancel.
        CHECK(bookWasReset(window));
        CHECK(capture(window, "books-reset"));
        CHECK(csvContents(directory) == originalCsv);
        all->click(); // A stale selectedId_ would reselect the old object here.
        CHECK(bookWasReset(window));
        clickCell(books, 2, 0);
        CHECK(search->text() == "B003" && stock->value() == 1);
        clickCell(books, 3, 2);
        CHECK(search->text() == "B004" && stock->value() == 2);
        search->setText("B003");
        find->click();
        find->click();
        CHECK(search->text() == "B003" && books->selectionModel()->isRowSelected(2));
        clickCell(books, 2, 3);
        CHECK(bookWasReset(window));

        // Keyboard selection is also part of the real state before a mouse press.
        clickCell(books, 2, 0);
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(books, &down);
        CHECK(books->currentRow() == 3 && books->selectionModel()->isRowSelected(3));
        clickCell(books, 3, 1);
        CHECK(bookWasReset(window));
        clickCell(books, 2, 0);
        clickCell(books, 2, 0, QEvent::MouseButtonDblClick);
        CHECK(bookWasReset(window));
        clickCell(books, 2, 0);
        CHECK(search->text() == "B003");
        clickCell(books, 2, 0, QEvent::MouseButtonDblClick);
        CHECK(bookWasReset(window));

        keyword->setText(QStringLiteral("布莱恩特"));
        filter->click();
        CHECK(books->rowCount() == 1);
        clickCell(books, 0, 0);
        CHECK(search->text() == "B004");
        clickCell(books, 0, 2);
        CHECK(bookWasReset(window));
        CHECK(keyword->text() == QStringLiteral("布莱恩特") && books->rowCount() == 1);
        keyword->setText("no-such-title");
        filter->click();
        CHECK(books->rowCount() == 0);
        search->setText("B003");
        find->click();
        click(books->viewport(), QPoint(20, 12)); // Blank viewport is not an item click.
        CHECK(search->text() == "B003" && stock->value() == 1);
        all->click();
        click(books->viewport(), books->visualItemRect(books->item(2, 2)).center(), Qt::RightButton);
        click(books->horizontalHeader()->viewport(), QPoint(30, 16));
        CHECK(search->text() == "B003" && get<QPushButton>(window, "saveButton")->isEnabled());
        clickCell(books, 2, 1);
        CHECK(bookWasReset(window));

        tabs->setCurrentIndex(1);
        auto *recommend = get<QTableWidget>(window, "recommendTable");
        const auto recommendations = values(recommend);
        get<QLineEdit>(window, "recommendUser")->setText("unsubmitted-user");
        get<QSpinBox>(window, "topK")->setValue(7);
        get<QLineEdit>(window, "ratingUser")->setText("U1");
        get<QLineEdit>(window, "ratingBook")->setText("B002");
        get<QSpinBox>(window, "ratingScore")->setValue(3);
        clickCell(recommend, 0, 1);
        CHECK(recommend->selectionModel()->isRowSelected(0));
        CHECK(capture(window, "recommend-selected"));
        clickCell(recommend, 0, 3);
        CHECK(hasNoSelection(recommend) && values(recommend) == recommendations);
        CHECK(get<QLineEdit>(window, "recommendUser")->text() == "unsubmitted-user");
        CHECK(get<QSpinBox>(window, "topK")->value() == 7);
        CHECK(get<QLineEdit>(window, "ratingUser")->text() == "U1");
        CHECK(get<QLineEdit>(window, "ratingBook")->text() == "B002");
        CHECK(get<QSpinBox>(window, "ratingScore")->value() == 3);
        CHECK(window.statusBar()->currentMessage() == QStringLiteral("用户 U0 · 2 条推荐"));
        CHECK(capture(window, "recommend-reset"));
        get<QLineEdit>(window, "recommendUser")->setText("U0");
        get<QPushButton>(window, "recommendButton")->click();
        clickCell(recommend, 0, 0);
        CHECK(recommend->selectionModel()->isRowSelected(0));
        clickCell(recommend, 0, 2, QEvent::MouseButtonDblClick);
        CHECK(hasNoSelection(recommend));

        tabs->setCurrentIndex(2);
        auto *borrows = get<QTableWidget>(window, "borrowTable");
        get<QLineEdit>(window, "borrowUser")->setText("U2");
        get<QLineEdit>(window, "borrowBook")->setText("B003");
        const bool returnEnabled = get<QPushButton>(window, "returnButton")->isEnabled();
        clickCell(borrows, 0, 5);
        CHECK(get<QLineEdit>(window, "returnRecord")->text() == "R001");
        CHECK(capture(window, "borrow-selected"));
        clickCell(borrows, 0, 2);
        CHECK(hasNoSelection(borrows));
        CHECK(get<QLineEdit>(window, "returnRecord")->text().isEmpty());
        CHECK(get<QLabel>(window, "borrowSelection")->text() == QStringLiteral("尚未选择借阅记录"));
        CHECK(get<QLineEdit>(window, "borrowUser")->text() == "U2");
        CHECK(get<QLineEdit>(window, "borrowBook")->text() == "B003");
        CHECK(get<QPushButton>(window, "returnButton")->isEnabled() == returnEnabled);
        CHECK(window.statusBar()->currentMessage() == QStringLiteral("借阅记录 · 2 条"));
        CHECK(capture(window, "borrow-reset"));
        clickCell(borrows, 0, 0);
        clickCell(borrows, 1, 0);
        CHECK(get<QLineEdit>(window, "returnRecord")->text() == "R002");
        clickCell(borrows, 1, 4, QEvent::MouseButtonDblClick);
        CHECK(hasNoSelection(borrows) && get<QLineEdit>(window, "returnRecord")->text().isEmpty());
        tabs->setCurrentIndex(0);
        CHECK(bookWasReset(window));
        CHECK(csvContents(directory) == originalCsv);
    }
    // Reload immediately after cancellations: none of the three pages wrote data.
    CHECK(csvContents(directory) == originalCsv);
    MainWindow restarted(directory.path());
    CHECK(get<QTableWidget>(restarted, "booksTable")->item(2, 4)->text() == "1");
    CHECK(get<QTableWidget>(restarted, "borrowTable")->item(0, 5)->text() == QStringLiteral("未归还"));
    CHECK(get<QLabel>(restarted, "ratingCount")->text() == "10");
    return true;
}

bool programmaticSelection()
{
    QTemporaryDir directory;
    CHECK(copyData(QStringLiteral(SAMPLE_GOODBOOKS_DATA_DIR), directory));
    MainWindow window(directory.path());
    window.show();
    settle();
    auto *books = get<QTableWidget>(window, "booksTable");
    auto *search = get<QLineEdit>(window, "bookSearch");
    search->setText("B003");
    get<QPushButton>(window, "searchButton")->click();
    auto *scroll = books->verticalScrollBar();
    CHECK(scroll->isVisible() && scroll->maximum() > 0);
    click(scroll, QPoint(scroll->width() / 2, scroll->height() - 20));
    CHECK(search->text() == "B003" && get<QPushButton>(window, "saveButton")->isEnabled());
    books->scrollToItem(books->item(2, 0));
    settle();
    const QPoint pressed = books->visualItemRect(books->item(2, 0)).center();
    mouse(books->viewport(), pressed, QEvent::MouseButtonPress, Qt::LeftButton);
    get<QPushButton>(window, "showAllButton")->click();
    mouse(books->viewport(), pressed, QEvent::MouseButtonRelease, Qt::LeftButton);
    CHECK(search->text() == "B003" && books->selectionModel()->isRowSelected(2));
    get<QLineEdit>(window, "bookTitle")->setText("Saved title");
    get<QPushButton>(window, "saveButton")->click();
    get<QPushButton>(window, "saveButton")->click();
    CHECK(search->text() == "B003" && books->selectionModel()->isRowSelected(2));
    clickCell(books, 2, 0);
    CHECK(bookWasReset(window));
    clickCell(books, 2, 0);
    CHECK(get<QLineEdit>(window, "bookTitle")->text() == "Saved title");
    get<QLineEdit>(window, "newBookId")->setText("B101");
    get<QPushButton>(window, "addBookButton")->click();
    CHECK(search->text() == "B101" && books->selectionModel()->isRowSelected(100));
    const auto savedCsv = csvContents(directory);
    clickCell(books, 100, 1);
    CHECK(bookWasReset(window));
    get<QPushButton>(window, "showAllButton")->click();
    CHECK(bookWasReset(window) && csvContents(directory) == savedCsv);
    MainWindow restarted(directory.path());
    CHECK(get<QTableWidget>(restarted, "booksTable")->item(2, 1)->text() == "Saved title");
    CHECK(get<QTableWidget>(restarted, "booksTable")->rowCount() == 101);
    return true;
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QApplication app(argc, argv);
    if (app.platformName() == QStringLiteral("offscreen")
        && QFile::exists(QStringLiteral("C:/Windows/Fonts/msyh.ttc")))
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    return sampleInteractions() && programmaticSelection() ? 0 : 1;
}
