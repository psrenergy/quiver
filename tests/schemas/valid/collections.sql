-- Schema: Collections with vectors, sets, time series
-- Tests: Vector, set, and time series patterns
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    some_integer INTEGER,
    some_float REAL
) STRICT;

CREATE TABLE Collection_vector_values (
    id INTEGER,
    vector_index INTEGER,
    value_int INTEGER,
    value_float REAL,
    FOREIGN KEY (id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Collection_set_tags (
    id INTEGER,
    tag TEXT,
    FOREIGN KEY(id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE(id, tag)
) STRICT;

CREATE TABLE Collection_time_series_data (
    collection_id INTEGER,
    date_time TEXT,
    value REAL,
    FOREIGN KEY (collection_id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (collection_id, date_time)
) STRICT;
