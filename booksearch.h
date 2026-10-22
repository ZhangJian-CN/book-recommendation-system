#ifndef BOOKSEARCH_H
#define BOOKSEARCH_H

#include <QString>
#include <QVector>
//KMP 字符串匹配：建立前缀表，利用失配回退检索书名和作者
// 本类不保存永久的关键词索引，而是在用户检索时扫描当前图书的书名和作者。
// 调用方得到原图书数组的下标，再交给界面显示，不直接在算法中操作表格。

struct Book; // 前置声明 Book 类型；函数只需声明接收图书数组，完整实体定义由实现文件包含。

// 关键词检索返回图书数组下标；具体字符串匹配使用下面的 KMP 函数。
class BookSearch { // class 封装图书检索方法；本类不保存状态，也不直接操作界面控件。
public: // 公有入口：界面通过它统一处理关键词，再获得要展示的实体数组下标。
    static QVector<int> keywordRows(const QVector<Book> &books, const QString &keyword); // 检索书名或作者，返回命中的数组下标；static 允许用类名直接调用。

private: // 私有辅助函数：外部不直接调用，保证用户输入先经过 keywordRows 的统一规则。
    static bool containsKmp(const QString &text, const QString &pattern); // 用 KMP 判断正文是否包含模式串，命中返回 true；不返回界面行号。
};

#endif // BOOKSEARCH_H
