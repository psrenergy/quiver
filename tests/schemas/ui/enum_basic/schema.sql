-- Fixture: enum_basic -- the tracer slice's UI sidecar (tests/schemas/ui/enum_basic/ui/)
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Storage (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    has_commitment INTEGER,
    max_generation REAL,
    internal_code INTEGER,
    notes TEXT
) STRICT;
