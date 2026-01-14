-- Schema: Basic scalar types
-- Tests: All scalar attribute types in Configuration
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL,
    integer_attribute INTEGER DEFAULT 6,
    float_attribute REAL,
    string_attribute TEXT,
    date_attribute TEXT,
    boolean_attribute INTEGER
) STRICT;
