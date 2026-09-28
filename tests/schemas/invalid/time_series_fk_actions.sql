-- Invalid: Time series table whose parent FK does not use ON DELETE CASCADE ON UPDATE CASCADE
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_data (
    id INTEGER NOT NULL,
    date_time TEXT NOT NULL,
    value REAL,
    FOREIGN KEY (id) REFERENCES Collection(id),
    PRIMARY KEY (id, date_time)
) STRICT;
