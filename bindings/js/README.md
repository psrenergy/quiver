# quiverdb

SQLite wrapper binding for Quiver via Bun FFI (`bun:ffi`).

## Requirements

- [Bun](https://bun.sh) >= 1.1
- `libquiver.dll` and `libquiver_c.dll` (or `.so`/`.dylib`) accessible (see [Native Library Setup](#native-library-setup))

## Installation

```bash
bun add quiverdb
```

The bundled native libraries ship inside the package, so no separate download
step is required on supported platforms. To build the native libraries from
source, see the Quiver project build instructions.

```typescript
import { Database } from "quiverdb";
```

Bun loads native code via FFI without any permission flags — unlike Deno, there
is no `--allow-ffi`/`--allow-read`/`--allow-write` to pass.

## Native Library Setup

The loader searches for native libraries in three tiers:

1. **Bundled**: `libs/{os}-{arch}/` directory next to the binding source (shipped
   inside the published package).
2. **Dev mode**: Walks up directories looking for `build/bin/` (auto-discovered
   during in-tree development against a local C++ build).
3. **System PATH**: Falls back to loading by library name.

Currently prebuilt binaries ship for `linux-x86_64`, `macos-aarch64`, and
`windows-x86_64`. On other platforms, build from source and point to `build/bin/`
via tier 2.

## Quick Start

```typescript
import { Database } from "quiverdb";

const db = Database.fromSchema("my.db", "schema.sql");
db.createElement("Items", { label: "Item 1", value: 42 });

const values = db.readScalarIntegers("Items", "value");
console.log(values); // [42]

db.close();
```

Run with:

```bash
bun run example.ts
```

## API Methods

### Lifecycle

- `Database.fromSchema(dbPath, schemaPath, options?)` -- Create database from SQL schema file
- `Database.fromMigrations(dbPath, migrationsPath, options?)` -- Create database from migrations directory
- `Database.open(dbPath, options?)` -- Open an existing database file; the schema loads on first use
- `Database.validateMigrations(migrationsPath)` -- Validate a migrations directory (every `up.sql`, then every `down.sql`, ending with no table left behind) in-memory; throws on failure
- `close()` -- Close the database connection

### Create / Delete

- `createElement(collection, data)` -- Create element, returns numeric ID
- `updateElement(collection, id, data)` -- Update element by ID
- `deleteElement(collection, id)` -- Delete element by ID
- `updateElementByLabel(collection, label, data)` -- Update element by label
- `deleteElementByLabel(collection, label)` -- Delete element by label
- `updateRelation(collectionFrom, collectionTo, relationType, id, targetLabel)` -- Point a scalar
  relation at the element labelled `targetLabel` (`null` clears it)
- `updateRelationByLabel(collectionFrom, collectionTo, relationType, label, targetLabel)` -- Same,
  addressing the element by label
- `updateVectorGroup(collection, group, id, data)` -- Replace an element's rows in one named vector
  group (`{}` clears it)
- `updateVectorGroupByLabel(collection, group, label, data)` -- Same, addressing the element by label
- `updateSetGroup(collection, group, id, data)` -- Replace an element's rows in one named set group
  (`{}` clears it)
- `updateSetGroupByLabel(collection, group, label, data)` -- Same, addressing the element by label

### Read (bulk)

- `readScalarIntegers(collection, attribute)` -- Read all integer scalars
- `readScalarBooleans(collection, attribute)` -- Read INTEGER-backed boolean scalars
- `readScalarFloats(collection, attribute)` -- Read all float scalars
- `readScalarStrings(collection, attribute)` -- Read all string scalars
- `readVectorIntegers(collection, attribute)` -- Read all integer vectors
- `readVectorBooleans(collection, attribute)` -- Read INTEGER-backed boolean vectors
- `readVectorFloats(collection, attribute)` -- Read all float vectors
- `readVectorStrings(collection, attribute)` -- Read all string vectors
- `readSetIntegers(collection, attribute)` -- Read all integer sets
- `readSetBooleans(collection, attribute)` -- Read INTEGER-backed boolean sets
- `readSetFloats(collection, attribute)` -- Read all float sets
- `readSetStrings(collection, attribute)` -- Read all string sets

### Read (by ID)

- `readScalarIntegerById(collection, attribute, id)` -- Read integer or null
- `readScalarBooleanById(collection, attribute, id)` -- Read INTEGER-backed boolean or null
- `readScalarFloatById(collection, attribute, id)` -- Read float or null
- `readScalarStringById(collection, attribute, id)` -- Read string or null
- `readVectorIntegersById(collection, attribute, id)` -- Read integer vector
- `readVectorBooleansById(collection, attribute, id)` -- Read INTEGER-backed boolean vector
- `readVectorFloatsById(collection, attribute, id)` -- Read float vector
- `readVectorStringsById(collection, attribute, id)` -- Read string vector
- `readSetIntegersById(collection, attribute, id)` -- Read integer set
- `readSetBooleansById(collection, attribute, id)` -- Read INTEGER-backed boolean set
- `readSetFloatsById(collection, attribute, id)` -- Read float set
- `readSetStringsById(collection, attribute, id)` -- Read string set
- `readVectorGroupById(collection, group, id)` -- Read a whole vector group as rows (`null` for a SQL NULL cell)
- `readSetGroupById(collection, group, id)` -- Read a whole set group as rows (`null` for a SQL NULL cell)

### Read (IDs)

- `readElementIds(collection)` -- Read all element IDs in a collection
- `numberOfElements(collection)` -- Current number of elements in a collection

### Metadata

- `getScalarMetadata(collection, attribute)` -- Get scalar attribute metadata
- `getVectorMetadata(collection, attribute)` -- Get vector group metadata
- `getSetMetadata(collection, attribute)` -- Get set group metadata
- `getTimeSeriesMetadata(collection, attribute)` -- Get time series group metadata
- `listScalarAttributes(collection)` -- List all scalar attributes
- `listVectorGroups(collection)` -- List all vector groups
- `listSetGroups(collection)` -- List all set groups
- `listTimeSeriesGroups(collection)` -- List all time series groups

### Time Series

- `readTimeSeriesGroup(collection, group, id)` -- Read time series data for an element
- `updateTimeSeriesGroup(collection, group, id, data)` -- Write time series data for an element
  (`{}` clears it)
- `updateTimeSeriesGroupByLabel(collection, group, label, data)` -- Same, addressing the element by
  label
- `readTimeSeriesRow(collection, group, attribute, dateTime)` -- One value per element: the last
  non-null value at or before `dateTime`, or `null` if there is none
- `upsertTimeSeriesRow(collection, group, id, row)` -- Insert or replace one row, keyed by its
  dimension value
- `upsertTimeSeriesRowByLabel(collection, group, label, row)` -- Same, addressing the element by label
- `hasTimeSeriesFiles(collection)` -- Check whether the collection has a time series files table
- `listTimeSeriesFilesColumns(collection)` -- List the time series files columns
- `readTimeSeriesFiles(collection)` -- Read the file paths (`null` for an unset column)
- `updateTimeSeriesFiles(collection, data)` -- Write the file paths

### Query

- `queryString(sql, parameters?)` -- Query returning string or null
- `queryInteger(sql, parameters?)` -- Query returning integer or null
- `queryBoolean(sql, parameters?)` -- Query returning an INTEGER-backed boolean or null
- `queryFloat(sql, parameters?)` -- Query returning float or null

Parameters are passed as an array of `number | bigint | boolean | string | null` (a `bigint` binds
as an exact INTEGER, a `boolean` as the INTEGER 1 or 0).

### Transaction

- `beginTransaction()` -- Begin explicit transaction
- `commit()` -- Commit current transaction
- `rollback()` -- Rollback current transaction
- `inTransaction()` -- Check if transaction is active
- `beginDryRun()` -- Begin a dry run: a transaction `endDryRun()` always rolls back. While it is
  active, `beginTransaction`/`commit`/`rollback` are absorbed (no-ops)
- `endDryRun()` -- End the active dry run, rolling back everything it covered
- `inDryRun()` -- Check if a dry run is active

### CSV

- `exportCsv(collection, group, filePath, options?)` -- Export collection group to CSV file
- `importCsv(collection, group, filePath, options?)` -- Import CSV file into collection group

### Composites

- `readScalarsById(collection, id)` -- Read all scalar attributes for an element
- `readVectorsById(collection, id)` -- Read all vector groups for an element
- `readSetsById(collection, id)` -- Read all set groups for an element

### Introspection

- `describe()` -- Whole-database text report (returns a string)
- `describeCollection(collection)` -- One collection's structure (returns a string)
- `summarizeCollection(collection)` -- Per-attribute null/value summary (returns a string)
- `isHealthy()` -- Check database health
- `path()` -- Get database file path
- `currentVersion()` -- Get current schema version

### Lua

- `Sandbox(db)` -- Create Lua script runner with database access
- `run(script)` -- Execute a Lua script; returns its return value as a JSON string, or `""` if it
  returned nothing
- `close()` -- Close the Sandbox

## Types

Exported types available for TypeScript consumers:

- `ScalarValue` -- `number | bigint | boolean | string | null` (a `boolean` is stored as INTEGER 1/0)
- `ArrayValue` -- `number[] | bigint[] | boolean[] | string[]`
- `Value` -- `ScalarValue | ArrayValue`
- `ElementData` -- `Record<string, Value | undefined>`
- `QueryParam` -- `number | bigint | boolean | string | null`
- `GroupColumns` -- `Record<string, (number | bigint | string | boolean | null)[]>`, the column-oriented
  payload of `updateTimeSeriesGroup` / `updateVectorGroup` / `updateSetGroup` (and their `ByLabel` forms)
- `QuiverError` -- Error class for all Quiver operations
- `ScalarMetadata` -- Scalar attribute metadata
- `GroupMetadata` -- Vector/set/time series group metadata
- `TimeSeriesData` -- Time series column data
- `CsvOptions` -- CSV import/export options
- `DatabaseOptions` -- `{ readOnly?: boolean; consoleLevel?: number }` (a `LOG_LEVEL_*` constant)
- `LOG_LEVEL_DEBUG | INFO | WARN | ERROR | OFF`, `DATA_TYPE_INTEGER | FLOAT | STRING | DATE_TIME | NULL`
  -- constants for `DatabaseOptions.consoleLevel` and metadata `dataType`

## Development

```bash
bun install          # Install dev dependencies (@types/bun, biome)
bun test test        # Run all tests
bun run lint         # Biome linting
bun run format       # Biome formatting (auto-fix)
```

Tests discover the native library from a local `build/bin/` (tier 2). On Windows,
prepend `build/bin` to `PATH` first (the `test/test.bat` helper does this for you).

## License

MIT
