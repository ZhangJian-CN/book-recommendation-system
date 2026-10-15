#ifndef CSVUTIL_H
#define CSVUTIL_H

#include <QString>
#include <QStringList>
#include <QVector>

namespace Csv {
bool read(const QString &path, const QString &header, int columns,
          QVector<QStringList> *rows, QString *error);
bool write(const QString &path, const QString &header,
           const QVector<QStringList> &rows, QString *error);
}

#endif // CSVUTIL_H
