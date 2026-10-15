#ifndef BOOKSEARCH_H
#define BOOKSEARCH_H

#include <QString>
#include <QVector>

//KMP算法：书名和作者关键字索引

struct Book;

// 关键词检索返回图书数组下标；具体字符串匹配使用下面的 KMP 函数。
class BookSearch {
public:
    // 输入图书数组及关键词；输出书名或作者命中的原数组下标，保持原图书顺序。
    static QVector<int> keywordRows(const QVector<Book> &books, const QString &keyword);

private:
    // text 为正文，pattern 为模式；找到连续子串返回 true，否则 false。
    static bool containsKmp(const QString &text, const QString &pattern);
};

#endif // BOOKSEARCH_H
