# Attributes

## Scalar Attributes

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

## Vector Attributes

- In case of a vector attribute, a table should be created with its name indicating the name of the Collection and the name of a group of the attribute, separated by `_vector_`, such as `COLLECTION_vector_GROUP_OF_ATTRIBUTES`.
- The table must contain a Column named `id` and another named `vector_index`. These two columns together should form the Primary Key of the table.
- There must be a Column named after the attributes names, which will store the value of the attribute for the specified element `id` and index `vector_index`.

These groups are used to store vectors that should have the same size. If two vectors don't necessarily have the same size, they should be stored in different groups.

Example:

```sql
CREATE TABLE ThermalPlant_vector_some_group(
    id INTEGER,
    vector_index INTEGER NOT NULL,
    some_value REAL NOT NULL,
    some_other_value REAL,
    FOREIGN KEY (id) REFERENCES ThermalPlant(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (id, vector_index)
) STRICT;
```

A vector relation with another collection should be stored in a table of vector groups and be defined the same way as a vector attribute. To tell that it is a relation with another collection, the name of the relational attribute should be the name of the target collection followed by the relation type defined as `_relation_type`, i.e. `gaugingstation_id` indicated that the collection HydroPlant has an `id` relation with the collection GaugingStation. If the name of the attribute was `gaugingstation_turbine_to`, it would indicate that the collection HydroPlant has a relation `turbine_to` with the collection GaugingStation.

```sql
CREATE TABLE HydroPlant_vector_gaugingstations(
    id INTEGER,
    vector_index INTEGER NOT NULL,
    conversion_factor REAL NOT NULL,
    gaugingstation_id INTEGER,
    FOREIGN KEY (gaugingstation_id) REFERENCES GaugingStation(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (id, vector_index)
) STRICT;
```

## Set Attributes

A set is a collection of unique values associated with an element from a Collection. Sets are similar to vectors, but they do not have a specific order and have to be unique.

- In case of a set attribute, a table should be created with its name indicating the name of the Collection and the name of a group of the attribute, separated by `_set_`, such as `COLLECTION_set_GROUP_OF_ATTRIBUTES`.
- The table must contain a Column named `id`.
- The table must not have any primary keys.
- The table must have an unique constraint on the combination of all columns.

Example:

```sql
CREATE TABLE HydroPlant_set_gaugingstations(
    id INTEGER,
    conversion_factor REAL NOT NULL,
    gaugingstation_id INTEGER,
    FOREIGN KEY (gaugingstation_id) REFERENCES GaugingStation(id) ON UPDATE CASCADE ON DELETE CASCADE,
    UNIQUE (id, conversion_factor, gaugingstation_id)
) STRICT;
```
