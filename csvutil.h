#ifndef CSVUTIL_H
#define CSVUTIL_H

#include <QString>
#include <QStringList>
#include <QVector>

//怎么调用CSV文件

// 通用 CSV 层只处理表头、列数及文本字段；实体类型和业务约束由上层校验。
// read 成功才更新 rows；read/write 失败返回 false 并把原因写入 error。
namespace Csv {
// path=文件路径，header=必须匹配的表头，columns=每行字段数。
// rows 是输出参数；error 接收失败原因，调用方需传入有效指针。
bool read(const QString &path, const QString &header, int columns,//读取文件，把解析出的字段交给调用方。
          QVector<QStringList> *rows, QString *error);
// 将 rows 按 header 写入 path；UTF-8 编码并处理引号，成功提交后返回 true。
bool write(const QString &path, const QString &header,//把调用方提供的字段写入文件。
           const QVector<QStringList> &rows, QString *error);
}

#endif // CSVUTIL_H
