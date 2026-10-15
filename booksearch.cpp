// KMP。先构造模式串前缀表，再扫描书名或作者字符串。
#include "booksearch.h"
#include "bookrepository.h"

bool BookSearch::containsKmp(const QString &text, const QString &pattern)
{
    // 输入 text（书名/作者）、pattern（关键词）；输出是否包含该连续子串。
    // 单次匹配：正文长 n、模式串长 m，时间 O(n+m)，前缀表空间 O(m)。
    if (pattern.isEmpty())
        return true;
    // prefix[i] 是 pattern[0..i] 的最长相等真前缀、真后缀长度。
    // matched 表示当前已匹配长度；失配时沿 prefix 回退，避免从头比较。
    // “真”表示不能把整个子串本身当作前缀；例 ABABAC 的 prefix 为 [0,0,1,2,3,0]。
    QVector<int> prefix(pattern.size());
    for (int i = 1, matched = 0; i < pattern.size(); ++i) {
        // 构造前缀表：把当前字符接在已有前缀后；不相等就试更短的相等前后缀。
        while (matched > 0 && pattern[i] != pattern[matched])
            matched = prefix[matched - 1];
        // 相等则匹配长度加 1；仍不相等且 matched=0 时，保持长度为 0。
        if (pattern[i] == pattern[matched])
            ++matched;
        // 保存该位置的回退依据，后续匹配和前缀表构造都可复用。
        prefix[i] = matched;
    }
    // 正文下标 i 始终向前；失配只改变 matched，不让正文指针后退。
    // 例：已匹配 ABABA（长度 5），下一个正文字符为 B，但模式下一位为 C。
    // 退到 prefix[4]=3 后比较模式下标 3 的 B，匹配长度变为 4，无需重扫正文。
    for (int i = 0, matched = 0; i < text.size(); ++i) {
        while (matched > 0 && text[i] != pattern[matched])
            matched = prefix[matched - 1];
        if (text[i] == pattern[matched])
            ++matched;
        // matched 达到模式长度，说明整个关键词已连续命中，立即结束本次匹配。
        if (matched == pattern.size())
            return true;
    }
    // 扫完整个正文仍未命中完整模式，返回 false。
    return false;
}

QVector<int> BookSearch::keywordRows(const QVector<Book> &books, const QString &keyword)
{
    // 去除关键词两端空白，并统一大小写；空关键词在检索入口返回空结果。
    QVector<int> rows;
    const QString pattern = keyword.trimmed().toCaseFolded();
    // 空关键词不触发“显示全部”；显示全部是主界面中单独的按钮操作。
    if (pattern.isEmpty())
        return rows;
    // 逐本检查书名或作者，任意一项包含关键词就返回该书的数组下标。
    // 当前实现对每个被检查的字符串分别调用 KMP，前缀表也在各次调用中构造。
    // || 表示书名、作者任意一项命中即可；书名命中时短路，不再检查该书作者。
    for (int row = 0; row < books.size(); ++row) {
        if (containsKmp(books[row].title.toCaseFolded(), pattern)
            || containsKmp(books[row].author.toCaseFolded(), pattern))
            rows.append(row);
    }
    return rows;
}
