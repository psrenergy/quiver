PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Resource (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Resource_time_series_load (
    id INTEGER NOT NULL REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    block INTEGER NOT NULL,
    load REAL,
    flag INTEGER,
    PRIMARY KEY (id, date_time, block)
) STRICT;
