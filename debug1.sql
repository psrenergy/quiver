PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_group1 (
    id INTEGER,
    date_time TEXT NOT NULL,
    some_vector1 REAL,
    PRIMARY KEY (id, date_time)
) STRICT;

CREATE TABLE Collection_time_series_group2 (
    id INTEGER,
    date_time TEXT NOT NULL,
    some_vector2 REAL,
    PRIMARY KEY (id, date_time)
) STRICT;
