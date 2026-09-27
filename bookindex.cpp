#include "bookindex.h"
#include "bookrepository.h"

#include <algorithm>

BookIndex::BookIndex(int bucketCount)
    : buckets_(std::max(1, bucketCount))
{
}

BookIndex::BookIndex(const BookIndex &other)
    : buckets_(other.buckets_.size())
{
    for (const auto &bucket : other.buckets_) {
        for (const Node *node = bucket.get(); node; node = node->next.get())
            insert(node->id, node->row);
    }
}

BookIndex &BookIndex::operator=(const BookIndex &other)
{
    if (this != &other) {
        BookIndex copy(other);
        buckets_ = std::move(copy.buckets_);
    }
    return *this;
}

int BookIndex::bucketForId(const QString &id) const
{
    quint64 hash = 0;
    for (const QChar ch : id)
        hash = hash * 31 + ch.unicode();
    return int(hash % quint64(buckets_.size()));
}

int BookIndex::bucketSize(int bucket) const
{
    if (bucket < 0 || bucket >= int(buckets_.size()))
        return 0;
    int count = 0;
    for (const Node *node = buckets_[bucket].get(); node; node = node->next.get())
        ++count;
    return count;
}

void BookIndex::build(const QVector<Book> &books)
{
    for (auto &bucket : buckets_)
        bucket.reset();
    for (int row = 0; row < books.size(); ++row)
        insert(books[row].id, row);
}

void BookIndex::insert(const QString &id, int row)
{
    const int bucket = bucketForId(id);
    auto node = std::make_unique<Node>();
    node->id = id;
    node->row = row;
    node->next = std::move(buckets_[bucket]);
    buckets_[bucket] = std::move(node);
}

void BookIndex::erase(const QString &id)
{
    auto *link = &buckets_[bucketForId(id)];
    while (link->get()) {
        if ((*link)->id == id) {
            *link = std::move((*link)->next);
            return;
        }
        link = &(*link)->next;
    }
}

int BookIndex::rowForId(const QString &id) const
{
    for (const Node *node = buckets_[bucketForId(id)].get(); node;
         node = node->next.get()) {
        if (node->id == id)
            return node->row;
    }
    return -1;
}
