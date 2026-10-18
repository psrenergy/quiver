-- Schema: Malformed DATE_TIME cells
-- Tests: readers that meet a date the write gate would have refused, as a pre-gate database or
-- another tool leaves behind. query_* is read-only, so the rows are seeded here: element 1 of
-- Items and of Collection each hold one '2024-1-5' cell.
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items_vector_events (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    date_event TEXT,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_data (
    id INTEGER,
    date_time TEXT,
    value REAL,
    FOREIGN KEY (id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;

INSERT INTO Configuration (label) VALUES ('Config');
INSERT INTO Items (label) VALUES ('Item 1');
INSERT INTO Items_vector_events (id, vector_index, date_event) VALUES (1, 1, '2024-1-5');
INSERT INTO Collection (label) VALUES ('Item 1');
INSERT INTO Collection_time_series_data (id, date_time, value) VALUES (1, '2024-1-5', 1.5);
