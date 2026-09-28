-- Invalid: Set table whose id has no foreign key to its parent collection
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
    id INTEGER NOT NULL,
    tag TEXT NOT NULL,
    UNIQUE (id, tag)
) STRICT;
