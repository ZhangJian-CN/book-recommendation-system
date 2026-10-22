// 输入/展示在 MainWindow，实体与索引在 BookRepository，引用限制和保存回滚在这里。
// 向老师讲删除时，先展示 deleteBook 的引用检查，再展示仓库删除后重建索引。
#include "librarystore.h"
#include "csvutil.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <limits>

//LibraryStore 检查用户和图书是否存在、评分是否有效，并决定新增还是更新记录；然后调用 Csv::write 完成文件写入。

namespace {
const QString directoryLockName = QStringLiteral(".bookrecommendation.lock");

bool acquireDirectoryLock(QLockFile &lock, const QString &path, QString *error)
{
    lock.setStaleLockTime(0); // 活跃实例不会仅因超过默认 30 秒而失去占用。
    if (lock.tryLock(0))
        return true;
    if (lock.error() == QLockFile::LockFailedError)
        *error = QStringLiteral("数据目录正在使用：%1。请关闭使用该目录的其他实例，或选择独立副本。").arg(path);
    else if (lock.error() == QLockFile::PermissionError)
        *error = QStringLiteral("无法取得数据目录写权限：%1").arg(path);
    else
        *error = QStringLiteral("无法锁定数据目录：%1").arg(path);
    return false;
}

bool validId(const QString &id)
{
    return !id.isEmpty() && id == id.trimmed();
}

QString ratingKey(const QString &userId, const QString &bookId)
{
    // 共用辅助键：把用户和图书配成一个组合，用于查重评分和未归还借阅。
    return userId + QChar(0x1f) + bookId;
}

QString markerPath(const QDir &dir)
{
    return dir.filePath(QStringLiteral(".borrow_transaction"));
}

QString backupPath(const QDir &dir, const QString &name)
{
    return dir.filePath(QStringLiteral(".") + name + QStringLiteral(".borrow_backup"));
}

bool writeBytes(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
        || !file.commit()) {
        *error = QStringLiteral("无法写入 %1：%2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool restoreBackup(const QDir &dir, const QString &name, QString *error)
{
    QFile backup(backupPath(dir, name));
    if (!backup.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("借阅事务备份缺失或无法读取：%1").arg(backup.fileName());
        return false;
    }
    const QByteArray original = backup.readAll();
    QFile current(dir.filePath(name));
    if (current.open(QIODevice::ReadOnly) && current.readAll() == original)
        return true;
    current.close();
    return writeBytes(dir.filePath(name), original, error);
}

void clearBorrowTransaction(const QDir &dir)
{
    const QString booksBackup = backupPath(dir, QStringLiteral("books.csv"));
    const QString recordsBackup = backupPath(dir, QStringLiteral("borrow_records.csv"));
    const bool booksRemoved = !QFile::exists(booksBackup) || QFile::remove(booksBackup);
    const bool recordsRemoved = !QFile::exists(recordsBackup) || QFile::remove(recordsBackup);
    if (booksRemoved && recordsRemoved)
        QFile::remove(markerPath(dir));
}

bool recoverBorrowTransaction(const QDir &dir, QString *error)
{
    // 借还模块简述：根据标记决定恢复旧文件或清理已完成事务的备份。
    // pending 表示两份文件尚未整体提交，需要恢复备份；committed 表示已完成，只清理残留。
    QFile marker(markerPath(dir));
    if (!marker.exists())
        return true;
    if (!marker.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("无法读取借阅事务标记：%1").arg(marker.errorString());
        return false;
    }
    const QByteArray state = marker.readAll();
    marker.close();
    if (state == "pending") {
        if (!restoreBackup(dir, QStringLiteral("books.csv"), error)
            || !restoreBackup(dir, QStringLiteral("borrow_records.csv"), error))
            return false;
    } else if (state != "committed") {
        *error = QStringLiteral("借阅事务标记内容无效，请检查数据目录");
        return false;
    }
    clearBorrowTransaction(dir);
    return true;
}

bool saveBorrowRecords(const QDir &dir, const QVector<BorrowRecord> &records,
                       QString *error)
{
    QVector<QStringList> rows;
    for (const BorrowRecord &record : records) {
        rows.append({record.id, record.userId, record.bookId, record.borrowDate,
                     record.returnDate, record.status});
    }
    return Csv::write(dir.filePath(QStringLiteral("borrow_records.csv")),
                      QStringLiteral("record_id,user_id,book_id,borrow_date,return_date,status"),
                      rows, error);
}

bool saveBorrowAndBooks(const QDir &dir, const BookRepository &books,
                        const QVector<BorrowRecord> &records, QString *error)
{
    // 借还同时修改库存文件和借阅文件：先备份，再写 pending，保存两份文件，最后写 committed。
    // 启动及再次保存时检查残留标记；这是应用层恢复机制，不是数据库事务或断电保证。
    if (!recoverBorrowTransaction(dir, error))
        return false;
    const QString booksName = QStringLiteral("books.csv");
    const QString recordsName = QStringLiteral("borrow_records.csv");
    QFile::remove(backupPath(dir, booksName));
    QFile::remove(backupPath(dir, recordsName));
    if (!QFile::copy(dir.filePath(booksName), backupPath(dir, booksName))
        || !QFile::copy(dir.filePath(recordsName), backupPath(dir, recordsName))) {
        *error = QStringLiteral("无法备份图书或借阅记录，未写入更改");
        return false;
    }
    if (!writeBytes(markerPath(dir), "pending", error))
        return false;
    if (!books.save(dir.filePath(booksName), error)
        || !saveBorrowRecords(dir, records, error)
        || !writeBytes(markerPath(dir), "committed", error)) {
        const QString failure = *error;
        QString recoveryError;
        if (!recoverBorrowTransaction(dir, &recoveryError)) {
            *error = failure + QStringLiteral("；恢复未完成：") + recoveryError;
        } else {
            *error = failure + QStringLiteral("；原数据已恢复");
        }
        return false;
    }
    clearBorrowTransaction(dir);
    return true;
}
}

bool LibraryStore::initializeDataDirectory(const QString &dataDir, QString *error)
{
    if (dataDir.trimmed().isEmpty() || !QDir().mkpath(dataDir)) {
        *error = QStringLiteral("无法创建数据目录：%1").arg(dataDir);
        return false;
    }
    const QDir dir(QFileInfo(dataDir).canonicalFilePath());
    QLockFile lock(dir.filePath(directoryLockName));
    if (!acquireDirectoryLock(lock, dir.path(), error))
        return false;
    QStringList files = dir.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    files.removeAll(directoryLockName);
    if (!files.isEmpty())
        return true;
    for (const QString &name : QStringList{"books.csv", "users.csv", "ratings.csv", "borrow_records.csv"}) {
        const QString target = dir.filePath(name);
        if (!QFile::copy(QStringLiteral(":/seed/") + name, target)
            || !QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner | QFileDevice::WriteUser)) {
            *error = QStringLiteral("无法初始化示例数据：%1").arg(target);
            return false;
        }
    }
    return true;
}

bool LibraryStore::requireWriteAccess(QString *error) const
{
    if (directoryLock_ && directoryLock_->isLocked() && !dataDir_.isEmpty())
        return true;
    *error = QStringLiteral("尚未成功加载并持有数据目录写入锁，不能保存数据");
    return false;
}

bool LibraryStore::load(const QString &dataDir, QString *error)
{
    // 【数据接口讲解】四份文件通过 user_id/book_id 连接，不能各读一份就直接交给界面。
    // 先处理未完成借还的恢复，再按依赖顺序校验；所有局部数据合法后才一起替换成员。
    // 例：ratings.csv 引用了不存在的 B999，必须整次加载失败，不能出现悬空推荐记录。
    // 加载顺序：图书 -> 用户 -> 评分 -> 借阅；后两者需要引用前两者的有效编号。
    // 各类记录先存到局部变量，全部通过校验后才替换成员，避免只加载一部分。
    const QString canonical = dataDir.trimmed().isEmpty() ? QString() : QFileInfo(dataDir).canonicalFilePath();
    if (canonical.isEmpty() || !QFileInfo(canonical).isDir()) {
        *error = QStringLiteral("数据目录不存在或无效：%1").arg(dataDir);
        return false;
    }
    const QDir dir(canonical);
    std::unique_ptr<QLockFile> candidate;
    if (!directoryLock_ || dataDir_ != canonical) {
        candidate = std::make_unique<QLockFile>(dir.filePath(directoryLockName));
        if (!acquireDirectoryLock(*candidate, canonical, error))
            return false;
    }
    // 恢复也会写文件，必须在持有目录锁后执行；失败时候选锁自动释放，旧状态保留。
    if (!recoverBorrowTransaction(dir, error))
        return false;
    BookRepository books;
    // 图书加载内部已完成字段校验、编号查重和哈希索引建立；后面可直接按编号核对引用。
    if (!books.load(dir.filePath(QStringLiteral("books.csv")), error))
        return false;

    QVector<QStringList> rows;
    if (!Csv::read(dir.filePath(QStringLiteral("users.csv")),
                   QStringLiteral("user_id,name"), 2, &rows, error))
        return false;
    QVector<User> users;
    // 用户集合既检查重复编号，也为评分及借阅提供引用存在性检查。
    QSet<QString> userIds;
    for (int i = 0; i < rows.size(); ++i) { // 第一组记录循环：将 users.csv 的每行变为 User，检查编号和重复。
        const User user{rows[i][0], rows[i][1]};
        if (!validId(user.id) || user.name.trimmed().isEmpty()
            || userIds.contains(user.id)) {
            *error = QStringLiteral("users.csv 第 %1 行数据无效或编号重复").arg(i + 2);
            return false;
        }
        userIds.insert(user.id);
        users.append(user);
    }

    if (!Csv::read(dir.filePath(QStringLiteral("ratings.csv")),
                   QStringLiteral("user_id,book_id,score"), 3, &rows, error))
        return false;
    QVector<Rating> ratings;
    // ratingKey 把用户编号与图书编号组成一对键，防止同一组合有多行评分。
    QSet<QString> ratingKeys;
    for (int i = 0; i < rows.size(); ++i) { // 第二组记录循环：将评分文本转为整数，并检查用户、图书引用及组合唯一性。
        bool scoreOk = false;
        const int score = rows[i][2].toInt(&scoreOk);
        const Rating rating{rows[i][0], rows[i][1], score};
        // 校验引用存在、评分为 1～5，且同一用户/图书组合不能在文件中重复。
        const QString key = ratingKey(rating.userId, rating.bookId);
        if (!userIds.contains(rating.userId) || !books.findById(rating.bookId)
            || !scoreOk || score < 1 || score > 5 || ratingKeys.contains(key)) {
            *error = QStringLiteral("ratings.csv 第 %1 行引用、评分或唯一性错误").arg(i + 2);
            return false;
        }
        ratingKeys.insert(key);
        ratings.append(rating);
    }

    if (!Csv::read(dir.filePath(QStringLiteral("borrow_records.csv")),
                   QStringLiteral("record_id,user_id,book_id,borrow_date,return_date,status"),
                   6, &rows, error))
        return false;
    QVector<BorrowRecord> records;
    QSet<QString> recordIds;
    // activeLoans 只记录未归还组合，已归还历史不阻止再次借阅。
    QSet<QString> activeLoans;
    for (int i = 0; i < rows.size(); ++i) { // 第三组记录循环：校验借阅编号、日期、状态和未归还组合，再收集完整历史。
        const BorrowRecord record{rows[i][0], rows[i][1], rows[i][2],
                                  rows[i][3], rows[i][4], rows[i][5]};
        const QDate borrowed = QDate::fromString(record.borrowDate, Qt::ISODate);
        const QDate returned = QDate::fromString(record.returnDate, Qt::ISODate);
        const bool datesOk = record.status == QStringLiteral("borrowed")
                                 ? record.returnDate.isEmpty()
                                 : record.status == QStringLiteral("returned")
                                       && returned.isValid() && returned >= borrowed;
        if (!validId(record.id) || recordIds.contains(record.id)
            || !userIds.contains(record.userId) || !books.findById(record.bookId)
            || !borrowed.isValid() || !datesOk
            || (record.status == QStringLiteral("borrowed")
                && activeLoans.contains(ratingKey(record.userId, record.bookId)))) {
            *error = QStringLiteral("borrow_records.csv 第 %1 行引用、日期、状态或重复借阅错误").arg(i + 2);
            return false;
        }
        recordIds.insert(record.id);
        if (record.status == QStringLiteral("borrowed"))
            activeLoans.insert(ratingKey(record.userId, record.bookId));
        records.append(record);
    }

    // 四类记录全通过校验后才更新全部成员；后续保存使用这个数据目录。
    dataDir_ = dir.absolutePath();
    books_ = books;
    users_ = users;
    ratings_ = ratings;
    borrowRecords_ = records;
    if (candidate)
        directoryLock_ = std::move(candidate);
    return true;
}

bool LibraryStore::hasUser(const QString &id) const
{
    // 用户目前顺序扫描，时间 O(U)；这与图书的哈希查询是不同路径。
    for (const User &user : users_) { // 顺序检查用户编号，找到即返回 true；这是用户存在性查询，不是图书哈希检索。
        if (user.id == id)
            return true;
    }
    return false;
}

bool LibraryStore::addBook(const Book &book, QString *error)
{
    if (!requireWriteAccess(error))
        return false;
    // 【曹振江重点：一次新增不等于只调用 insert】
    // book 是界面收集的完整实体；error 输出失败原因；true 表示内存与 books.csv 均已更新。
    // 库存输入框即使限制了非负数，仓库仍会检查字段及重复编号，保证其他调用入口也受约束。
    // 先备份内存，再新增并保存；文件保存失败恢复旧仓库，界面据返回值显示失败。
    // before 同时备份实体及索引；BookIndex 深拷贝使恢复时不会共享链表结点。
    const BookRepository before = books_;
    if (!books_.insert(book, error))
        return false;
    if (!books_.save(QDir(dataDir_).filePath(QStringLiteral("books.csv")), error)) {
        // 保存失败恢复内存，避免刷新界面时错误地显示本次新增已成功。
        books_ = before;
        return false;
    }
    return true;
}

bool LibraryStore::updateBook(const Book &book, QString *error)
{
    if (!requireWriteAccess(error))
        return false;
    // 【曹振江重点：保存失败为什么界面不能显示新库存】
    // before 保留旧实体和旧索引；update 改内存，save 改文件，任一保存失败就恢复旧仓库。
    // 对已有编号改字段不改变数组位置，因此通常不必重建索引，但备份中仍包含完整索引。
    // 图书修改采用与新增相同的“内存备份 -> 修改 -> 保存 -> 失败回滚”流程。
    const BookRepository before = books_;
    if (!books_.update(book, error))
        return false;
    if (!books_.save(QDir(dataDir_).filePath(QStringLiteral("books.csv")), error)) {
        books_ = before;
        return false;
    }
    return true;
}

bool LibraryStore::deleteBook(const QString &id, QString *error)
{
    if (!requireWriteAccess(error))
        return false;
    // 【曹振江重点：删除约束】界面确认框只确认用户意愿，是否允许删除由这里最终决定。
    // 检查顺序：评分引用 -> 所有借阅历史引用 -> 删除实体并更新索引 -> 保存，失败回滚。
    // 即使借阅记录已归还，也要保留其图书编号对应的实体，不能留下无效历史引用。
    // 引用检查会扫描评分和借阅记录，所以完整业务删除的耗时不只是一次哈希查询。
    // 保留引用完整性：有评分或任何借阅历史（包括已归还）的图书都不能删除。
    for (const Rating &rating : ratings_) { // 删除前的第一轮检查：遍历所有评分，找是否仍有记录引用待删图书。
        if (rating.bookId == id) { // 只要有一条评分引用就拒绝删除，避免推荐数据失去对应图书。
            *error = QStringLiteral("图书 %1 有评分记录，不能删除").arg(id);
            return false;
        }
    }
    for (const BorrowRecord &record : borrowRecords_) { // 第二轮检查全部借阅历史；已归还记录也会被遍历。
        if (record.bookId == id) { // 不按归还状态过滤，因此任何历史引用都会阻止删除。
            *error = QStringLiteral("图书 %1 有借阅记录，不能删除（含已归还记录）").arg(id);
            return false;
        }
    }
    const BookRepository before = books_;
    if (!books_.removeById(id, error))
        return false;
    if (!books_.save(QDir(dataDir_).filePath(QStringLiteral("books.csv")), error)) {
        books_ = before;
        return false;
    }
    return true;
}

bool LibraryStore::saveRatings(QString *error) const
{
    // 评分模块简述：将内存评分序列化为 ratings.csv，再交给通用 CSV 层保存。
    // 把评分实体按 user_id,book_id,score 转为 CSV，实际写入由 Csv::write 处理。
    QVector<QStringList> rows;
    for (const Rating &rating : ratings_)
        rows.append({rating.userId, rating.bookId, QString::number(rating.score)});
    return Csv::write(QDir(dataDir_).filePath(QStringLiteral("ratings.csv")),
                      QStringLiteral("user_id,book_id,score"), rows, error);
}

bool LibraryStore::rate(const QString &userId, const QString &bookId,
                        int score, QString *error)
{
    if (!requireWriteAccess(error))
        return false;
    // 先校验用户、图书和评分范围；已有组合更新分数，否则追加，保存失败回滚。
    // 此处保存评分记录；评分矩阵由 RecommendationService 在下一次推荐时重新建立。
    if (!hasUser(userId) || !books_.findById(bookId) || score < 1 || score > 5) {
        *error = QStringLiteral("用户、图书不存在或评分不在 1～5 范围内");
        return false;
    }
    const QVector<Rating> before = ratings_;
    // found 区分“更新”与“新增”，保证同一用户/图书组合只有一条评分记录。
    bool found = false;
    for (Rating &rating : ratings_) {
        if (rating.userId == userId && rating.bookId == bookId) {
            // 引用遍历直接修改评分；命中后结束，不再追加重复记录。
            rating.score = score;
            found = true;
            break;
        }
    }
    // 没有原记录才追加，新评分会在下一次构造矩阵时填到对应行列。
    if (!found)
        ratings_.append({userId, bookId, score});
    if (!saveRatings(error)) {
        // 保存失败恢复原评分数组；界面接到 false 后显示错误，不刷新成功状态。
        ratings_ = before;
        return false;
    }
    return true;
}

bool LibraryStore::borrowBook(const QString &userId, const QString &bookId,
                              const QString &borrowDate, QString *recordId, QString *error)
{
    if (!requireWriteAccess(error))
        return false;
    // 借出要求库存充足、日期合法且没有同用户同图书的活动借阅；编号按现有最大 R 编号续增。
    const QDate date = QDate::fromString(borrowDate, Qt::ISODate);
    const Book *current = books_.findById(bookId);
    if (!hasUser(userId) || !current) {
        *error = QStringLiteral("用户或图书编号不存在");
        return false;
    }
    if (!date.isValid() || date.toString(Qt::ISODate) != borrowDate) {
        *error = QStringLiteral("借出日期必须是有效的 YYYY-MM-DD");
        return false;
    }
    if (current->stock <= 0) {
        *error = QStringLiteral("图书 %1 库存为零，不能借出").arg(bookId);
        return false;
    }
    quint64 highest = 0;
    int width = 3;
    for (const BorrowRecord &record : borrowRecords_) {
        if (record.userId == userId && record.bookId == bookId
            && record.status == QStringLiteral("borrowed")) {
            *error = QStringLiteral("该用户已有这本书的未归还记录");
            return false;
        }
        if (record.id.startsWith('R') && record.id.size() > 1) {
            const QString digits = record.id.mid(1);
            bool ok = false;
            const quint64 number = digits.toULongLong(&ok);
            if (ok) {
                highest = qMax(highest, number);
                width = qMax(width, int(digits.size()));
            }
        }
    }
    if (highest == std::numeric_limits<quint64>::max()) {
        *error = QStringLiteral("借阅编号已用尽");
        return false;
    }
    const QString newId = QStringLiteral("R")
                          + QString::number(highest + 1).rightJustified(width, '0');
    const BookRepository booksBefore = books_;
    const QVector<BorrowRecord> recordsBefore = borrowRecords_;
    Book updated = *current;
    --updated.stock;
    books_.update(updated, error);
    borrowRecords_.append({newId, userId, bookId, borrowDate, {}, QStringLiteral("borrowed")});
    if (!saveBorrowAndBooks(QDir(dataDir_), books_, borrowRecords_, error)) {
        books_ = booksBefore;
        borrowRecords_ = recordsBefore;
        return false;
    }
    if (recordId)
        *recordId = newId;
    return true;
}

bool LibraryStore::returnBook(const QString &recordId, const QString &returnDate,
                              QString *error)
{
    if (!requireWriteAccess(error))
        return false;
    // 仅未归还记录可归还；日期不能早于借出日期（允许当天），同时恢复一份可借库存。
    const QDate date = QDate::fromString(returnDate, Qt::ISODate);
    if (!date.isValid() || date.toString(Qt::ISODate) != returnDate) {
        *error = QStringLiteral("归还日期必须是有效的 YYYY-MM-DD");
        return false;
    }
    for (int i = 0; i < borrowRecords_.size(); ++i) {
        const BorrowRecord &record = borrowRecords_[i];
        if (record.id != recordId)
            continue;
        if (record.status != QStringLiteral("borrowed")) {
            *error = QStringLiteral("借阅记录 %1 已归还，不能重复归还").arg(recordId);
            return false;
        }
        if (date < QDate::fromString(record.borrowDate, Qt::ISODate)) {
            *error = QStringLiteral("归还日期不能早于借出日期");
            return false;
        }
        const Book *current = books_.findById(record.bookId);
        if (!current || current->stock == std::numeric_limits<int>::max()) {
            *error = QStringLiteral("图书不存在或库存无法增加");
            return false;
        }
        const BookRepository booksBefore = books_;
        const QVector<BorrowRecord> recordsBefore = borrowRecords_;
        Book updated = *current;
        ++updated.stock;
        books_.update(updated, error);
        borrowRecords_[i].returnDate = returnDate;
        borrowRecords_[i].status = QStringLiteral("returned");
        if (!saveBorrowAndBooks(QDir(dataDir_), books_, borrowRecords_, error)) {
            books_ = booksBefore;
            borrowRecords_ = recordsBefore;
            return false;
        }
        return true;
    }
    *error = QStringLiteral("借阅记录 %1 不存在").arg(recordId);
    return false;
}
