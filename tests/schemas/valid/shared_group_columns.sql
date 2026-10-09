-- Schema: groups whose column names collide, which the validator allows
-- Tests: a per-column reader resolves a column NAME (Schema::find_vector_table/find_set_table),
-- so only a group-addressed read (read_{vector,set}_group_by_id) is sure to reach a group's own
-- table
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Parent (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Child (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

-- Two vector groups share the FK column parent_ref (validate_no_duplicate_attributes exempts FK
-- columns). A per-column read of parent_ref resolves to Child_vector_links, the vector table whose
-- name sorts first, never to Child_vector_routes
CREATE TABLE Child_vector_links (
    id INTEGER NOT NULL REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    parent_ref INTEGER REFERENCES Parent(id) ON DELETE SET NULL ON UPDATE CASCADE,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Child_vector_routes (
    id INTEGER NOT NULL REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    parent_ref INTEGER REFERENCES Parent(id) ON DELETE SET NULL ON UPDATE CASCADE,
    cost REAL,
    PRIMARY KEY (id, vector_index)
) STRICT;

-- A group named after a column it does not hold: the per-column read of Child_vector_routes'
-- "cost" must not stop at this table just because of its name
CREATE TABLE Child_vector_cost (
    id INTEGER NOT NULL REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    amount REAL,
    PRIMARY KEY (id, vector_index)
) STRICT;

-- The set counterpart of links/routes: parent_ref resolves to Child_set_mentors
CREATE TABLE Child_set_mentors (
    id INTEGER NOT NULL REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    parent_ref INTEGER REFERENCES Parent(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE (id, parent_ref)
) STRICT;

CREATE TABLE Child_set_sponsors (
    id INTEGER NOT NULL REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    parent_ref INTEGER REFERENCES Parent(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tier INTEGER,
    UNIQUE (id, parent_ref, tier)
) STRICT;

-- The set counterpart of Child_vector_cost: named after sponsors' "tier" column without holding it
CREATE TABLE Child_set_tier (
    id INTEGER NOT NULL REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    rank INTEGER,
    UNIQUE (id, rank)
) STRICT;
