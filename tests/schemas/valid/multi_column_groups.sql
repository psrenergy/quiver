-- Schema: Vector and set groups with more than one value column
-- Tests: same-length validation across a group's value columns (an empty alphabetically-first
-- column used to skip the check and index past the end), multi-column whole-group read/write
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

-- "amount" sorts before "score": the column map the core builds is name-ordered
CREATE TABLE Items_vector_readings (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    amount REAL,
    score REAL,
    PRIMARY KEY (id, vector_index)
) STRICT;

-- "code" sorts before "weight"; every set value column must be part of the UNIQUE constraint
CREATE TABLE Items_set_codes (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    code TEXT,
    weight REAL,
    UNIQUE (id, code, weight)
) STRICT;
