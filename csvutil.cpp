// 阅读顺序：parseLine 拆一行 -> read 读整份 -> escape 转义字段 -> write 提交文件。
// 这里保存的是普通 UTF-8 文本；并非数据库，也不存哈希桶或推荐矩阵等可重建的内存结构。
#include "csvutil.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

//具体怎么读写CSV文件

namespace {

bool parseLine(const QString &line, QStringList *fields)
{
    // 为什么不用 split(',')？书名“算法,图解”被引号包住时，中间逗号不是列分隔符。
    // 三种情况要区分：引号内逗号保留、引号外逗号分列、两个连续引号转成一个字面引号。
    // 输入一行文本，输出拆出的字段；返回 false 表示引号格式不合法。
    // closedQuote 检查闭合引号后只允许分隔符或行结束，未闭合引号视为格式错误。
    QString field; // 缓存正在解析的这一列，遇到引号外逗号时才提交到 fields。
    bool quoted = false; // 是否正处在引号包围的字段中，决定逗号应作内容还是分隔符。
    bool closedQuote = false; // 是否刚关闭一个引号字段，用于拒绝闭合后夹带其他文字。
    fields->clear(); // 输出参数先清空，随后按字段顺序追加。
    for (qsizetype i = 0; i < line.size(); ++i) { // 逐字符推进，以两个状态变量解释每个字符的意义。
        const QChar ch = line[i]; // 当前字符；这里只观察原文本，不改变传入的 line。
        if (quoted) { // 引号还开着，逗号可作为普通字段内容保留。
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    // 引号内连续两个双引号：保存一个字面双引号，并跳过另一个。
                    field += '"';
                    ++i; // 手动跳过第二个双引号，外层 ++i 随后继续到它后面的字符。
                } else {
                    // 单个双引号结束引号字段，之后只允许逗号或行结束。
                    quoted = false;
                    closedQuote = true;
                }
            } else {
                field += ch;
            }
        } else if (ch == ',') {
            // 引号外逗号结束当前列；即使 field 为空也要保存，不能丢失空字段。
            fields->append(field);
            field.clear(); // 当前列已交付，复用缓冲区开始收集下一列。
            closedQuote = false; // 新的一列不继承上一列“引号已经闭合”的状态。
        } else if (ch == '"' && field.isEmpty() && !closedQuote) {
            // 双引号只能出现在本列开头，表示这列使用引号包装。
            quoted = true;
        } else if (closedQuote || ch == '"') {
            // 闭合引号后还有普通字符，或未包装字段中出现双引号，都拒绝解析。
            return false;
        } else {
            field += ch;
        }
    }
    if (quoted)
        return false;
    // 行末没有逗号也要加入最后一列；行末逗号会产生合法的空末列。
    fields->append(field);
    return true;
}

QString escape(const QString &field)
{
    // 这是 parseLine 的反向处理；读写要配对，不能读取支持逗号而保存时直接拼接原文。
    // 含逗号或双引号的字段加外层引号，内部双引号加倍，保证保存后能正确重读。
    // 例：算法,图解 写成 "算法,图解"；字符内容 A"B 写成 "A""B"。
    if (!field.contains(',') && !field.contains('"'))
        return field;
    QString value = field;
    value.replace('"', "\"\"");
    return '"' + value + '"';
}

} // namespace

bool Csv::read(const QString &path, const QString &header, int columns,
               QVector<QStringList> *rows, QString *error)
{
    // header 是预期的完整表头，columns 是约定列数，rows 是“读完并校验通过”的输出。
    // loaded 暂存数据的意义：第 20 行格式错误时，不能把前 19 行误当成一次完整加载成功。
    // 读取后先核对约定表头，再逐行检查字段数量；统一 UTF-8 保留中文内容。
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("无法读取 %1：%2").arg(path, file.errorString());
        return false;
    }
    QTextStream in(&file);
    // 文本流负责字节与 QString 的转换；显式指定 UTF-8 保留中文内容。
    in.setEncoding(QStringConverter::Utf8);
    if (in.readLine() != header) {
        *error = QStringLiteral("%1 表头不符合约定").arg(path);
        return false;
    }
    // 暂存所有行，整份文件检查成功后再交给调用方，避免失败时返回半份数据。
    QVector<QStringList> loaded;
    int lineNumber = 1;
    // 第 1 行是表头，循环先递增，因此首条数据对应文件第 2 行。
    while (!in.atEnd()) { // 持续读取剩余数据行；是否停止取决于文件末尾，不预设记录数。
        QStringList fields; // 每轮新建这一行的字段容器，避免混入上一行数据。
        const QString line = in.readLine(); // 读一整行后交给引号状态解析器处理。
        ++lineNumber; // 物理文件行号，用于向用户准确指出坏数据的位置。
        if (!parseLine(line, &fields) || fields.size() != columns) {
            // 语法合法还不够，列数也必须符合约定；否则整次读取失败。
            *error = QStringLiteral("%1 第 %2 行 CSV 格式错误").arg(path).arg(lineNumber);
            return false;
        }
        loaded.append(fields); // 只有语法和列数都合法的完整行，才能进入本次候选结果。
    }
    if (in.status() != QTextStream::Ok) {
        *error = QStringLiteral("读取 %1 时发生错误").arg(path);
        return false;
    }
    // 全部校验成功才通过输出参数交付数据，失败路径不返回半份数据。
    *rows = loaded;
    return true;
}

bool Csv::write(const QString &path, const QString &header,
                const QVector<QStringList> &rows, QString *error)
{
    // rows 已由上层按字段顺序序列化。本层仍检查自身不支持的字段内换行。
    // “写到临时文件”和“提交替换”分开；单个文件采用 QSaveFile，借还两文件另有恢复流程。
    // QSaveFile 先写临时文件，最后 commit 才替换目标文件，减少直接覆盖造成的数据损坏。
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        *error = QStringLiteral("无法保存 %1：%2").arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    // 先写固定表头，每个 QStringList 对应一条记录；字段转义后用逗号连接。
    out << header << '\n';
    for (const QStringList &row : rows) { // 外层每轮处理一条记录，最终输出文件中的一整行。
        QStringList escaped; // 暂存本行已经转义的字段，不修改调用方提供的原字段。
        for (const QString &field : row) { // 内层逐列检查和转义，顺序与上层给定的字段顺序相同。
            if (field.contains('\n') || field.contains('\r')) {
                // 当前 CSV 按单行记录解析，不支持字段内换行，保存时明确拒绝。
                *error = QStringLiteral("CSV 字段不能包含换行符");
                return false;
            }
            escaped.append(escape(field)); // 对含逗号/双引号的字段加包装，保证重读后仍是一列。
        }
        out << escaped.join(',') << '\n'; // 字段间插入分隔逗号，记录末尾加换行。
    }
    // 先检查文本流写入状态，再提交文件；失败原因返回业务层用于回滚和界面提示。
    // flush 把文本缓冲写入文件设备；commit 提交临时文件，两者作用不同。
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        *error = QStringLiteral("写入 %1 失败：%2").arg(path, file.errorString());
        return false;
    }
    return true;
}
