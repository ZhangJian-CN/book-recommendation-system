#ifndef CSVUTIL_H
#define CSVUTIL_H

#include <QString>
#include <QStringList>
#include <QVector>

// 文件中的每行先变成 QStringList，再由 BookRepository/LibraryStore 转成具体实体。
// 本层判断“格式能否读写”，上层判断“编号、库存、评分和引用是否符合业务规则”。

// 通用 CSV 层只处理表头、列数及文本字段；实体类型和业务约束由上层校验。
// read 成功才更新 rows；read/write 失败返回 false 并把原因写入 error。
namespace Csv { // namespace 是命名空间，不是 class；用 Csv::read/write 区分同名函数，不需要创建对象。
// 返回 bool 表示是否成功；error 是错误文本输出参数，调用方负责传入有效地址。
// path=文件路径，header=必须匹配的表头，columns=每行字段数。
// rows 是输出参数；error 接收失败原因，调用方需传入有效指针。
bool read(const QString &path, const QString &header, int columns, // 输入路径、预期表头和每行列数。
          QVector<QStringList> *rows, QString *error); // 读取并校验 CSV；成功时把整份字段数组交给 rows，失败原因写入 error。
// 将 rows 按 header 写入 path；UTF-8 编码并处理引号，成功提交后返回 true。
bool write(const QString &path, const QString &header, // 输入目标路径和约定表头。
           const QVector<QStringList> &rows, QString *error); // 转义并写入所有字段，提交成功返回 true；失败原因写入 error。
}

#endif // CSVUTIL_H
