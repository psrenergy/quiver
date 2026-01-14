# Set Attributes

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
