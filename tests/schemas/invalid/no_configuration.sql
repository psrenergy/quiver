-- Invalid: Missing Configuration table
PRAGMA foreign_keys = ON;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;
