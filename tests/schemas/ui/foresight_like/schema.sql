-- Fixture: foresight_like -- CORPUS-01 (Foresight enum.toml), PARSE-04, and the corpus's
-- encoding fixture (accented labels reachable at locale "en" -- see 01-02-PLAN.md).
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE EconomicDriver (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    forecast_model INTEGER,
    notes TEXT
) STRICT;
