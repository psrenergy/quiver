-- Schema: CSV export test
-- Tests: Scalar export, group export, enum resolution, DateTime formatting, NULL values
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    status INTEGER,
    price REAL,
    date_created TEXT,
    notes TEXT
) STRICT;

CREATE TABLE Items_vector_measurements (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    measurement REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Items_set_tags (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tag TEXT NOT NULL,
    UNIQUE (id, tag)
) STRICT;

CREATE TABLE Items_time_series_readings (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    temperature REAL NOT NULL,
    humidity INTEGER NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
