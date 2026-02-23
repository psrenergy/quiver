-- Schema: Foreign key relations
-- Tests: Relations between collections, self-references, vectors/sets with FK
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
    label TEXT UNIQUE NOT NULL,
    parent_id INTEGER,
    sibling_id INTEGER,
    FOREIGN KEY (parent_id) REFERENCES Parent(id) ON DELETE SET NULL ON UPDATE CASCADE,
    FOREIGN KEY (sibling_id) REFERENCES Child(id) ON DELETE SET NULL ON UPDATE CASCADE
) STRICT;

CREATE TABLE Child_vector_refs (
    id INTEGER,
    vector_index INTEGER,
    parent_ref INTEGER,
    FOREIGN KEY (id) REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    FOREIGN KEY (parent_ref) REFERENCES Parent(id) ON DELETE SET NULL ON UPDATE CASCADE,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Child_set_parents (
    id INTEGER,
    parent_ref INTEGER,
    FOREIGN KEY(id) REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    FOREIGN KEY(parent_ref) REFERENCES Parent(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE(id, parent_ref)
) STRICT;

CREATE TABLE Child_set_mentors (
    id INTEGER,
    mentor_id INTEGER,
    FOREIGN KEY(id) REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    FOREIGN KEY(mentor_id) REFERENCES Parent(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE(id, mentor_id)
) STRICT;

CREATE TABLE Child_set_scores (
    id INTEGER,
    score INTEGER,
    FOREIGN KEY(id) REFERENCES Child(id) ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE(id, score)
) STRICT;
