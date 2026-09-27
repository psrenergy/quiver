-- Schema: two collections that reference each other with ON DELETE CASCADE
-- Tests: import_csv deletes an omitted element without cascading around the cycle into a kept one
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Item (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    tag_pinned INTEGER,
    FOREIGN KEY (tag_pinned) REFERENCES Tag(id) ON DELETE CASCADE ON UPDATE CASCADE
) STRICT;

CREATE TABLE Tag (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    item_owner INTEGER,
    FOREIGN KEY (item_owner) REFERENCES Item(id) ON DELETE CASCADE ON UPDATE CASCADE
) STRICT;

CREATE TABLE Item_vector_weights (
    id INTEGER NOT NULL REFERENCES Item(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    weight REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;
