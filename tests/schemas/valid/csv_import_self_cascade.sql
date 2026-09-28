-- Schema: a self-reference with ON DELETE CASCADE
-- Tests: import_csv deletes an omitted element without cascading through a stale self-reference
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Node (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    node_parent INTEGER,
    FOREIGN KEY (node_parent) REFERENCES Node(id) ON DELETE CASCADE ON UPDATE CASCADE
) STRICT;

CREATE TABLE Node_vector_weights (
    id INTEGER NOT NULL REFERENCES Node(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    weight REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;
