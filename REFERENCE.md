# PSR Database - Public API Reference

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
