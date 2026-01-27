-- Schema: Time series test schema
-- Tests: Time series patterns with single and multi-dimensional tables
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Resource (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

-- Simple time series with only date_time dimension
CREATE TABLE Resource_time_series_group1 (
    id INTEGER NOT NULL,
    date_time TEXT NOT NULL,
    value1 REAL,
    value2 REAL,
    FOREIGN KEY(id) REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;

-- Multi-dimensional time series with block dimension
CREATE TABLE Resource_time_series_group2 (
    id INTEGER NOT NULL,
    date_time TEXT NOT NULL,
    block INTEGER NOT NULL,
    value3 REAL,
    FOREIGN KEY(id) REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time, block)
) STRICT;

-- Multi-dimensional time series with block and scenario dimensions
CREATE TABLE Resource_time_series_group3 (
    id INTEGER NOT NULL,
    date_time TEXT NOT NULL,
    block INTEGER NOT NULL,
    scenario INTEGER NOT NULL,
    value4 REAL,
    FOREIGN KEY(id) REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time, block, scenario)
) STRICT;
