---
name: conventions
description: SQL schema conventions for QUIVERDatabase. When writing SQL schemas for QUIVERDatabase, follow these conventions strictly.
---

# Collections (Tables)

**Naming:**
- Table name = Collection name
- Use PascalCase (e.g., `ThermalPlant`, `GaugingStation`)
- Always singular form (not plural)

**Required structure:**
- Must have `id INTEGER PRIMARY KEY AUTOINCREMENT`
- Must end with `STRICT`
- Every database must have a `Configuration` table

```sql
CREATE TABLE CollectionName (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    -- columns here
) STRICT;
```

# Scalar Attributes

**Naming:**
- Use snake_case (e.g., `minimum_generation`, `some_example_value`)
- Singular form

**Special columns:**
- `label`: Must be `TEXT UNIQUE NOT NULL`
- `date_*` columns: Must be `TEXT` (mapped to DateTime)

**Relations to other collections:**
- Format: `targetcollection_relationtype` (e.g., `gaugingstation_id`, `plant_spill_to`)
- Type: `INTEGER` (never `NOT NULL`)
- Must have foreign key with `ON UPDATE CASCADE ON DELETE CASCADE`
- Self-referential relations use `ON UPDATE SET NULL ON DELETE CASCADE`

```sql
CREATE TABLE Plant(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    capacity REAL NOT NULL,
    gaugingstation_id INTEGER,
    plant_spill_to INTEGER,
    FOREIGN KEY(gaugingstation_id) REFERENCES GaugingStation(id) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY(plant_spill_to) REFERENCES Plant(id) ON UPDATE SET NULL ON DELETE CASCADE
) STRICT;
```

# Vector Attributes

**Table naming:** `COLLECTION_vector_GROUPNAME`

**Required structure:**
- Column `id INTEGER` (references parent)
- Column `vector_index INTEGER NOT NULL`
- Primary key: `(id, vector_index)`
- Foreign key to parent collection with CASCADE

```sql
CREATE TABLE ThermalPlant_vector_costs(
    id INTEGER,
    vector_index INTEGER NOT NULL,
    cost_value REAL NOT NULL,
    FOREIGN KEY (id) REFERENCES ThermalPlant(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (id, vector_index)
) STRICT;
```

# Set Attributes

**Table naming:** `COLLECTION_set_GROUPNAME`

**Required structure:**
- Column `id INTEGER` (references parent)
- NO primary key
- UNIQUE constraint on ALL columns combined

```sql
CREATE TABLE HydroPlant_set_gaugingstations(
    id INTEGER,
    conversion_factor REAL NOT NULL,
    gaugingstation_id INTEGER,
    FOREIGN KEY (gaugingstation_id) REFERENCES GaugingStation(id) ON UPDATE CASCADE ON DELETE CASCADE,
    UNIQUE (id, conversion_factor, gaugingstation_id)
) STRICT;
```

# Time Series

**Table naming:** `COLLECTION_time_series_GROUPNAME`

**Required structure:**
- Column `id INTEGER` (references parent)
- Column `date_time TEXT NOT NULL`
- Primary key: `(id, date_time)`
- Foreign key to parent collection with CASCADE

```sql
CREATE TABLE Resource_time_series_generation (
    id INTEGER,
    date_time TEXT NOT NULL,
    power_output REAL,
    efficiency REAL,
    FOREIGN KEY(id) REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT;
```

# Time Series Files

**Table naming:** `COLLECTION_time_series_files`

**Structure:**
- Each column stores a file path as `TEXT`
- Column names = attribute names

```sql
CREATE TABLE Plant_time_series_files (
    generation TEXT,
    cost TEXT
) STRICT;
```

# Quick Reference

| Element | Naming Pattern | Key Points |
|---------|---------------|------------|
| Collection | `PascalCase` singular | `id INTEGER PRIMARY KEY AUTOINCREMENT`, `STRICT` |
| Attribute | `snake_case` singular | `label` is `TEXT UNIQUE NOT NULL` |
| Relation | `collection_relationtype` | `INTEGER`, nullable, CASCADE |
| Vector table | `Collection_vector_group` | PK: `(id, vector_index)` |
| Set table | `Collection_set_group` | No PK, UNIQUE on all columns |
| Time series | `Collection_time_series_group` | PK: `(id, date_time)` |
| TS files | `Collection_time_series_files` | Columns store file paths |

# Common Mistakes to Avoid

1. Using plural table names (wrong: `Plants`, right: `Plant`)
2. Forgetting `STRICT` at the end
3. Making relation columns `NOT NULL`
4. Using `ON DELETE SET NULL` for non-self-referential relations
5. Forgetting `vector_index` in vector tables
6. Adding primary key to set tables
7. Using camelCase for attributes (use snake_case)
