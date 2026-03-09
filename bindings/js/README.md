# quiverdb

Bun-native SQLite wrapper binding for Quiver via bun:ffi.

## Requirements

- Bun >= 1.0.0
- `libquiver.dll` and `libquiver_c.dll` in system PATH

## Installation

```bash
bun add quiverdb
```

The native Quiver DLLs must be compiled separately and available in your system PATH. See the Quiver project build instructions for details.

## DLL Setup

This binding loads the Quiver C API library (`libquiver_c.dll`) at runtime via `bun:ffi`. The core library (`libquiver.dll`) is a dependency that must also be loadable.

Options for making the DLLs available:

1. Add the `build/bin/` directory to your system PATH
2. Copy both DLLs to a directory already in PATH
3. Place both DLLs alongside your application entry point

## Quick Start

```typescript
import { Database } from "quiverdb";

const db = Database.fromSchema("my.db", "schema.sql");
db.createElement("Items", { label: "Item 1", value: 42 });

const values = db.readScalarIntegers("Items", "value");
console.log(values); // [42]

db.close();
```

## API Methods

### Lifecycle

- `Database.fromSchema(dbPath, schemaPath)` -- Create database from SQL schema file
- `Database.fromMigrations(dbPath, migrationsPath)` -- Create database from migrations directory
- `close()` -- Close the database connection

### Create / Delete

- `createElement(collection, data)` -- Create element, returns numeric ID
- `deleteElement(collection, id)` -- Delete element by ID

### Read (bulk)

- `readScalarIntegers(collection, attribute)` -- Read all integer scalars
- `readScalarFloats(collection, attribute)` -- Read all float scalars
- `readScalarStrings(collection, attribute)` -- Read all string scalars

### Read (by ID)

- `readScalarIntegerById(collection, attribute, id)` -- Read integer or null
- `readScalarFloatById(collection, attribute, id)` -- Read float or null
- `readScalarStringById(collection, attribute, id)` -- Read string or null

### Read (IDs)

- `readElementIds(collection)` -- Read all element IDs in a collection

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

## Types

Exported types available for TypeScript consumers:

- `ScalarValue` -- `number | bigint | string | null`
- `ArrayValue` -- `number[] | bigint[] | string[]`
- `Value` -- `ScalarValue | ArrayValue`
- `ElementData` -- `Record<string, Value | undefined>`
- `QueryParam` -- `number | string | null`
- `QuiverError` -- Error class for all Quiver operations

## Development

```bash
bun test         # Run all tests
bun run typecheck # TypeScript type checking (tsc --noEmit)
bun run lint     # Biome linting
bun run format   # Biome formatting (auto-fix)
```

## License

MIT
