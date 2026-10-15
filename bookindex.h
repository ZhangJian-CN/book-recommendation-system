#ifndef BOOKINDEX_H
#define BOOKINDEX_H

#include <QString>
#include <QVector>
#include <memory>
#include <vector>

struct Book;

class BookIndex {
public:
    explicit BookIndex(int bucketCount = 101);
    BookIndex(const BookIndex &other);
    BookIndex &operator=(const BookIndex &other);
    BookIndex(BookIndex &&) = default;
    BookIndex &operator=(BookIndex &&) = default;
    void build(const QVector<Book> &books);
    void insert(const QString &id, int row);
    void erase(const QString &id);
    int rowForId(const QString &id) const;
    int bucketForId(const QString &id) const;
    int bucketSize(int bucket) const;

private:
    struct Node {
        QString id;
        int row;
        std::unique_ptr<Node> next;
    };
    std::vector<std::unique_ptr<Node>> buckets_;
};

#endif // BOOKINDEX_H
