-- Fixture: bess_like -- CORPUS-01 (BESSOperation storage.toml), PARSE-03, PARSE-07, PARSE-08
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Storage (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    degradation INTEGER,
    capacity REAL,
    cycles INTEGER
) STRICT;

-- Value column is "rate", never "degradation" -- a column named after the group would collide
-- with the Storage.degradation scalar column and make from_schema fail
-- (src/schema_validator.cpp:284-288, "Duplicate attribute ... already defined in collection").
CREATE TABLE Storage_vector_degradation (
    id INTEGER NOT NULL REFERENCES Storage(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    rate REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Storage_vector_rte (
    id INTEGER NOT NULL REFERENCES Storage(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    efficiency REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;
