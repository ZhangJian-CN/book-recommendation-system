#include "booksearch.h"
#include "bookrepository.h"

bool BookSearch::containsKmp(const QString &text, const QString &pattern)
{
    if (pattern.isEmpty())
        return true;
    QVector<int> prefix(pattern.size());
    for (int i = 1, matched = 0; i < pattern.size(); ++i) {
        while (matched > 0 && pattern[i] != pattern[matched])
            matched = prefix[matched - 1];
        if (pattern[i] == pattern[matched])
            ++matched;
        prefix[i] = matched;
    }
    for (int i = 0, matched = 0; i < text.size(); ++i) {
        while (matched > 0 && text[i] != pattern[matched])
            matched = prefix[matched - 1];
        if (text[i] == pattern[matched])
            ++matched;
        if (matched == pattern.size())
            return true;
    }
    return false;
}

QVector<int> BookSearch::keywordRows(const QVector<Book> &books, const QString &keyword)
{
    QVector<int> rows;
    const QString pattern = keyword.trimmed().toCaseFolded();
    if (pattern.isEmpty())
        return rows;
    for (int row = 0; row < books.size(); ++row) {
        if (containsKmp(books[row].title.toCaseFolded(), pattern)
            || containsKmp(books[row].author.toCaseFolded(), pattern))
            rows.append(row);
    }
    return rows;
}
