PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

-- Plant.events: date_time is the only date column in the primary key, so it is the dimension.
-- date_approved is a nullable value column that also starts with date_ and sorts before
-- date_time; it must never be taken for the dimension.
CREATE TABLE Plant (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Plant_time_series_events (
    id INTEGER NOT NULL REFERENCES Plant(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    date_approved TEXT,
    value REAL,
    PRIMARY KEY (id, date_time)
) STRICT;

-- Meter.blocks: the date column sits outside the primary key, so the group has no date
-- dimension and its metadata and reads throw "Dimension column not found".
CREATE TABLE Meter (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Meter_time_series_blocks (
    id INTEGER NOT NULL REFERENCES Meter(id) ON DELETE CASCADE ON UPDATE CASCADE,
    block INTEGER NOT NULL,
    date_time TEXT,
    value REAL,
    PRIMARY KEY (id, block)
) STRICT;
