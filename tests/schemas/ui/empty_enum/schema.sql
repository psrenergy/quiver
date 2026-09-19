-- Fixture: empty_enum -- PARSE-09, zero-byte enum.toml (the other half of PARSE-09 vs no_enum)
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Storage (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    has_commitment INTEGER
) STRICT;
