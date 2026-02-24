-- Schema: All type combinations for gap coverage
-- Tests: String vectors, integer sets, float sets
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE AllTypes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    some_integer INTEGER,
    some_float REAL,
    some_text TEXT
) STRICT;

-- String vector (missing from all existing schemas)
CREATE TABLE AllTypes_vector_labels (
    id INTEGER NOT NULL REFERENCES AllTypes(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    label_value TEXT NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

-- Integer vector (for completeness)
CREATE TABLE AllTypes_vector_counts (
    id INTEGER NOT NULL REFERENCES AllTypes(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    count_value INTEGER NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

-- Integer set (missing from non-FK schemas)
CREATE TABLE AllTypes_set_codes (
    id INTEGER NOT NULL REFERENCES AllTypes(id) ON DELETE CASCADE ON UPDATE CASCADE,
    code INTEGER NOT NULL,
    UNIQUE (id, code)
) STRICT;

-- Float set (missing from all existing schemas)
CREATE TABLE AllTypes_set_weights (
    id INTEGER NOT NULL REFERENCES AllTypes(id) ON DELETE CASCADE ON UPDATE CASCADE,
    weight REAL NOT NULL,
    UNIQUE (id, weight)
) STRICT;

-- String set (for completeness)
CREATE TABLE AllTypes_set_tags (
    id INTEGER NOT NULL REFERENCES AllTypes(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tag TEXT NOT NULL,
    UNIQUE (id, tag)
) STRICT;
