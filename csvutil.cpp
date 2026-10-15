// CSV 读写。UTF-8 文本 -> 字段数组 -> 业务层记录；保存时执行反向转换。
#include "csvutil.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

//具体怎么读写CSV文件

namespace {

bool parseLine(const QString &line, QStringList *fields)
{
    // 输入一行文本，输出拆出的字段；返回 false 表示引号格式不合法。
    // 状态解析一行：引号外逗号分列，引号内逗号属于内容，两个双引号表示一个双引号。
    // closedQuote 检查闭合引号后只允许分隔符或行结束，未闭合引号视为格式错误。
    // field 缓存当前列，quoted 表示位于引号内，closedQuote 表示本列引号已闭合。
    QString field;
    bool quoted = false;
    bool closedQuote = false;
    fields->clear();
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    // 引号内连续两个双引号：保存一个字面双引号，并跳过另一个。
                    field += '"';
                    ++i;
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
            field.clear();
            closedQuote = false;
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
    while (!in.atEnd()) {
        QStringList fields;
        const QString line = in.readLine();
        ++lineNumber;
        if (!parseLine(line, &fields) || fields.size() != columns) {
            // 语法合法还不够，列数也必须符合约定；否则整次读取失败。
            *error = QStringLiteral("%1 第 %2 行 CSV 格式错误").arg(path).arg(lineNumber);
            return false;
        }
        loaded.append(fields);
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
    for (const QStringList &row : rows) {
        QStringList escaped;
        for (const QString &field : row) {
            if (field.contains('\n') || field.contains('\r')) {
                // 当前 CSV 按单行记录解析，不支持字段内换行，保存时明确拒绝。
                *error = QStringLiteral("CSV 字段不能包含换行符");
                return false;
            }
            escaped.append(escape(field));
        }
        out << escaped.join(',') << '\n';
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
