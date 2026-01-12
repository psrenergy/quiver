# PSRDatabase

## SQL Schema Conventions

SQL schemas for the `PSRDatabase` framework should follow the conventions described in this document. Note that this is a tool for creating and developing some kinds of applications. Not all tools will need to use this framework.

### Collections

- The Table name should be the same as the name of the Collection.
- The Table name of a Collection should beging with a capital letter and be in singular form.
- In case of a Collection with a composite name, the Table name should written in Pascal Case.
- The Table must contain a primary key named `id` that is an `INTEGER`. You should use the `AUTOINCREMENT` keyword to automatically generate the `id` for each element.

Examples:

```sql
CREATE TABLE Resource (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    some_type TEXT
) STRICT;

CREATE TABLE ThermalPlant(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    minimum_generation REAL DEFAULT 0
) STRICT;
```

#### Configuration collection

Every database definition must have a `Configuration`, which will store information from the case. 
The column `label` is not mandatory for a `Configuration` collection.

```sql
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    value1 REAL NOT NULL DEFAULT 100,
) STRICT;
```

### Scalar Attributes

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

### Vector Attributes

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

### Set Attributes

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

### Time Series

- Time Series stored in the database should be stored in a table with the name of the Collection followed by `_time_series_` and the name of the attribute group, such a `COLLECTION_time_series_GROUP_OF_ATTRIBUTES`.
- The table must contain a Column named `id`.
- A mandatory column named `date_time` should be created to store the date of the time series data. The date_time column should be of type `TEXT` and have the `NOT NULL` constraint.

Example:

```sql
CREATE TABLE Resource_time_series_group1 (
    id INTEGER, 
    date_time TEXT NOT NULL,
    some_vector1 REAL,
    some_vector2 REAL,
    FOREIGN KEY(id) REFERENCES Resource(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (id, date_time)
) STRICT; 
```

### Time Series Files

- All Time Series files for the elements from a Collection should be stored in a Table
- The Table name should be the same as the name of the Collection followed by `_time_series_files`, e. g. `COLLECTION_time_series_files`.
- Each Column of the table should be named after the name of the attribute.
- Each Column should store the path to the file containing the time series data.

Example:

```sql
CREATE TABLE Plant_time_series_files (
    generation TEXT,
    cost TEXT
) STRICT;
```

### Migrations

Migrations are an important part of the `PSRDatabase` framework. They are used to update the database schema to a new version without the need to delete the database and create a new one from scratch. Migrations are defined by two separate `.sql` files that are stored in the `migrations` directory of the model. The first file is the `up` migration and it is used to update the database schema to a new version. The second file is the `down` migration and it is used to revert the changes made by the `up` migration. Migrations are stored in directories in the model and they have a specific naming convention. The name of the migration folder should be the number of the version (e.g. `/migrations/1/`).

```md
database/migrations
├── 1
│   ├── up.sql
│   └── down.sql
└── 2
    ├── up.sql
    └── down.sql
```

## Design Pillars

1. **Two Reading Paradigms**
   - Attribute-focused: Read one attribute across elements (by label)
   - Element-focused: Read all attributes for one element (by ID)

2. **Full CRUD Support**
   - Create, Read, Update, Delete elements

3. **Explicit Groups**
   - Users specify vector/set/time_series group names explicitly

4. **Plural/Singular Naming**
   - Plural = all elements (`read_scalars`)
   - Singular = one element (`read_scalar`)

## Database Options Builder

### Create Database Options

| Language |                Syntax                 |
| -------- | ------------------------------------- |
| C++      | `DatabaseOptions()`                   |
| C        | `database_options_create()`           |
| Dart     | `DatabaseOptions()`                   |
| Julia    | `DatabaseOptions()`                   |
| Python   | TODO                                  |

### Set Read Only

Arguments:
- `read_only`: Boolean indicating if the database should be opened in read-only mode

| Language |                        Syntax                        |
| -------- | ---------------------------------------------------- |
| C++      | `options.set_read_only(read_only)`                   |
| C        | `database_options_set_read_only(options, read_only)` |
| Dart     | `options.setReadOnly(readOnly)`                      |
| Julia    | `set_read_only!(options, read_only)`                 |
| Python   | TODO                                                 |

### Set Logging Level

Arguments:
- `level`: Logging level (e.g., DEBUG, INFO, WARN, ERROR)

| Language |                        Syntax                        |
| -------- | ---------------------------------------------------- |
| C++      | `options.set_logging_level(level)`                   |
| C        | `database_options_set_logging_level(options, level)` |
| Dart     | `options.setLoggingLevel(level)`                     |
| Julia    | `set_logging_level!(options, level)`                 |
| Python   | TODO                                                 |

## Database Lifecycle

### Create Database From Schema

Arguments:
- `path`: Path to the database file to create
- `schema`: Schema object defining the database structure
- `options`: Database options

| Language |                        Syntax                         |
| -------- | ----------------------------------------------------- |
| C++      | `Database::create_from_schema(path, schema, options)` |
| C        | `database_create_from_schema(path, schema, options)`  |
| Dart     | `Database.createFromSchema(path, schema, options)`    |
| Julia    | `Database.create_from_schema(path, schema, options)`  |
| Python   | TODO                                                  |

### Create Database From Migrations

Arguments:
- `path`: Path to the database file to create
- `directory`: Directory containing migration files
- `options`: Database options

| Language |                            Syntax                            |
| -------- | ------------------------------------------------------------ |
| C++      | `Database::create_from_migrations(path, directory, options)` |
| C        | `database_create_from_migrations(path, directory, options)`  |
| Dart     | `Database.createFromMigrations(path, directory, options)`    |
| Julia    | `Database.create_from_migrations(path, directory, options)`  |
| Python   | TODO                                                         |

### Open Database

Arguments:
- `path`: Path to the database file to open
- `options`: Database options

| Language |             Syntax             |
| -------- | ------------------------------ |
| C++      | `Database(path, options)`      |
| C        | `database_open(path, options)` |
| Dart     | `Database.open(path, options)` |
| Julia    | `Database(path, options)`      |
| Python   | TODO                           |

### Close Database

| Language |        Syntax        |
| -------- | -------------------- |
| C++      | `~Database()`        |
| C        | `database_close(db)` |
| Dart     | `db.close()`         |
| Julia    | `close(db)`          |
| Python   | TODO                 |

---

## Element Builder

### Create Element

| Language |       Syntax       |
| -------- | ------------------ |
| C++      | `Element()`        |
| C        | `element_create()` |
| Dart     | `Element()`        |
| Julia    | `Element()`        |
| Python   | TODO               |

### Set Element String Value (Same for Integer, Float, Bool)

Arguments:
- `name`: Name of the attribute
- `value`: String value to set

| Language |                   Syntax                   |
| -------- | ------------------------------------------ |
| C++      | `element.set(name, value)`                 |
| C        | `element_set_string(element, name, value)` |
| Dart     | `element[name] = value`                    |
| Julia    | `element[name] = value`                    |
| Python   | TODO                                       |

### Set Element Null Value

Arguments:
- `name`: Name of the attribute

| Language |                Syntax                 |
| -------- | ------------------------------------- |
| C++      | `element.set_null(name)`              |
| C        | `element_set_null(element, name)`     |
| Dart     | `element[name] = null`                |
| Julia    | `element[name] = nothing`             |
| Python   | TODO                                  |

## CRUD

### Create Element

Arguments:
- `collection`: Name of the collection to add the element to
- `element`: Element object

| Language |                       Syntax                       |
| -------- | -------------------------------------------------- |
| C++      | `db.create_element(collection, element)`           |
| C        | `database_create_element(db, collection, element)` |
| Dart     | `db.createElement(collection, element)`            |
| Julia    | `create_element!(db, collection, element)`         |
| Python   | TODO                                               |

### Read (Element-focused - by ID)

|    Operation     |                           C++                            |                         Dart                          |                           Julia                           |
| ---------------- | -------------------------------------------------------- | ----------------------------------------------------- | --------------------------------------------------------- |
| List IDs         | `db.get_element_ids(collection)`                         | `db.getElementIds(collection)`                        | `get_element_ids(db, collection)`                         |
| Read scalars     | `db.read_element_scalars(collection, id)`                | `db.readElementScalars(collection, id)`               | `read_element_scalars(db, collection, id)`                |
| Read vectors     | `db.read_element_vectors(collection, id, grp)`           | `db.readElementVectors(collection, id, grp)`          | `read_element_vectors(db, collection, id, grp)`           |
| Read sets        | `db.read_element_sets(collection, id, grp)`              | `db.readElementSets(collection, id, grp)`             | `read_element_sets(db, collection, id, grp)`              |

### Read (Attribute-focused - by label)

|  Operation   |                      C++                       |                     Dart                      |                      Julia                      |
| ------------ | ---------------------------------------------- | --------------------------------------------- | ----------------------------------------------- |
| Scalar (all) | `db.read_scalars(collection, attribute)`       | `db.readScalars(collection, attribute)`       | `read_scalars(db, collection, attribute)`       |
| Scalar (one) | `db.read_scalar(collection, attribute, label)` | `db.readScalar(collection, attribute, label)` | `read_scalar(db, collection, attribute, label)` |
| Vector (all) | `db.read_vectors(collection, attribute)`       | `db.readVectors(collection, attribute)`       | `read_vectors(db, collection, attribute)`       |
| Vector (one) | `db.read_vector(collection, attribute, label)` | `db.readVector(collection, attribute, label)` | `read_vector(db, collection, attribute, label)` |
| Set (all)    | `db.read_sets(collection, attribute)`          | `db.readSets(collection, attribute)`          | `read_sets(db, collection, attribute)`          |
| Set (one)    | `db.read_set(collection, attribute, label)`    | `db.readSet(collection, attribute, label)`    | `read_set(db, collection, attribute, label)`    |

### Update

| Operation |                     C++                      |                    Dart                    |                      Julia                       |
| --------- | -------------------------------------------- | ------------------------------------------ | ------------------------------------------------ |
| Update    | `db.update_element(collection, id, element)` | `db.updateElement(collection, id, values)` | `update_element!(db, collection, id; kwargs...)` |

### Delete

| Operation |                 C++                 |                Dart                |                 Julia                 |
| --------- | ----------------------------------- | ---------------------------------- | ------------------------------------- |
| Delete    | `db.delete_element(collection, id)` | `db.deleteElement(collection, id)` | `delete_element!(db, collection, id)` |

---



## Table Naming Conventions

| Table Type  |              Pattern               |          Example           |
| ----------- | ---------------------------------- | -------------------------- |
| Collection  | `CollectionName`                   | `Plant`                    |
| Vector      | `CollectionName_vector_group`      | `Plant_vector_hourly`      |
| Set         | `CollectionName_set_group`         | `Plant_set_fuels`          |
| Time Series | `CollectionName_time_series_group` | `Plant_time_series_output` |
