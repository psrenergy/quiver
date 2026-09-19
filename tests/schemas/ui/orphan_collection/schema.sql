-- Fixture: orphan_collection -- PARSE-01. agent.toml is a fully-formed collection file that
-- ui/main.toml's `collections` array does not list, reproducing SCE's real case.
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Storage (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    has_commitment INTEGER
) STRICT;

-- Agent exists in the schema so agent.toml is unloaded because main.collections omits it,
-- never because the table itself is missing.
CREATE TABLE Agent (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    risk_profile INTEGER
) STRICT;
