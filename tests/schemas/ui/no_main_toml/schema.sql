-- Fixture: no_main_toml -- WR-01: a ui/ directory that exists and holds enum.toml and
-- storage.toml, but no main.toml, must be treated as malformed (D-24/D-25), not as a valid empty
-- sidecar. Schema is an exact copy of tests/schemas/ui_golden/schema.sql (never edited directly --
-- that fixture lives outside tests/schemas/ui/ on purpose) so this fixture's byte-identical output
-- can be checked against the same golden .txt files.
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
