-- Schema: Composite helper tests (read_vectors_by_id, read_sets_by_id)
-- Tests: Single-column vector/set groups where group_name == column_name
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

-- Vector groups: group_name == column_name
CREATE TABLE Items_vector_amount (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    amount INTEGER NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Items_vector_score (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    score REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Items_vector_note (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    note TEXT NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

-- Set groups: group_name == column_name
CREATE TABLE Items_set_code (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    code INTEGER NOT NULL,
    UNIQUE (id, code)
) STRICT;

CREATE TABLE Items_set_weight (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    weight REAL NOT NULL,
    UNIQUE (id, weight)
) STRICT;

CREATE TABLE Items_set_tag (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tag TEXT NOT NULL,
    UNIQUE (id, tag)
) STRICT;
