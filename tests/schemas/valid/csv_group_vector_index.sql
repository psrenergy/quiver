-- Schema: group columns named vector_index outside a vector group
-- Tests: import_csv treats only a vector group's vector_index as the structural index; a set or
-- time-series column of that name keeps its declared type (TEXT stays text, INTEGER is validated).
-- Two collections, because one collection may not declare the same attribute in two groups.
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items_time_series_slots (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    vector_index INTEGER NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;

CREATE TABLE Codes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Codes_set_tags (
    id INTEGER NOT NULL REFERENCES Codes(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index TEXT NOT NULL,
    UNIQUE (id, vector_index)
) STRICT;
