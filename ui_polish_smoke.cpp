#include "mainwindow.h"

#include <QApplication>
#include <QCompleter>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QEventLoop>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyleOptionSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

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

bool copyData(const QString &source, const QTemporaryDir &target)
{
    CHECK(target.isValid());
    for (const QString &name : {QStringLiteral("books.csv"), QStringLiteral("users.csv"),
                                QStringLiteral("ratings.csv"), QStringLiteral("borrow_records.csv")})
        CHECK(QFile::copy(QDir(source).filePath(name), target.filePath(name)));
    return true;
}

// Close expected error/confirmation dialogs inside their nested event loop.
void answerDialog(MainWindow &window, bool confirmation = false, const QString &screenshot = {})
{
    QTimer::singleShot(0, &window, [&window, confirmation, screenshot] {
        for (auto *box : window.findChildren<QMessageBox *>()) {
            if (!box->isVisible())
                continue;
            const QString directory = qEnvironmentVariable("UI_SCREENSHOT_DIR");
            if (!directory.isEmpty() && !screenshot.isEmpty())
                box->grab().save(QDir(directory).filePath(screenshot + ".png"));
            if (confirmation && box->button(QMessageBox::Yes)) {
                box->button(QMessageBox::Yes)->click();
                answerDialog(window); // A referenced book can produce a second error dialog.
            } else {
                box->accept();
            }
        }
    });
}

bool capturePages(MainWindow &window, const QString &prefix)
{
    const QString directory = qEnvironmentVariable("UI_SCREENSHOT_DIR");
    if (directory.isEmpty())
        return true;
    CHECK(QDir().mkpath(directory));
    auto *tabs = get<QTabWidget>(window, "mainTabs");
    const QStringList pages{QStringLiteral("books"), QStringLiteral("recommend"), QStringLiteral("borrow")};
    for (int page = 0; page < pages.size(); ++page) {
        tabs->setCurrentIndex(page);
        const char *fields[]{"bookSearch", "recommendUser", "borrowUser"};
        get<QLineEdit>(window, fields[page])->setFocus();
        QApplication::processEvents();
        CHECK(window.grab().save(QDir(directory).filePath(prefix + "-" + pages[page] + ".png")));
        const char *focusFields[]{"bookTitle", "ratingScore", "returnRecord"};
        window.findChild<QWidget *>(QString::fromLatin1(focusFields[page]))->setFocus();
        QApplication::processEvents();
        CHECK(window.grab().save(QDir(directory).filePath(prefix + "-" + pages[page] + "-focus.png")));
    }
    return true;
}

bool checkLayout(MainWindow &window, const QSize &size, bool captureCompact = false)
{
    window.resize(size);
    window.show();
    auto *tabs = get<QTabWidget>(window, "mainTabs");
    const QList<QStringList> controls{
        {"bookSearch", "keywordSearch", "newBookId", "bookTitle", "bookAuthor", "bookCategory",
         "bookStock", "searchButton", "keywordButton", "showAllButton", "saveButton", "addBookButton", "deleteBookButton"},
        {"recommendUser", "topK", "recommendButton", "ratingUser", "ratingBook", "ratingScore", "rateButton"},
        {"borrowUser", "borrowBook", "borrowButton", "returnRecord", "returnButton"}};
    for (int page = 0; page < controls.size(); ++page) {
        tabs->setCurrentIndex(page);
        QApplication::processEvents();
        for (const QString &name : controls[page]) {
            auto *widget = window.findChild<QWidget *>(name);
            CHECK(widget && widget->isVisible());
            // A short screen may need form scrolling; every control must be reachable in full.
            for (QWidget *ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
                if (auto *scroll = qobject_cast<QScrollArea *>(ancestor)) {
                    scroll->ensureWidgetVisible(widget);
                    QApplication::processEvents();
                    // Exercise the scrollbar if Qt considers a partially visible editor sufficient.
                    const QRect frame(widget->mapTo(scroll->viewport(), QPoint()), widget->size());
                    auto *bar = scroll->verticalScrollBar();
                    if (frame.top() < 0)
                        bar->setValue(bar->value() + frame.top());
                    else if (frame.bottom() >= scroll->viewport()->height())
                        bar->setValue(bar->value() + frame.bottom() - scroll->viewport()->height() + 1);
                    QApplication::processEvents();
                    if (!scroll->viewport()->rect().contains(QRect(
                            widget->mapTo(scroll->viewport(), QPoint()), widget->size())))
                        qWarning() << name << "viewport" << scroll->viewport()->rect()
                                   << "control" << QRect(widget->mapTo(scroll->viewport(), QPoint()), widget->size());
                    CHECK(scroll->viewport()->rect().contains(QRect(
                        widget->mapTo(scroll->viewport(), QPoint()), widget->size())));
                }
            }
            CHECK(window.rect().contains(QRect(widget->mapTo(&window, QPoint()), widget->size())));
        }
        // These actions must remain visible even while the form is scrolled.
        const QStringList fixedButtons = page == 0
            ? QStringList{"saveButton", "addBookButton", "deleteBookButton"}
            : page == 1 ? QStringList{"rateButton"} : QStringList{"returnButton"};
        for (const QString &name : fixedButtons)
            CHECK(!window.findChild<QPushButton *>(name)->visibleRegion().isEmpty());
    }
    if (captureCompact)
        CHECK(capturePages(window, QStringLiteral("sample-compact") + qEnvironmentVariable("UI_SCREENSHOT_TAG")));
    return true;
}

QRect spinButton(QSpinBox *box, QStyle::SubControl control)
{
    QStyleOptionSpinBox option;
    option.initFrom(box);
    option.buttonSymbols = box->buttonSymbols();
    return box->style()->subControlRect(QStyle::CC_SpinBox, &option, control, box);
}

void spinMouse(QSpinBox *box, QEvent::Type type, const QPoint &point)
{
    const bool move = type == QEvent::MouseMove;
    QMouseEvent event(type, QPointF(point), QPointF(box->mapToGlobal(point)),
                      move ? Qt::NoButton : Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(box, &event);
}

void spinClick(QSpinBox *box, QStyle::SubControl control)
{
    const QPoint point = spinButton(box, control).center();
    spinMouse(box, QEvent::MouseButtonPress, point);
    spinMouse(box, QEvent::MouseButtonRelease, point);
}

void spinKey(QWidget *widget, int key, const QString &text = {})
{
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    QApplication::sendEvent(widget, &press);
    QApplication::sendEvent(widget, &release);
}

bool checkSpinBoxes(MainWindow &window)
{
    const QList<QSpinBox *> boxes{get<QSpinBox>(window, "bookStock"),
                                  get<QSpinBox>(window, "topK"), get<QSpinBox>(window, "ratingScore")};
    const int minima[]{0, 1, 1}, maxima[]{1000000, 1000, 5}, defaults[]{0, 2, 4};
    const int middle[]{3, 5, 3};
    auto *tabs = get<QTabWidget>(window, "mainTabs");
    for (int index = 0; index < boxes.size(); ++index) {
        auto *box = boxes[index];
        tabs->setCurrentIndex(index == 0 ? 0 : 1);
        QApplication::processEvents();
        CHECK(box->minimum() == minima[index] && box->maximum() == maxima[index]);
        CHECK(box->value() == defaults[index] && box->singleStep() == 1 && !box->wrapping());
        CHECK(box->buttonSymbols() == QAbstractSpinBox::PlusMinus);
        const QRect minus = spinButton(box, QStyle::SC_SpinBoxDown);
        const QRect plus = spinButton(box, QStyle::SC_SpinBoxUp);
        CHECK(minus.size() == QSize(28, 28) && plus.size() == QSize(28, 28));
        CHECK(minus.right() + 3 == plus.left() && minus.top() == plus.top());
        CHECK(box->rect().contains(minus) && box->rect().contains(plus));
        auto *edit = box->findChild<QLineEdit *>();
        CHECK(edit && edit->geometry().right() < minus.left());
        get<QLineEdit>(window, index == 0 ? "bookSearch" : "ratingUser")->setFocus();
        QApplication::processEvents();
        const QRect editBefore = edit->geometry();
        box->setFocus();
        QApplication::processEvents();
        CHECK(edit->geometry() == editBefore);
        CHECK(spinButton(box, QStyle::SC_SpinBoxDown) == minus);
        CHECK(spinButton(box, QStyle::SC_SpinBoxUp) == plus);
        box->setValue(middle[index]);
        spinClick(box, QStyle::SC_SpinBoxUp);
        CHECK(box->value() == middle[index] + 1);
        spinClick(box, QStyle::SC_SpinBoxDown);
        CHECK(box->value() == middle[index]);
        spinKey(box, Qt::Key_Up);
        CHECK(box->value() == middle[index] + 1);
        spinKey(box, Qt::Key_Down);
        CHECK(box->value() == middle[index]);
        box->setValue(minima[index]);
        spinClick(box, QStyle::SC_SpinBoxDown);
        CHECK(box->value() == minima[index]);
        spinClick(box, QStyle::SC_SpinBoxUp);
        CHECK(box->value() == minima[index] + 1);
        box->setValue(maxima[index]);
        spinClick(box, QStyle::SC_SpinBoxUp);
        CHECK(box->value() == maxima[index]);
        spinClick(box, QStyle::SC_SpinBoxDown);
        CHECK(box->value() == maxima[index] - 1);
        edit->selectAll();
        const QString maximum = QString::number(maxima[index]);
        for (const QChar digit : maximum)
            spinKey(edit, Qt::Key_0 + digit.digitValue(), QString(digit));
        box->interpretText();
        CHECK(box->value() == maxima[index] && box->hasAcceptableInput());
        if (edit->fontMetrics().horizontalAdvance(edit->text()) + 4 > edit->width())
            qWarning() << box->objectName() << edit->geometry() << edit->text()
                       << edit->fontMetrics().horizontalAdvance(edit->text());
        CHECK(edit->fontMetrics().horizontalAdvance(edit->text()) + 4 <= edit->width());
        spinKey(edit, Qt::Key_9, QStringLiteral("9"));
        box->interpretText();
        CHECK(box->value() >= minima[index] && box->value() <= maxima[index]);
        box->setValue(middle[index]);
        box->setEnabled(false);
        spinClick(box, QStyle::SC_SpinBoxUp);
        CHECK(box->value() == middle[index]);
        box->setEnabled(true);
        box->setValue(defaults[index]);
    }
    CHECK(boxes[1]->width() == 136);
    boxes[1]->setValue(20);
    spinMouse(boxes[1], QEvent::MouseButtonPress, spinButton(boxes[1], QStyle::SC_SpinBoxUp).center());
    QEventLoop hold;
    QTimer::singleShot(900, &hold, &QEventLoop::quit);
    hold.exec();
    spinMouse(boxes[1], QEvent::MouseButtonRelease, spinButton(boxes[1], QStyle::SC_SpinBoxUp).center());
    CHECK(boxes[1]->value() > 21); // Qt's existing repeat timer must still run.
    boxes[1]->setValue(defaults[1]);

    const QString directory = qEnvironmentVariable("SPIN_SCREENSHOT_DIR");
    if (!directory.isEmpty()) {
        CHECK(QDir().mkpath(directory));
        const qreal ratio = window.devicePixelRatioF();
        QPixmap sheet(QSize(1060, 502) * ratio);
        sheet.setDevicePixelRatio(ratio);
        sheet.fill(QColor("#F4F7F9"));
        QPainter painter(&sheet);
        painter.setFont(window.font());
        painter.setPen(QColor("#172B3A"));
        const QStringList titles{QStringLiteral("库存 · 0～1000000"), QStringLiteral("Top K · 1～1000"),
                                 QStringLiteral("评分 · 1～5")};
        const QStringList states{QStringLiteral("默认"), QStringLiteral("悬停减号"), QStringLiteral("悬停加号"),
            QStringLiteral("按下减号"), QStringLiteral("按下加号"), QStringLiteral("最小值"),
            QStringLiteral("最大值"), QStringLiteral("聚焦"), QStringLiteral("整框禁用")};
        for (int index = 0; index < boxes.size(); ++index) {
            auto *box = boxes[index];
            tabs->setCurrentIndex(index == 0 ? 0 : 1);
            QApplication::processEvents();
            painter.drawText(150 + index * 300, 28, titles[index]);
            for (int state = 0; state < states.size(); ++state) {
                get<QLineEdit>(window, index == 0 ? "bookSearch" : "ratingUser")->setFocus();
                box->setValue(state == 5 ? minima[index] : (state == 6 || state == 7)
                                                               ? maxima[index] : middle[index]);
                auto *edit = box->findChild<QLineEdit *>();
                edit->deselect();
                edit->setCursorPosition(edit->text().size());
                QEvent leave(QEvent::Leave);
                QApplication::sendEvent(box, &leave);
                const QPoint point = spinButton(box, state == 1 || state == 3
                    ? QStyle::SC_SpinBoxDown : QStyle::SC_SpinBoxUp).center();
                if (state >= 1 && state <= 4) {
                    QEnterEvent enter(QPointF(point), QPointF(point), QPointF(box->mapToGlobal(point)));
                    QApplication::sendEvent(box, &enter);
                    QHoverEvent hover(QEvent::HoverMove, QPointF(point), QPointF(box->mapToGlobal(point)), QPointF(-1, -1));
                    QApplication::sendEvent(box, &hover);
                    spinMouse(box, QEvent::MouseMove, point);
                }
                if (state == 3 || state == 4)
                    spinMouse(box, QEvent::MouseButtonPress, point);
                if (state == 7)
                    box->setFocus();
                if (state == 8)
                    box->setEnabled(false);
                QApplication::processEvents();
                if (index == 0)
                    painter.drawText(20, 74 + state * 48, states[state]);
                painter.drawPixmap(150 + index * 300, 50 + state * 48, box->grab());
                if (state == 3 || state == 4)
                    spinMouse(box, QEvent::MouseButtonRelease, point);
                box->setEnabled(true);
            }
            box->setValue(defaults[index]);
        }
        painter.end();
        CHECK(sheet.save(QDir(directory).filePath("states-" + qEnvironmentVariable("SPIN_SCREENSHOT_TAG", "150") + ".png")));
    }
    tabs->setCurrentIndex(0);
    return true;
}

bool sampleStates()
{
    QTemporaryDir dir;
    CHECK(copyData(QStringLiteral(SAMPLE_DATA_DIR), dir));
    MainWindow window(dir.path());
    window.show();
    QApplication::processEvents();
    CHECK(!window.styleSheet().isEmpty());
    const QSize available = window.screen()->availableGeometry().size();
    CHECK(window.width() <= available.width() - 32 && window.height() <= available.height() - 64);
    CHECK(get<QLabel>(window, "bookCount")->text() == "4");
    CHECK(get<QLabel>(window, "userCount")->text() == "3");
    CHECK(get<QLabel>(window, "ratingCount")->text() == "10");
    CHECK(get<QLabel>(window, "borrowCount")->text() == "2");
    CHECK(checkSpinBoxes(window));
    CHECK(get<QLabel>(window, "bookSelectionTitle")->text() == QStringLiteral("尚未选择图书"));
    CHECK(!get<QWidget>(window, "bookSelectionCard")->property("selected").toBool());
    auto *tabs = get<QTabWidget>(window, "mainTabs");
    tabs->setCurrentIndex(1);
    QApplication::processEvents();
    auto *rating = get<QSpinBox>(window, "ratingScore");
    auto *rate = get<QPushButton>(window, "rateButton");
    const int gap = rate->mapTo(&window, QPoint()).y()
        - rating->mapTo(&window, QPoint(0, rating->height())).y();
    CHECK(gap >= 8 && gap <= 24);
    CHECK(get<QWidget>(window, "ratingCard")->height() < tabs->currentWidget()->height() - 40);
    tabs->setCurrentIndex(0);
    auto *directoryLabel = get<QLabel>(window, "datasetLabel");
    CHECK(directoryLabel->toolTip() == QDir::toNativeSeparators(dir.path()));
    auto *save = get<QPushButton>(window, "saveButton");
    auto *remove = get<QPushButton>(window, "deleteBookButton");
    CHECK(!save->isEnabled() && !remove->isEnabled());
    CHECK(get<QPushButton>(window, "addBookButton")->isEnabled());
    auto *recommendations = get<QTableWidget>(window, "recommendTable");
    CHECK(recommendations->item(0, 0)->text() == "B003");
    CHECK(recommendations->item(0, 2)->text() == "3.8889");
    CHECK(recommendations->item(1, 0)->text() == "B004");
    CHECK(recommendations->item(1, 2)->text() == "2.1111");
    auto *search = get<QLineEdit>(window, "bookSearch");
    CHECK(search->completer() && search->completer()->model()->rowCount() == 4);
    search->setText("B001");
    get<QPushButton>(window, "searchButton")->click();
    CHECK(save->isEnabled() && remove->isEnabled());
    CHECK(get<QWidget>(window, "bookSelectionCard")->property("selected").toBool());
    const QString savedTitle = get<QLabel>(window, "bookSelectionTitle")->text();
    CHECK(savedTitle == QStringLiteral("算法图解"));
    get<QLineEdit>(window, "bookTitle")->setText(QStringLiteral("尚未保存的书名"));
    CHECK(get<QLabel>(window, "bookSelectionTitle")->text() == savedTitle);
    get<QLineEdit>(window, "bookTitle")->setText(savedTitle);
    search->setText("B0");
    search->completer()->setCompletionPrefix(QStringLiteral("B0"));
    search->completer()->complete();
    QApplication::processEvents();
    auto *popup = search->completer()->popup();
    CHECK(popup->model()->rowCount() == 4 && !popup->styleSheet().isEmpty());
    const QString screenshotDir = qEnvironmentVariable("UI_SCREENSHOT_DIR");
    if (!screenshotDir.isEmpty()) {
        CHECK(QDir().mkpath(screenshotDir));
        CHECK(popup->grab().save(QDir(screenshotDir).filePath("id-completion.png")));
    }
    popup->hide();
    search->setText("B001");
    get<QLineEdit>(window, "returnRecord")->setText("R001");
    CHECK(get<QLabel>(window, "borrowSelection")->text().contains(QStringLiteral("未归还")));
    CHECK(get<QTableWidget>(window, "borrowTable")->item(0, 4)->text() == QStringLiteral("—"));
    for (const char *name : {"booksTable", "recommendTable", "borrowTable"}) {
        auto *table = get<QTableWidget>(window, name);
        for (int row = 0; row < table->rowCount(); ++row)
            for (int column = 0; column < table->columnCount(); ++column) {
                CHECK(!table->cellWidget(row, column));
                CHECK(table->item(row, column)->text() == table->item(row, column)->toolTip());
            }
    }
    CHECK(capturePages(window, QStringLiteral("sample") + qEnvironmentVariable("UI_SCREENSHOT_TAG")));
    get<QTabWidget>(window, "mainTabs")->setCurrentIndex(0);
    search->setFocus();
    QApplication::processEvents();
    CHECK(search->hasFocus());
    if (!qEnvironmentVariable("UI_SCREENSHOT_DIR").isEmpty())
        CHECK(window.grab().save(QDir(qEnvironmentVariable("UI_SCREENSHOT_DIR"))
            .filePath("sample-focus" + qEnvironmentVariable("UI_SCREENSHOT_TAG") + ".png")));
    CHECK(checkLayout(window, window.size()));
    CHECK(checkLayout(window, QSize(1000, 680), true));

    auto *table = get<QTableWidget>(window, "booksTable");
    for (int row = 0; row < table->rowCount(); ++row)
        for (int column = 0; column < table->columnCount(); ++column)
            CHECK(table->item(row, column)->toolTip() == table->item(row, column)->text());
    get<QLineEdit>(window, "keywordSearch")->setText(QStringLiteral("布莱恩特"));
    get<QPushButton>(window, "keywordButton")->click();
    CHECK(table->rowCount() == 1 && table->item(0, 0)->text() == "B004");
    CHECK(get<QLabel>(window, "bookSelection")->text().contains("B001"));
    CHECK(get<QLabel>(window, "bookSelection")->text().contains(QStringLiteral("不在筛选结果")));
    CHECK(save->isEnabled());
    get<QLineEdit>(window, "keywordSearch")->setText("no-such-title");
    get<QPushButton>(window, "keywordButton")->click();
    CHECK(table->rowCount() == 0 && !get<QLabel>(window, "booksEmpty")->isHidden());
    if (!screenshotDir.isEmpty()) {
        tabs->setCurrentIndex(0);
        QApplication::processEvents();
        CHECK(window.grab().save(QDir(screenshotDir).filePath("books-empty.png")));
    }
    get<QPushButton>(window, "showAllButton")->click();
    CHECK(get<QLabel>(window, "booksEmpty")->isHidden());

    const QString originalTitle = get<QLineEdit>(window, "bookTitle")->text();
    get<QLineEdit>(window, "bookTitle")->clear();
    answerDialog(window);
    save->click();
    CHECK(get<QLineEdit>(window, "bookTitle")->text().isEmpty());
    CHECK(save->isEnabled() && window.statusBar()->currentMessage() == QStringLiteral("保存失败"));
    get<QLineEdit>(window, "newBookId")->setText("B001");
    get<QLineEdit>(window, "bookTitle")->setText(QStringLiteral("失败时保留的输入"));
    answerDialog(window, false, QStringLiteral("sample-error") + qEnvironmentVariable("UI_SCREENSHOT_TAG"));
    get<QPushButton>(window, "addBookButton")->click();
    CHECK(get<QLineEdit>(window, "newBookId")->text() == "B001");
    CHECK(get<QLineEdit>(window, "bookTitle")->text() == QStringLiteral("失败时保留的输入"));
    CHECK(get<QLabel>(window, "bookCount")->text() == "4");
    CHECK(window.statusBar()->currentMessage() == QStringLiteral("新增失败"));
    get<QLineEdit>(window, "bookTitle")->setText(originalTitle);
    answerDialog(window, true, QStringLiteral("sample-delete-confirm") + qEnvironmentVariable("UI_SCREENSHOT_TAG"));
    remove->click();
    CHECK(table->rowCount() == 4 && save->isEnabled()); // B001 is referenced.
    get<QLineEdit>(window, "newBookId")->setText("B005");
    get<QLineEdit>(window, "bookTitle")->setText(QStringLiteral("测试新增图书"));
    get<QPushButton>(window, "addBookButton")->click();
    CHECK(get<QLabel>(window, "bookCount")->text() == "5");
    CHECK(search->completer()->model()->rowCount() == 5);
    answerDialog(window, true);
    remove->click();
    CHECK(get<QLabel>(window, "bookCount")->text() == "4");
    CHECK(!save->isEnabled() && !remove->isEnabled());
    CHECK(search->completer()->model()->rowCount() == 4);

    get<QPushButton>(window, "borrowButton")->click();
    CHECK(get<QLineEdit>(window, "returnRecord")->text() == "R003");
    CHECK(get<QLabel>(window, "borrowCount")->text() == "3");
    answerDialog(window);
    get<QPushButton>(window, "borrowButton")->click();
    CHECK(get<QLabel>(window, "borrowCount")->text() == "3");
    get<QPushButton>(window, "returnButton")->click();
    CHECK(get<QLabel>(window, "borrowSelection")->text().contains(QStringLiteral("已归还")));
    answerDialog(window);
    get<QPushButton>(window, "returnButton")->click();
    CHECK(table->item(3, 4)->text() == "2");

    get<QPushButton>(window, "rateButton")->click(); // U0/B003=4, existing recommendation refresh rule.
    CHECK(get<QLabel>(window, "ratingCount")->text() == "11");
    get<QLineEdit>(window, "ratingBook")->setText("B004");
    get<QPushButton>(window, "rateButton")->click();
    CHECK(recommendations->rowCount() == 0);
    CHECK(!get<QLabel>(window, "recommendEmpty")->isHidden());
    if (!screenshotDir.isEmpty()) {
        tabs->setCurrentIndex(1);
        QApplication::processEvents();
        CHECK(window.grab().save(QDir(screenshotDir).filePath("recommend-empty.png")));
    }
    const QString summary = get<QLabel>(window, "recommendSummary")->text();
    get<QLineEdit>(window, "recommendUser")->setText("invalid-user");
    answerDialog(window);
    get<QPushButton>(window, "recommendButton")->click();
    CHECK(get<QLineEdit>(window, "recommendUser")->text() == "invalid-user");
    CHECK(get<QLabel>(window, "recommendSummary")->text() == summary);

    bool pathChecked = false;
    QTimer::singleShot(0, &window, [&] {
        auto *path = get<QLineEdit>(window, "dataDirectoryPath");
        pathChecked = path->isReadOnly() && path->hasSelectedText()
            && path->text() == QDir::toNativeSeparators(dir.path());
        qobject_cast<QDialog *>(path->parentWidget())->accept();
    });
    get<QPushButton>(window, "dataDirectoryButton")->click();
    CHECK(pathChecked);

    QTemporaryDir longDirectory(QDir::tempPath() + "/" + QString(90, QChar(0x957F)) + "-XXXXXX");
    CHECK(copyData(QStringLiteral(SAMPLE_DATA_DIR), longDirectory));
    MainWindow longName(longDirectory.path());
    longName.show();
    QApplication::processEvents();
    auto *longLabel = get<QLabel>(longName, "datasetLabel");
    CHECK(longLabel->fontMetrics().horizontalAdvance(longLabel->text()) > longLabel->width());
    CHECK(longLabel->toolTip() == QDir::toNativeSeparators(longDirectory.path()));
    CHECK(longName.width() <= longName.screen()->availableGeometry().width() - 32);
    if (!screenshotDir.isEmpty())
        CHECK(longName.grab().save(QDir(screenshotDir).filePath("neutral-and-long-directory.png")));

    QTemporaryDir emptyDir;
    CHECK(copyData(QStringLiteral(SAMPLE_DATA_DIR), emptyDir));
    QFile records(emptyDir.filePath("borrow_records.csv"));
    CHECK(records.open(QIODevice::WriteOnly | QIODevice::Truncate));
    CHECK(records.write("record_id,user_id,book_id,borrow_date,return_date,status\n") > 0);
    records.close();
    MainWindow emptyBorrows(emptyDir.path());
    CHECK(get<QLabel>(emptyBorrows, "borrowCount")->text() == "0");
    CHECK(!get<QLabel>(emptyBorrows, "borrowEmpty")->isHidden());
    if (!screenshotDir.isEmpty()) {
        emptyBorrows.show();
        get<QTabWidget>(emptyBorrows, "mainTabs")->setCurrentIndex(2);
        QApplication::processEvents();
        CHECK(emptyBorrows.grab().save(QDir(screenshotDir).filePath("borrow-empty.png")));
    }
    return true;
}

bool goodbooksRehearsal()
{
    QTemporaryDir dir;
    CHECK(copyData(QStringLiteral(SAMPLE_GOODBOOKS_DATA_DIR), dir));
    {
        MainWindow window(dir.path());
        window.show();
        QApplication::processEvents();
        CHECK(get<QLineEdit>(window, "recommendUser")->text() == "U00");
        CHECK(get<QLineEdit>(window, "ratingUser")->text() == "U00");
        CHECK(get<QLabel>(window, "bookCount")->text() == "100");
        CHECK(get<QLabel>(window, "userCount")->text() == "40");
        CHECK(get<QLabel>(window, "ratingCount")->text() == "800");
        CHECK(get<QLabel>(window, "borrowCount")->text() == "10");
        auto *search = get<QLineEdit>(window, "bookSearch");
        search->setText("B100");
        get<QPushButton>(window, "searchButton")->click();
        CHECK(get<QLineEdit>(window, "bookTitle")->text() == "The Poisonwood Bible");
        search->setText("B001");
        get<QPushButton>(window, "searchButton")->click();
        auto *table = get<QTableWidget>(window, "booksTable");
        CHECK(table->item(0, 1)->toolTip() == "The Hunger Games (The Hunger Games, #1)");
        get<QSpinBox>(window, "topK")->setValue(5);
        get<QPushButton>(window, "recommendButton")->click();
        auto *recommendations = get<QTableWidget>(window, "recommendTable");
        const QStringList expected{"B051", "B059", "B068", "B085", "B090"};
        CHECK(recommendations->rowCount() == expected.size());
        for (int row = 0; row < expected.size(); ++row) {
            CHECK(recommendations->item(row, 0)->text() == expected[row]);
            CHECK(recommendations->item(row, 2)->text().section('.', 1).size() == 4);
        }
        get<QLineEdit>(window, "returnRecord")->setText("R001");
        CHECK(capturePages(window, QStringLiteral("goodbooks") + qEnvironmentVariable("UI_SCREENSHOT_TAG")));
        get<QPushButton>(window, "borrowButton")->click();
        CHECK(get<QLineEdit>(window, "returnRecord")->text() == "R011");
        CHECK(get<QLabel>(window, "borrowCount")->text() == "11");
        CHECK(table->item(3, 4)->text() == "2");
        get<QPushButton>(window, "returnButton")->click();
        CHECK(table->item(3, 4)->text() == "3");
        get<QLineEdit>(window, "ratingBook")->setText("B051");
        get<QPushButton>(window, "rateButton")->click();
        CHECK(get<QLabel>(window, "ratingCount")->text() == "801");
        for (int row = 0; row < recommendations->rowCount(); ++row)
            CHECK(recommendations->item(row, 0)->text() != "B051");
    }
    MainWindow restarted(dir.path());
    CHECK(get<QLabel>(restarted, "ratingCount")->text() == "801");
    CHECK(get<QLabel>(restarted, "borrowCount")->text() == "11");
    CHECK(get<QTableWidget>(restarted, "borrowTable")->item(10, 5)->text() == QStringLiteral("已归还"));
    CHECK(get<QTableWidget>(restarted, "booksTable")->item(3, 4)->text() == "3");
    CHECK(get<QLineEdit>(restarted, "recommendUser")->text() == "U00");
    get<QSpinBox>(restarted, "topK")->setValue(5);
    get<QPushButton>(restarted, "recommendButton")->click();
    auto *recommendations = get<QTableWidget>(restarted, "recommendTable");
    for (int row = 0; row < recommendations->rowCount(); ++row)
        CHECK(recommendations->item(row, 0)->text() != "B051");
    return true;
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QApplication app(argc, argv);
    // The Windows offscreen plugin does not discover system fonts on its own.
    if (app.platformName() == QStringLiteral("offscreen")
        && QFile::exists(QStringLiteral("C:/Windows/Fonts/msyh.ttc")))
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    qInfo() << "Qt screen:" << app.primaryScreen()->availableGeometry()
            << "DPR:" << app.primaryScreen()->devicePixelRatio();
    return sampleStates() && goodbooksRehearsal() ? 0 : 1;
}
