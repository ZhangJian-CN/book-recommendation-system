#include "librarystore.h"
#include "recommendation.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    for (const QString &dir : {QStringLiteral(SAMPLE_LARGE_DATA_DIR),
                               QStringLiteral(SAMPLE_GOODBOOKS_DATA_DIR)}) {
        QString error;
        LibraryStore store;
        if (!store.load(dir, &error))
            return 1;
        if (store.books().all().size() != 100 || store.users().size() != 40
            || store.ratings().size() != 800 || store.borrowRecords().size() != 10)
            return 2;

        RecommendationService service(store.books(), store.users(), store.ratings());
        const auto top = service.recommend(QStringLiteral("U00"), 5);
        if (top.size() != 5)
            return 3;
        for (const Recommendation &item : top) {
            if (!store.books().findById(item.bookId)
                || store.books().findById(item.bookId)->stock <= 0)
                return 4;
        }
        if (dir == QStringLiteral(SAMPLE_GOODBOOKS_DATA_DIR)) {
            const Book *first = store.books().findById(QStringLiteral("B001"));
            if (!first || first->title
                              != QStringLiteral("The Hunger Games (The Hunger Games, #1)"))
                return 5;
        }
    }
    return 0;
}
