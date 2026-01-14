# Vector Attributes

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