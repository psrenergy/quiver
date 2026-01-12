PRAGMA user_version = 1;
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    value1 REAL NOT NULL DEFAULT 100,
    text1 TEXT NOT NULL DEFAULT 'default',
    text2 TEXT NOT NULL DEFAULT "default",
    text3 TEXT NOT NULL DEFAULT 'de"something"fault',
    text4 TEXT NOT NULL DEFAULT "de'something'fault",
    text5 TEXT NOT NULL DEFAULT 'de''something''fault',
    text6 TEXT NOT NULL DEFAULT "de' 'something' 'fault",
    text7 TEXT NOT NULL DEFAULT '"default"',
    text8 TEXT NOT NULL DEFAULT "  d e f a u l t   "
) STRICT;
