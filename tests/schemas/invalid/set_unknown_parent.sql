-- Invalid: Set table named after a collection that does not exist
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Ghost_set_tags (
    id INTEGER NOT NULL,
    tag TEXT NOT NULL,
    FOREIGN KEY (id) REFERENCES Ghost(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE (id, tag)
) STRICT;
