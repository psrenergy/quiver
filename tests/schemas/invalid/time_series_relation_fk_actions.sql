-- Invalid: Time series relation FK without ON UPDATE CASCADE and ON DELETE SET NULL or CASCADE
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Parent (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_events (
    id INTEGER NOT NULL,
    date_time TEXT NOT NULL,
    parent_id INTEGER,
    FOREIGN KEY (id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    FOREIGN KEY (parent_id) REFERENCES Parent(id),
    PRIMARY KEY (id, date_time)
) STRICT;
