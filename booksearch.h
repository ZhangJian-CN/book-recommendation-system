#ifndef BOOKSEARCH_H
#define BOOKSEARCH_H

#include <QString>
#include <QVector>

struct Book;

class BookSearch {
public:
    static QVector<int> keywordRows(const QVector<Book> &books, const QString &keyword);

private:
    static bool containsKmp(const QString &text, const QString &pattern);
};

#endif // BOOKSEARCH_H
