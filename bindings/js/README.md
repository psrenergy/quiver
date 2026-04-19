# quiverdb

SQLite wrapper binding for Quiver via Deno FFI (`Deno.dlopen`).

## Requirements

- Deno >= 2.0
- `libquiver.dll` and `libquiver_c.dll` (or `.so`/`.dylib`) accessible (see [Native Library Setup](#native-library-setup))

## Installation

This is a Deno module imported from a local path:

```typescript
import { Database } from "./src/index.ts";
```

The native Quiver libraries must be compiled separately. See the Quiver project build instructions.

## Deno Permissions

This binding uses Deno FFI to load native C libraries at runtime. Deno requires explicit permission flags:

| Flag | Why |
|------|-----|
| `--allow-ffi` | Required to call `Deno.dlopen()` and load the native Quiver C library |
| `--allow-read` | Required to locate the native library on disk and read schema/data files |
| `--allow-write` | Required when creating or modifying databases (and to cache the native library when imported from JSR) |
| `--allow-env` | Required to resolve the cache directory (`XDG_CACHE_HOME`/`HOME`/`LOCALAPPDATA`) when imported from JSR |
| `--allow-net=jsr.io` | Required on first run when imported from JSR to download the bundled native library |

Unlike runtimes where any package can load native code silently, Deno requires explicit user consent for FFI access. `--allow-ffi` is a security feature -- you control exactly which programs can call into native libraries.

Example (local install, libs already on disk):

```bash
deno run --allow-ffi --allow-read --allow-write my_script.ts
```

Example (JSR install, first run downloads the bundled native library to a local cache):

```bash
deno run --allow-ffi --allow-read --allow-write --allow-env --allow-net=jsr.io my_script.ts
```

## Native Library Setup

The loader searches for native libraries in four tiers:

1. **Bundled**: `libs/{os}-{arch}/` directory next to the binding source (used for local installs and JSR installs after the first-run download completes).
2. **Dev mode**: Walks up directories looking for `build/bin/` (auto-discovered during development).
3. **JSR download**: When imported from `jsr:` or `https:`, fetches `libs/{os}-{arch}/libquiver(_c).{dll,so}` from the package's served URL and caches it under `$XDG_CACHE_HOME/psrenergy-quiver/` (Unix) or `$LOCALAPPDATA/psrenergy-quiver/` (Windows). Subsequent runs reuse the cache.
4. **System PATH**: Falls back to loading by library name from system PATH.

Currently prebuilt binaries ship for `linux-x86_64` and `windows-x86_64`. On other platforms, build from source and point to `build/bin/` via tier 2.

## Quick Start

```typescript
import { Database } from "./src/index.ts";

const db = Database.fromSchema("my.db", "schema.sql");
db.createElement("Items", { label: "Item 1", value: 42 });

const values = db.readScalarIntegers("Items", "value");
console.log(values); // [42]

db.close();
```

Run with:

```bash
deno run --allow-ffi --allow-read --allow-write example.ts
```

## API Methods

### Lifecycle

- `Database.fromSchema(dbPath, schemaPath)` -- Create database from SQL schema file
- `Database.fromMigrations(dbPath, migrationsPath)` -- Create database from migrations directory
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

- `queryString(sql, params?)` -- Query returning string or null
- `queryInteger(sql, params?)` -- Query returning integer or null
- `queryFloat(sql, params?)` -- Query returning float or null

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
deno task test       # Run all tests (includes --allow-ffi --allow-read --allow-write --allow-env)
deno task lint       # Biome linting
deno task format     # Biome formatting (auto-fix)
```

Or run tests directly:

```bash
deno test --allow-ffi --allow-read --allow-write --allow-env test/
```

## License

MIT
