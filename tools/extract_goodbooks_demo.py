"""Extract a reproducible 100-book, 40-user demo from Goodbooks-10k v1.0."""

import argparse
import csv
import hashlib
from collections import Counter, defaultdict
from io import TextIOWrapper
from pathlib import Path
from zipfile import ZipFile


SOURCE_HASHES = {
    "books.zip": "1e5e29f4037f2c4ccd99cb5f10d07316ec0e3c0fcefbcdf9698585c418f7bd4f",
    "ratings.zip": "2dd0ea8e800860279d7305e88a481ff12090fe5bb54b1321a3854a3b651ebd2e",
}
OUTPUT = Path(__file__).resolve().parents[1] / "demo_data" / "goodbooks"


def read_source(path, member):
    with ZipFile(path) as archive:
        with archive.open(member) as data:
            yield from csv.DictReader(TextIOWrapper(data, encoding="utf-8-sig"))


def write_csv(name, header, rows):
    with (OUTPUT / name).open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source_dir", type=Path, help="directory containing books.zip and ratings.zip")
args = parser.parse_args()
for name, expected in SOURCE_HASHES.items():
    digest = hashlib.sha256()
    with (args.source_dir / name).open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    actual = digest.hexdigest()
    if actual != expected:
        raise ValueError(f"{name} is not the verified Goodbooks-10k v1.0 release asset")

books = {}
for row in read_source(args.source_dir / "books.zip", "books.csv"):
    book_id = int(row["book_id"])
    if 1 <= book_id <= 100:
        books[book_id] = (row["title"], row["authors"])
if len(books) != 100:
    raise ValueError("source is missing one of the first 100 books")

ratings_by_user = defaultdict(dict)
for row in read_source(args.source_dir / "ratings.zip", "ratings.csv"):
    book_id = int(row["book_id"])
    if book_id > 100:
        continue
    user_id = int(row["user_id"])
    score = int(row["rating"])
    if book_id in ratings_by_user[user_id] or not 1 <= score <= 5:
        raise ValueError("source contains a duplicate or invalid rating")
    ratings_by_user[user_id][book_id] = score

source_users = sorted(ratings_by_user, key=lambda u: (-len(ratings_by_user[u]), u))[:40]
shared = set(range(1, 101))
for user in source_users:
    shared.intersection_update(ratings_by_user[user])
anchors = sorted(shared)[:3]
if len(source_users) != 40 or len(anchors) != 3:
    raise ValueError("source lacks 40 users with three shared books")

chosen = {user: set(anchors) for user in source_users}
frequency = Counter({book: 40 for book in anchors})
remaining = sorted(set(range(1, 101)) - set(anchors),
                   key=lambda book: (sum(book in ratings_by_user[user] for user in source_users), book))
for book in remaining:
    candidates = [user for user in source_users
                  if book in ratings_by_user[user] and len(chosen[user]) < 20]
    if not candidates:
        raise ValueError(f"selected users cannot cover book {book}")
    user = min(candidates, key=lambda item: (len(chosen[item]), item))
    chosen[user].add(book)
    frequency[book] += 1

for user in source_users:
    while len(chosen[user]) < 20:
        book = min(set(ratings_by_user[user]) - chosen[user],
                   key=lambda item: (frequency[item], item))
        chosen[user].add(book)
        frequency[book] += 1

OUTPUT.mkdir(parents=True, exist_ok=True)
write_csv("books.csv", ("book_id", "title", "author", "category", "stock"),
          ((f"B{book:03d}", *books[book], "未分类",
            2 if 21 <= book <= 30 and book % 2 == 1 else 3)
           for book in range(1, 101)))
write_csv("users.csv", ("user_id", "name"),
          ((f"U{index:02d}", f"匿名读者{index:02d}") for index in range(40)))
write_csv("ratings.csv", ("user_id", "book_id", "score"),
          ((f"U{index:02d}", f"B{book:03d}", ratings_by_user[user][book])
           for index, user in enumerate(source_users) for book in sorted(chosen[user])))
write_csv("borrow_records.csv",
          ("record_id", "user_id", "book_id", "borrow_date", "return_date", "status"),
          ((f"R{index:03d}", f"U{index - 1:02d}", f"B{index + 20:03d}",
            "2026-09-20", "2026-09-21" if index % 2 == 0 else "",
            "returned" if index % 2 == 0 else "borrowed")
           for index in range(1, 11)))
write_csv("source_user_ids.csv", ("user_id", "goodbooks_user_id"),
          ((f"U{index:02d}", user) for index, user in enumerate(source_users)))
print("Wrote 100 source books, 40 anonymized users, 800 unchanged ratings, and 10 simulated loans")
