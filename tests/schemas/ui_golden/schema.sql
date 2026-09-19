-- Schema: pre-change describe/summarize_collection golden baseline (no ui/ sidecar)
-- Deliberately lives outside tests/schemas/ui/ so the corpus walk in plan 01-02 never sees it,
-- and deliberately has no ui/ subdirectory of its own.
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    priority INTEGER,
    weight REAL
) STRICT;

CREATE TABLE Items_vector_values (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    amount REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Items_set_tags (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tag TEXT NOT NULL,
    UNIQUE (id, tag)
) STRICT;

CREATE TABLE Items_time_series_data (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
