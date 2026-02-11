-- Invalid: Same attribute name in collection and vector table
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    value INTEGER
) STRICT;

CREATE TABLE Collection_vector_group (
    id INTEGER,
    vector_index INTEGER,
    value INTEGER,
    FOREIGN KEY (id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, vector_index)
) STRICT;
