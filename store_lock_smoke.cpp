#include "librarystore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { \
    qCritical() << "Line" << __LINE__ << ":" << #condition; return false; } } while (false)

namespace {
bool copyData(const QTemporaryDir &dir)
{
    CHECK(dir.isValid());
    for (const QString &name : QStringList{"books.csv", "users.csv", "ratings.csv", "borrow_records.csv"})
        CHECK(QFile::copy(QDir(QStringLiteral(SAMPLE_DATA_DIR)).filePath(name), dir.filePath(name)));
    return true;
}

QByteArray handshake(QProcess &child)
{
    QByteArray line;
    QElapsedTimer deadline;
    deadline.start();
    while (!line.contains('\n') && deadline.elapsed() < 5000) {
        line += child.readAllStandardOutput();
        if (!line.contains('\n')) child.waitForReadyRead(100);
    }
    return line.trimmed();
}

bool release(QProcess &child, bool crash = false)
{
    CHECK(child.write(crash ? "crash\n" : "quit\n") > 0);
    CHECK(child.waitForFinished(5000));
    CHECK(child.exitCode() == (crash ? 23 : 0));
    return true;
}

int holdDirectory(const QString &path, bool initialize)
{
    QString error;
    if (initialize && !LibraryStore::initializeDataDirectory(path, &error)) {
        QTextStream(stdout) << "REFUSED " << error << '\n' << Qt::flush;
        return 3;
    }
    LibraryStore store;
    if (!store.load(path, &error)) {
        QTextStream(stdout) << "REFUSED " << error << '\n' << Qt::flush;
        return 3;
    }
    QTextStream(stdout) << "READY\n" << Qt::flush;
    const QString command = QTextStream(stdin).readLine();
    if (command == "crash") std::_Exit(23); // Deliberately leave only this temporary test lock behind.
    return 0;
}

bool sameProcess()
{
    QTemporaryDir dir, other, invalid;
    CHECK(copyData(dir) && copyData(other) && copyData(invalid));
    QString error;
    {
        LibraryStore a, b;
        CHECK(a.load(dir.path(), &error));
        QFile transaction(dir.filePath(".borrow_transaction"));
        CHECK(transaction.open(QIODevice::WriteOnly) && transaction.write("pending") == 7);
        transaction.close();
        CHECK(!b.load(dir.path(), &error) && error.contains(QStringLiteral("正在使用")));
        CHECK(error.contains(QDir(dir.path()).canonicalPath()));
        CHECK(transaction.exists() && transaction.remove()); // Denied readers must not run recovery first.
        CHECK(!b.addBook({"B900", "Denied", "Author", "Category", 1}, &error));
        CHECK(a.addBook({"B901", "From A", "Author", "Category", 1}, &error));
        CHECK(a.load(dir.path(), &error)); // Own-directory reload reuses its lock.
        CHECK(!b.load(QDir::current().relativeFilePath(dir.path()), &error));
        CHECK(b.load(other.path(), &error));
        CHECK(b.addBook({"B903", "Other directory", "Author", "Category", 1}, &error));
        CHECK(!a.load(other.path(), &error));
        CHECK(a.books().findById("B901") && a.addBook({"B904", "Still A", "Author", "Category", 1}, &error));
        QFile broken(invalid.filePath("ratings.csv"));
        CHECK(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        CHECK(broken.write("invalid\n") > 0); broken.close();
        CHECK(!a.load(invalid.path(), &error));
        CHECK(a.books().findById("B904"));
        CHECK(!QFile::exists(invalid.filePath(".bookrecommendation.lock")));
        LibraryStore firstFailure;
        CHECK(!firstFailure.load(invalid.path(), &error));
        CHECK(!QFile::exists(invalid.filePath(".bookrecommendation.lock")));
        CHECK(QDir(invalid.path()).mkdir(".bookrecommendation.lock"));
        CHECK(!firstFailure.load(invalid.path(), &error));
        CHECK(!firstFailure.addBook({"B999", "No lock", "Author", "Category", 1}, &error));
        CHECK(QDir(invalid.path()).rmdir(".bookrecommendation.lock"));
    }
    {
        LibraryStore b;
        CHECK(b.load(dir.path(), &error));
        CHECK(b.books().findById("B901") && b.books().findById("B904"));
        CHECK(b.addBook({"B902", "From B", "Author", "Category", 1}, &error));
        CHECK(b.load(other.path(), &error)); // Successful switch releases the old directory.
        LibraryStore third;
        CHECK(third.load(dir.path(), &error));
        CHECK(third.books().findById("B901") && third.books().findById("B902"));
        CHECK(!third.books().findById("B903"));
    }
    LibraryStore unloaded;
    QString record;
    CHECK(!unloaded.addBook({"B999", "No directory", "Author", "Category", 1}, &error));
    CHECK(!unloaded.updateBook({"B001", "No directory", "Author", "Category", 1}, &error));
    CHECK(!unloaded.deleteBook("B001", &error));
    CHECK(!unloaded.rate("U0", "B001", 4, &error));
    CHECK(!unloaded.borrowBook("U0", "B004", "2026-10-22", &record, &error));
    CHECK(!unloaded.returnBook("R001", "2026-10-22", &error));
    qInfo() << "R4 same-process PASS: exclusion, aliases, different directories, reload, failed/successful switch, all write guards";
    return true;
}

bool processes(bool longTest)
{
    QTemporaryDir dir;
    CHECK(copyData(dir));
    QProcess holder;
    holder.start(QCoreApplication::applicationFilePath(), {"--hold", dir.path()});
    CHECK(handshake(holder) == "READY");
    QString error;
    LibraryStore denied;
    CHECK(!denied.load(dir.path(), &error));
    if (longTest) {
        QElapsedTimer elapsed; elapsed.start();
        QEventLoop loop;
        QTimer::singleShot(31000, &loop, &QEventLoop::quit);
        loop.exec();
        CHECK(elapsed.elapsed() >= 30000);
        CHECK(!denied.load(dir.path(), &error));
        qInfo() << "R4 long-lock PASS: active holder still excludes later writers after" << elapsed.elapsed() << "ms";
    }
    CHECK(release(holder));
    CHECK(denied.load(dir.path(), &error));
    denied = LibraryStore{};
    holder.start(QCoreApplication::applicationFilePath(), {"--hold", dir.path()});
    CHECK(handshake(holder) == "READY");
    CHECK(release(holder, true));
    CHECK(QFile::exists(dir.filePath(".bookrecommendation.lock")));
    CHECK(denied.load(dir.path(), &error)); // Qt identifies the exited test process, not a timed-out live owner.
    qInfo() << "R4 processes PASS: handshake-confirmed exclusion, normal release and stale lock after abnormal exit";
    return true;
}

bool initializationRace()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    QProcess first, second;
    for (QProcess *child : {&first, &second})
        child->start(QCoreApplication::applicationFilePath(), {"--initialize-hold", dir.path()});
    const QByteArray a = handshake(first), b = handshake(second);
    CHECK((a == "READY") != (b == "READY"));
    QProcess &owner = a == "READY" ? first : second;
    QProcess &refused = a == "READY" ? second : first;
    if (refused.state() != QProcess::NotRunning) CHECK(refused.waitForFinished(5000));
    CHECK(refused.exitCode() == 3);
    CHECK(release(owner));
    LibraryStore loaded;
    QString error;
    CHECK(loaded.load(dir.path(), &error));
    CHECK(loaded.books().all().size() == 4 && loaded.users().size() == 3 && loaded.ratings().size() == 10);
    CHECK(loaded.borrowRecords().size() == 2);
    qInfo() << "R4 initialization PASS: competing first starts leave one complete seed dataset and one writer";
    return true;
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() == 3 && (args[1] == "--hold" || args[1] == "--initialize-hold"))
        return holdDirectory(args[2], args[1] == "--initialize-hold");
    if (args.contains("--long-lock"))
        return processes(true) ? 0 : 1;
    return sameProcess() && processes(false) && initializationRace() ? 0 : 1;
}
