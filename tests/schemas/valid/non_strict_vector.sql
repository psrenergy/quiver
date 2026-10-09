-- Schema: a collection and a vector group whose tables are deliberately NOT STRICT
-- Tests: the C API group readers never narrow a REAL cell into an INTEGER column, and
-- summarize_collection's value distribution counts only integer cells. Without STRICT, INTEGER
-- affinity keeps a non-integral value written through raw SQL (1.5) as REAL, and a non-numeric one
-- as TEXT, so a cell's storage class can differ from its declared type.
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    code INTEGER
);

CREATE TABLE Items_vector_counts (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    quantity INTEGER,
    PRIMARY KEY (id, vector_index)
);
