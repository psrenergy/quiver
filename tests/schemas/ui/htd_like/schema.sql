-- Fixture: htd_like -- CORPUS-01 (HydroThermalDispatch hydro_plant.toml), PARSE-10, and the
-- missing-`id` skipped-not-fatal case (thermal_plant.toml).
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE HydroPlant (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    has_commitment INTEGER
) STRICT;

CREATE TABLE HydroPlant_time_series_generation (
    id INTEGER NOT NULL REFERENCES HydroPlant(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;

-- ThermalPlant exists so thermal_plant.toml is skipped for its missing `id`, never for a
-- missing table.
CREATE TABLE ThermalPlant (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;
