-- Fixture: format_table -- PARSE-06, the `format` 4-key table form (no real corpus file uses it,
-- so this fixture is deliberately synthetic), plus a collection with zero [[attribute]] blocks.
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Storage (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    capacity REAL,
    efficiency REAL,
    code TEXT
) STRICT;

CREATE TABLE EmptyCollection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;
