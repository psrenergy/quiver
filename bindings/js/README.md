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

- `Database.fromSchema(dbPath, schemaPath)` -- Create database from SQL schema file
- `Database.fromDatabase(dbPath, dir)` -- Create database from a model directory (migrations/ + optional ui/)
- `close()` -- Close the database connection

### Create / Delete

- `createElement(collection, data)` -- Create element, returns numeric ID
- `updateElement(collection, id, data)` -- Update element by ID
- `deleteElement(collection, id)` -- Delete element by ID

### Read (bulk)

- `readScalarIntegers(collection, attribute)` -- Read all integer scalars
- `readScalarFloats(collection, attribute)` -- Read all float scalars
- `readScalarStrings(collection, attribute)` -- Read all string scalars
- `readVectorIntegers(collection, attribute)` -- Read all integer vectors
- `readVectorFloats(collection, attribute)` -- Read all float vectors
- `readVectorStrings(collection, attribute)` -- Read all string vectors
- `readSetIntegers(collection, attribute)` -- Read all integer sets
- `readSetFloats(collection, attribute)` -- Read all float sets
- `readSetStrings(collection, attribute)` -- Read all string sets

### Read (by ID)

- `readScalarIntegerById(collection, attribute, id)` -- Read integer or null
- `readScalarFloatById(collection, attribute, id)` -- Read float or null
- `readScalarStringById(collection, attribute, id)` -- Read string or null
- `readVectorIntegersById(collection, attribute, id)` -- Read integer vector
- `readVectorFloatsById(collection, attribute, id)` -- Read float vector
- `readVectorStringsById(collection, attribute, id)` -- Read string vector
- `readSetIntegersById(collection, attribute, id)` -- Read integer set
- `readSetFloatsById(collection, attribute, id)` -- Read float set
- `readSetStringsById(collection, attribute, id)` -- Read string set

### Read (IDs)

- `readElementIds(collection)` -- Read all element IDs in a collection

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

### Query

- `queryString(sql, parameters?)` -- Query returning string or null
- `queryInteger(sql, parameters?)` -- Query returning integer or null
- `queryFloat(sql, parameters?)` -- Query returning float or null

Parameters are passed as an array of `number | string | null`.

### Transaction

- `beginTransaction()` -- Begin explicit transaction
- `commit()` -- Commit current transaction
- `rollback()` -- Rollback current transaction
- `inTransaction()` -- Check if transaction is active

### CSV

- `exportCsv(collection, group, filePath, options?)` -- Export collection group to CSV file
- `importCsv(collection, group, filePath, options?)` -- Import CSV file into collection group

### Composites

- `readScalarsById(collection, id)` -- Read all scalar attributes for an element
- `readVectorsById(collection, id)` -- Read all vector groups for an element
- `readSetsById(collection, id)` -- Read all set groups for an element

### Introspection

- `describe()` -- Print schema info to stdout
- `isHealthy()` -- Check database health
- `path()` -- Get database file path
- `currentVersion()` -- Get current schema version

### Lua

- `LuaRunner(db)` -- Create Lua script runner with database access
- `run(script)` -- Execute a Lua script
- `close()` -- Close the Lua runner

## Types

Exported types available for TypeScript consumers:

- `ScalarValue` -- `number | bigint | string | null`
- `ArrayValue` -- `number[] | bigint[] | string[]`
- `Value` -- `ScalarValue | ArrayValue`
- `ElementData` -- `Record<string, Value | undefined>`
- `QueryParam` -- `number | string | null`
- `QuiverError` -- Error class for all Quiver operations
- `ScalarMetadata` -- Scalar attribute metadata
- `GroupMetadata` -- Vector/set/time series group metadata
- `TimeSeriesData` -- Time series column data
- `CsvOptions` -- CSV import/export options

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
