#include "csvutil.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

namespace {

bool parseLine(const QString &line, QStringList *fields)
{
    QString field;
    bool quoted = false;
    bool closedQuote = false;
    fields->clear();
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    quoted = false;
                    closedQuote = true;
                }
            } else {
                field += ch;
            }
        } else if (ch == ',') {
            fields->append(field);
            field.clear();
            closedQuote = false;
        } else if (ch == '"' && field.isEmpty() && !closedQuote) {
            quoted = true;
        } else if (closedQuote || ch == '"') {
            return false;
        } else {
            field += ch;
        }
    }
    if (quoted)
        return false;
    fields->append(field);
    return true;
}

QString escape(const QString &field)
{
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
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("无法读取 %1：%2").arg(path, file.errorString());
        return false;
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    if (in.readLine() != header) {
        *error = QStringLiteral("%1 表头不符合约定").arg(path);
        return false;
    }
    QVector<QStringList> loaded;
    int lineNumber = 1;
    while (!in.atEnd()) {
        QStringList fields;
        const QString line = in.readLine();
        ++lineNumber;
        if (!parseLine(line, &fields) || fields.size() != columns) {
            *error = QStringLiteral("%1 第 %2 行 CSV 格式错误").arg(path).arg(lineNumber);
            return false;
        }
        loaded.append(fields);
    }
    if (in.status() != QTextStream::Ok) {
        *error = QStringLiteral("读取 %1 时发生错误").arg(path);
        return false;
    }
    *rows = loaded;
    return true;
}

bool Csv::write(const QString &path, const QString &header,
                const QVector<QStringList> &rows, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        *error = QStringLiteral("无法保存 %1：%2").arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << header << '\n';
    for (const QStringList &row : rows) {
        QStringList escaped;
        for (const QString &field : row) {
            if (field.contains('\n') || field.contains('\r')) {
                *error = QStringLiteral("CSV 字段不能包含换行符");
                return false;
            }
            escaped.append(escape(field));
        }
        out << escaped.join(',') << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        *error = QStringLiteral("写入 %1 失败：%2").arg(path, file.errorString());
        return false;
    }
    return true;
}
