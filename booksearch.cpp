// 第一段只看模式串，构造失配后可以退到哪里；第二段扫描正文并利用这个表跳转。
// 讲解时分别说明 i 是“正在读哪个字符”，matched 是“模式前面已经连续匹配了多长”。
#include "booksearch.h"
#include "bookrepository.h"

bool BookSearch::containsKmp(const QString &text, const QString &pattern)
{
    // 这个底层函数把空模式视为可匹配；用户输入空关键词时，上层 keywordRows 会提前返回。
    // 因而不要把此处的 true 讲成“点击空关键词会显示全部图书”。
    // 输入 text（书名/作者）、pattern（关键词）；输出是否包含该连续子串。
    // 单次匹配：正文长 n、模式串长 m，时间 O(n+m)，前缀表空间 O(m)。
    if (pattern.isEmpty())
        return true;
    // prefix[i] 是 pattern[0..i] 的最长相等真前缀、真后缀长度。
    // matched 表示当前已匹配长度；失配时沿 prefix 回退，避免从头比较。
    // “真”表示不能把整个子串本身当作前缀；例 ABABAC 的 prefix 为 [0,0,1,2,3,0]。
    QVector<int> prefix(pattern.size()); // 建立与模式等长的整数数组，元素初值为 0。
    // prefix[0] 初始为 0：单个字符没有更短的非空相等真前后缀，所以从 i=1 开始。
    for (int i = 1, matched = 0; i < pattern.size(); ++i) { // 逐步求出 prefix[1..m-1]；matched 沿循环保留当前可接续长度。
        // 构造前缀表：把当前字符接在已有前缀后；不相等就试更短的相等前后缀。
        while (matched > 0 && pattern[i] != pattern[matched]) // 当前前缀接不上字符时，可能需要连续尝试多个更短前缀。
            matched = prefix[matched - 1]; // 从“已匹配部分的最后一格”查回退长度，而不是简单减一重试。
        // 相等则匹配长度加 1；仍不相等且 matched=0 时，保持长度为 0。
        if (pattern[i] == pattern[matched]) // 已能接上，或已经退到 0 后首字符相同。
            ++matched; // 当前相等真前后缀可以再延长一个字符。
        // 保存该位置的回退依据，后续匹配和前缀表构造都可复用。
        prefix[i] = matched; // 保存此位置的最终长度，供后续位置和正文匹配复用。
    }
    // 正文下标 i 始终向前；失配只改变 matched，不让正文指针后退。
    // 例：已匹配 ABABA（长度 5），下一个正文字符为 B，但模式下一位为 C。
    // 退到 prefix[4]=3 后比较模式下标 3 的 B，匹配长度变为 4，无需重扫正文。
    for (int i = 0, matched = 0; i < text.size(); ++i) { // 每轮消费一个正文字符；i 不因失配回退。
        // 不匹配时，正文字符还没有消费；先缩短已有匹配，再尝试让同一正文字符继续匹配。
        // 每次回退依据已算好的 prefix，而不是把正文 i 拉回旧位置重新扫描。
        while (matched > 0 && text[i] != pattern[matched]) // 失配时，当前正文字符还可尝试匹配更短前缀的下一位。
            matched = prefix[matched - 1]; // 跳过已经知道不可能成功的模式对齐位置，不重扫旧正文。
        if (text[i] == pattern[matched]) // 当前正文字符与模式下一位一致。
            ++matched; // 已连续命中的模式长度加一。
        // matched 达到模式长度，说明整个关键词已连续命中，立即结束本次匹配。
        if (matched == pattern.size()) // 匹配长度达到模式长度，整个关键词已经命中。
            return true; // 立即结束，既避免额外扫描，也避免下一轮读取 pattern[m]。
    }
    // 扫完整个正文仍未命中完整模式，返回 false。
    return false;
}

QVector<int> BookSearch::keywordRows(const QVector<Book> &books, const QString &keyword)
{
    // 参数 books 是只读引用，避免为一次检索复制全部实体；rows 仅保存命中的整数下标。
    // 大小写折叠作用于比较用副本，原始英文书名与作者的显示内容保持原样。
    // 去除关键词两端空白，并统一大小写；空关键词在检索入口返回空结果。
    QVector<int> rows; // 结果容器仅保存命中下标，后续界面再按下标读取图书实体。
    const QString pattern = keyword.trimmed().toCaseFolded(); // 统一关键词两端空白和大小写，原书名不被改写。
    // 空关键词不触发“显示全部”；显示全部是主界面中单独的按钮操作。
    if (pattern.isEmpty())
        return rows;
    // 逐本检查书名或作者，任意一项包含关键词就返回该书的数组下标。
    // 当前实现对每个被检查的字符串分别调用 KMP，前缀表也在各次调用中构造。
    // || 表示书名、作者任意一项命中即可；书名命中时短路，不再检查该书作者。
    for (int row = 0; row < books.size(); ++row) { // 从第一本遍历到最后一本，因而结果保持实体数组顺序。
        // 同一本书即使书名和作者都命中也只追加一次，因为两个条件合成一个 if。
        if (containsKmp(books[row].title.toCaseFolded(), pattern)
            || containsKmp(books[row].author.toCaseFolded(), pattern))
            rows.append(row); // 命中任一字段只追加一次；这里保存原下标，不生成新的图书编号。
    }
    return rows;
}
