# Scalar Attributes

- The name of an Attribute should be in snake case and be in singular form.
- If the attribute's name is `label`, it should be stored as a `TEXT` and have the `UNIQUE` and `NOT NULL` constraints.

Example:

```sql
CREATE TABLE ThermalPlant(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    minimum_generation REAL NOT NULL
    some_example_of_attribute REAL
) STRICT;
```

If an attribute name starts with `date` it should be stored as a `TEXT` and indicates a date that will be mapped to a DateTime object.

Example:

```sql
CREATE TABLE ThermalPlant(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    minimum_generation REAL NOT NULL,
    date_of_construction TEXT
) STRICT;
```

A relation with another collection should be stored as an attribute whose name is the name of the target collection followed by the relation type defined as `_relation_type`, i.e. `collectionname_relation_type`. The relation attribute name starts with the name of another collection it should be stored as a `INTEGER` and indicates a relation with another collection. It should never have the `NOT NULL` constraint. All references should always declare the `ON UPDATE CASCADE ON DELETE CASCADE` constraint. In the example below the attribute `gaugingstation_id` indicates that the collection Plant has an `id` relation with the collection GaugingStation and the attribute `plant_spill_to` indicates that the collection Plant has a `spill_to` relation with itself.

Example:

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