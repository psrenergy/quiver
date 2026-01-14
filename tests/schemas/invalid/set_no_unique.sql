-- Invalid: Set table without UNIQUE constraint
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_set_tags (
    id INTEGER,
    tag TEXT,
    FOREIGN KEY(id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE
) STRICT;
