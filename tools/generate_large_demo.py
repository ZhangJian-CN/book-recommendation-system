"""Generate deterministic synthetic CSV data for the course demonstration."""

import csv
from pathlib import Path


OUTPUT = Path(__file__).resolve().parents[1] / "demo_data" / "large"
OUTPUT.mkdir(parents=True, exist_ok=True)


def write_csv(name, header, rows):
    with (OUTPUT / name).open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)


write_csv(
    "books.csv",
    ("book_id", "title", "author", "category", "stock"),
    (
        (f"B{i:03d}", f"演示图书{i:03d}", f"作者{(i - 1) % 20 + 1:02d}",
         ("计算机", "文学", "历史", "数学")[(i - 1) % 4], 1 + i % 4)
        for i in range(1, 101)
    ),
)

write_csv(
    "users.csv",
    ("user_id", "name"),
    ((f"U{u:02d}", f"演示读者{u:02d}") for u in range(40)),
)

ratings = []
for user in range(40):
    books = list(range(1, 9)) + [9 + (user * 11 + j * 7) % 92 for j in range(12)]
    for book in books:
        ratings.append((f"U{user:02d}", f"B{book:03d}", 1 + (user * 3 + book * 2) % 5))
write_csv("ratings.csv", ("user_id", "book_id", "score"), ratings)

write_csv(
    "borrow_records.csv",
    ("record_id", "user_id", "book_id", "borrow_date", "return_date", "status"),
    (
        (f"R{i:03d}", f"U{i - 1:02d}", f"B{i + 20:03d}", "2026-09-20",
         "2026-09-21" if i % 2 == 0 else "",
         "returned" if i % 2 == 0 else "borrowed")
        for i in range(1, 11)
    ),
)
