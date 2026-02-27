PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Sensor (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Sensor_time_series_readings (
    id INTEGER NOT NULL REFERENCES Sensor(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    temperature REAL NOT NULL,
    humidity INTEGER NOT NULL,
    status TEXT NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
