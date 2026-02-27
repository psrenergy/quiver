PRAGMA foreign_keys = ON;
PRAGMA user_version = 1;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL,
    integer_attribute INTEGER DEFAULT 6,
    string_attribute TEXT,
    float_attribute REAL,
    date_time_attribute TEXT,
    boolean_attribute INTEGER,
    coordinates_attribute TEXT,
    conditional_attribute INTEGER,
    advanced_conditional INTEGER
) STRICT;

CREATE TABLE Collection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Collection_time_series_some_time_series (
    id INTEGER,
    date_time TEXT,
    some_time_series_float REAL,
    some_time_series_integer INTEGER,
    FOREIGN KEY (id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;

CREATE TABLE Collection_set_some_set (
    id INTEGER,
    some_set_string TEXT,
    FOREIGN KEY(id) REFERENCES Collection(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE(id, some_set_string)
) STRICT;