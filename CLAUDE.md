# Project: Quiver

SQLite wrapper library with C++ core, C API for FFI, and language bindings (Julia, Dart, Python, JS/Bun, plus embedded Lua).

## Repo Map

This repo uses **nested CLAUDE.md files**: this root file holds everything cross-cutting; each
area's internals live in the CLAUDE.md next to it (loaded automatically when working there).

```
include/quiver/ + src/    # C++ core, Lua runner, binary + expression subsystems -> src/CLAUDE.md
include/quiver/c/ + src/c/ # C API for FFI                                       -> src/c/CLAUDE.md
bindings/julia/           # Quiver.jl (canonical; published repo is a mirror)    -> bindings/julia/CLAUDE.md
bindings/dart/            # quiverdb on pub (ffigen + native-assets hook)        -> bindings/dart/CLAUDE.md
bindings/python/          # quiverdb on PyPI (CFFI ABI-mode)                     -> bindings/python/CLAUDE.md
bindings/js/              # quiverdb on npm (Bun FFI, hand-written loader)       -> bindings/js/CLAUDE.md
tests/                    # C++/C API suites + shared SQL schemas                -> tests/CLAUDE.md
.github/                  # CI + release/publish workflows, composite actions    -> .github/CLAUDE.md
scripts/                  # build-all/test-all/clean-all.bat, format.bat, tidy.bat,
                          # generator.bat (runs all three FFI generators),
                          # assert_version.py (check + bump), validate_wheel*.py + test-wheel*.bat,
                          # ci/{dispatch_workflow.sh, native_s3.sh}, julia/generate_artifacts.jl
cmake/                    # CompilerOptions.cmake, Dependencies.cmake, Platform.cmake, quiverConfig.cmake.in
example/                  # example1.lua + example1.bat — quiver_cli/Lua CRUD demo
docs/                     # User-facing docs: introduction, rules, attributes, migrations, time_series
assets/                   # logo.svg
```

Top-level configs: `CMakePresets.json`, `.clang-format`, `.clang-tidy`, `.clangd`,
`.gitattributes`, `.pre-commit-config.yaml`, `codecov.yml`.

## Principles

- **Human-Centric**: Codebase optimized for human readability, not machine parsing
- **Status**: WIP project - breaking changes acceptable, no backwards compatibility required
- **ABI Stability**: Use Pimpl only when hiding private dependencies; plain value types use Rule of Zero
- **Target Standard**: C++20 - use modern language features where they simplify logic
- **Philosophy**: Clean code over defensive code (assume callers obey contracts, avoid excessive null checks). Simple solutions over complex abstractions. Delete unused code, do not deprecate.
- **Intelligence**: Logic resides in C++ layer. Bindings/wrappers remain thin.
- **Error Messages**: All error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own. The only exception is pre-FFI type-marshalling errors (e.g., Python/Dart rejecting an unsupported value type before the FFI call), which the C API cannot diagnose because the value never reaches it; those messages are crafted locally and should name the offending column and type.
- **Homogeneity**: Binding interfaces must be consistent and intuitive. API surface should feel uniform across wrappers.
- **Ownership**: RAII used strictly. Ownership of pointers/resources must be explicit and unambiguous.
- **Constraint**: Be critical. If code is already optimal, state that clearly. Do not invent useless suggestions just to provide output.
- All public C++ methods should be bound to C API, then to Julia/Dart/Python/JS/Lua (exception: the binary/expression subsystems are exposed only in Julia and Lua by decision — not Dart/Python/JS)
- All *.sql test schemas in `tests/schemas/`, bindings reference from there
- **Self-Updating**: Keep the CLAUDE.md nearest to your change up to date (root + `src/`, `src/c/`, `bindings/{julia,dart,python,js}/`, `tests/`, `.github/`)
- **Changelog**: user-visible changes get an entry in `CHANGELOG.md` under the current unreleased version section. Prefix a breaking one **BREAKING** and say what a caller must do about it. A `0.x` **minor** bump signals breaking changes, a **patch** bump does not — bump accordingly (all five manifests, see Versioning)

## Design Decisions

Settled questions — don't relitigate without the user; each was decided deliberately:

- **Time-series group data is column-oriented** (`{column: [values]}`) in every binding — the
  canonical cross-binding shape. The C++ core keeps row-shaped `vector<map<string, Value>>`.
- **`DatabaseOptions`** (`read_only`, `console_level`) is exposed as optional parameters on the
  open/factory methods of every binding.
- **Schema metadata loads lazily, on first use** (`Impl::require_schema`, `src/database_impl.h`).
  `Database(path, options)` — the only implementation behind `quiver_database_open` and every
  binding's `open()` — opens the file without reading the schema, so eager loading would have to
  live in the constructor, where it would validate a half-migrated database before `migrate_up`
  ran. Loading on first `require_schema` instead makes `open()` usable everywhere (it previously
  yielded a handle whose every metadata/CRUD call threw "no schema loaded") while
  `from_schema`/`from_migrations` keep validating eagerly at construction. `schema` and
  `type_validator` are `mutable` so the const readers can trigger it, and `load_schema_metadata`
  publishes neither until validation passes — a half-loaded state would survive a failed lazy load
  and crash the next call. A non-quiver database now reports the validator's actual reason.
- **The group writers are column-oriented while the group readers are row-oriented** in Dart and
  Python (`read_vector_group_by_id` returns rows; Python even adds a synthetic 0-based
  `vector_index`). The only asymmetric reader/writer pair in those bindings, and deliberate for now:
  the columnar shape is the canonical cross-binding one for group *writes* (it is what the C API
  takes), and changing either side is a breaking API change, not a fix.
- **Binary + expression subsystems are exposed in Julia and Lua only.** Dart, Python, and JS
  deliberately do not expose them (no FFI consumer); the tests-at-every-layer rule has this one
  documented exception. Lua binds the C++ classes directly via sol2 (`src/lua_runner.cpp`) with
  method syntax + string aggregation operations; pure-metadata builders live under a `quiver.*`
  namespace while file I/O is db-scoped (see cross-layer table and the sandbox decision below).
  `helper_maps.jl` is a second documented Julia-only exception (see convenience methods below).
- **Lua file operations are db-scoped and sandboxed to the database directory.** Every
  file-touching Lua operation (`db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`, `db:export_csv`,
  `db:import_csv`, `expr:save`) resolves relative paths against the directory containing the
  database file and rejects — reads and writes alike — anything that escapes it (subdirectories
  OK; checked via `weakly_canonical` with strict containment). In-memory databases (`:memory:`)
  reject all file operations. `dofile`/`loadfile` are removed from the Lua environment
  (string-form `load` stays). The enabled standard libraries are the pure-computation set
  `base`/`string`/`table`/`math`/`coroutine`/`utf8`; `os`/`io`/`package`/`debug` stay unloaded.
  Julia's standalone `open_file` is unaffected — this is LuaRunner policy (`resolve_sandboxed_path`
  in `src/lua_runner.cpp`), not binary-subsystem policy.
- **One scalar typing policy lives in C++**: an int64 is accepted for INTEGER and REAL columns
  (int-for-REAL coercion), a double only for REAL (a float into an INTEGER column is rejected), a
  string for TEXT / INTEGER-FK / DATE_TIME. `TypeValidator` (scalar create/update) and
  `value_matches_type` (time-series writes) share this rule; bindings never coerce
  schema-dependently.
- **`update_element` / `delete_element` throw on a missing id** (`"Element not found: <id> in
  collection '<c>'"`, Pattern 2) — not a silent no-op. The error surfaces through the C API error
  channel and every binding.
- **`query_*` validate parameter count**: `execute` rejects a mismatch between bound parameters and
  `?` placeholders (too few or too many) instead of binding NULL / ignoring extras.
- **Migration `down_sql` is a required feature** — do not remove the down path.
- **Dry runs live on `Database`, not on `LuaRunner`.** `begin_dry_run`/`end_dry_run`/`in_dry_run`
  hold one transaction and always roll it back; while active, the public
  `begin_transaction`/`commit`/`rollback` are absorbed (no-ops). Absorbing is the whole point —
  without it a script using `db:transaction(fn)` (the pattern the Lua reference recommends) dies on
  a nested `BEGIN`. A `dry_run(bool)` parameter on `LuaRunner::run` was implemented first and
  rejected: it duplicated transaction semantics inside the Lua *binding* layer and gated the
  feature behind Lua for no reason. Consequences, all documented rather than fixed: a nested
  rollback is **not** partial (everything is undone at the end regardless); `in_transaction()`
  still reports `true`; `import_csv` still throws its `transaction already active` precondition;
  and a caller can escape via a raw `COMMIT` through `query_*` (so `end_dry_run` guards on
  `sqlite3_get_autocommit` instead of throwing). A per-table changeset (SQLite session extension)
  was skipped — a script that wants a change count already has `SELECT total_changes()`.
- **`LuaRunner::run` returns the script's return value as a JSON string.** One encoder in C++
  (`src/lua_runner.cpp`, anonymous namespace); every binding passes the string through without
  parsing, so no binding gains a JSON dependency (Julia would have needed one). Only the first
  returned value is encoded; no `return` yields `""`, distinct from `return nil` → `"null"`.
  Non-finite numbers become `null`, a table keyed `1..n` is an array (`{}` → `[]`) and any other
  table is a key-sorted object — so a nullable bulk read, which Lua represents as `nil` holes,
  comes back as an object (`{"1":10,"3":30}`), not `[10,null,30]`. A function/coroutine/userdata
  throws, as does nesting past 32 levels (the cycle guard), output past 64 MiB (the *size* cap —
  depth alone does not bound a table that shares sub-tables), a string that is not valid UTF-8
  (JSON must be UTF-8, a Lua string need not), and a table where an integer key and a string key
  spell the same thing (two Lua keys, one JSON key — refused rather than silently dropped).
- **`import_csv` refuses to run inside an open transaction** (`PRAGMA foreign_keys` is a no-op
  mid-transaction, so nesting is unsupportable) — Pattern 1 precondition, not a silent rollback.
- **`BinaryMetadata::number_of_time_dimensions()` is derived** from `dimensions`, never stored.
- **One C API error channel**: everything (LuaRunner included) reports via
  `quiver_get_last_error`; no per-handle error channels.
- **Python's `Element` is internal**; users pass `**kwargs` to create/update.
- **JS keeps a string-based datetime surface** — no DateTime wrappers.
- **Binary `dims` parameter is the map-based form only** — indexed overloads were prototyped and
  deliberately dropped (perf rationale in `src/CLAUDE.md`).
- **Time-series group NULLs round-trip via a per-cell presence mask.** The columnar C API
  (`read`/`update`/`free_time_series_data`) carries a `uint8_t` mask parallel to the data arrays
  (NULL mask = dense; `mask[c][r] == 0` = SQL NULL, data ignored — so an all-NULL column can be
  FLOAT-tagged against any schema column). FFI bindings (Julia/Python/Dart/JS) read null-padded
  columns and accept null cells on update; their existing equal-length validations stay. Lua is
  mask- and sentinel-free: NULL is plain `nil`, the dimension column(s) are the row-count authority
  (`#ts.<dimension>`), value columns may be short/sparse/empty (missing cells write NULL),
  longer-than-dimension errors, and the named-but-empty anti-silent-clear trap is preserved
  (`{}` clears).
- **Element arrays accept NULL cells.** C++ `Element::set(name, std::vector<Value>)` stores null
  cells directly (a nullable group column, e.g. an optional relation, can have empty cells per
  row); `create_element`/`update_element` bind them as SQL NULL. The C API array setters
  (`quiver_element_set_array_{integer,float,string}`) carry the same trailing `const uint8_t*
  has_value` presence mask as the time-series writers (NULL mask = dense; `has_value[i] == 0` =
  SQL NULL, data slot ignored; a NULL `char*` entry in a string array is also NULL). Dart's
  `Element.set` accepts `List<T?>` with null cells and empty lists (an empty or all-null list is
  tagged integer — the type is irrelevant when no value is read); Julia/Python/JS pass a dense
  (NULL) mask and keep their non-null surfaces.
- **Scalar bulk reads preserve NULLs positionally (one entry per element).** `read_scalar_{integers,
  floats,strings}` return `std::vector<std::optional<T>>` in C++ (mirroring the `_by_id` variants);
  a SQL NULL is `nullopt`, aligned with `read_element_ids` by `ORDER BY rowid` — NULLs are never
  dropped. The C API numeric readers carry a parallel `uint8_t* out_mask` (`mask[i] == 0` = NULL,
  data slot is a 0/0.0 placeholder) freed by `quiver_database_free_mask`; the string reader needs
  no mask — a NULL is a `nullptr` entry in the `char**`. Python/Dart/JS always return the nullable
  element type (`list[T|None]` / `List<T?>` / `(T|null)[]`) — a static surface with no
  type-inference instability to fix. **Julia is nullability-aware**: `read_scalar_{integers,floats,
  strings}` return a concrete `Vector{T}` for `NOT NULL` columns and `Vector{Optional{T}}` for
  nullable ones (decided by `get_scalar_metadata(...).not_null`, schema- not data-driven), so a
  column that can never be NULL gives downstream code a concrete array. An `INTEGER PRIMARY KEY`
  (e.g. `id`) is a rowid alias and is reported `not_null` by the C++ core
  (`scalar_metadata_from_column`, even though SQLite's `PRAGMA table_info` leaves the flag unset),
  so `id` reads are concrete `Vector{Int64}`. Scope of that Julia rule is
  the bulk scalar readers only (their optional comes solely from NULL cells); `_by_id`/`query_*`
  (optional also from missing-id / unknown result nullability) and the time-series readers stay
  optional — tracked in `bindings/julia/type_stability_followup.md`. Lua uses
  `nil` holes (only `to_lua_table` changed; no C API mask) with `read_element_ids` as the
  count/position authority since `#t` is unreliable across holes. Scope is **scalars only** — the
  shared dense `read_column_values<T>` still serves vector/set `_by_id` and `read_element_ids`
  (NOT NULL / PK by convention); vector/set readers are unchanged.

## Do Not "Fix"

Reviewed adversarially and rejected — these are not improvements:

- Collapsing per-method FFI boilerplate in Dart/Python into closure-parameterized helpers — the
  expanded style is the de facto convention; helpers add pointer-type indirection for marginal gain.
- Deleting or "cleaning up" `tests/sandbox` — intentional scratch target.
- "Simplifying" the documented Bun FFI workarounds (`bindings/js/CLAUDE.md`) or the binary
  hot-path decisions (`src/CLAUDE.md`) — load-bearing.
- Drive-by fixing pre-existing lint debt in untouched JS files.
- Relocating the agent-facing Lua reference (`bindings/js/src/lua-api.ts`, `LUA_DB_API_REFERENCE`).
  Moving it into the C++ layer would turn a build-time constant into a runtime FFI call just to
  assemble a system prompt, and would make a one-sentence docs fix a native republish across npm
  libs, PyPI wheels, Julia artifacts, and S3 natives. Moving it to an imported `.md` was implemented,
  verified, and reverted — editing ergonomics only, at the cost of a `files`-allowlist dependency
  and a Bun text-loader floor. Neither fixes drift, which is what actually went wrong; the sync test
  does. Rationale in `bindings/js/CLAUDE.md`.

## Build & Test

### Running Python locally
Plain `python` / `python3` / `py` are not on PATH in this environment. Run Python through
`uv` instead — it provisions the interpreter and dependencies on the fly:
```bash
uv run python -c "..."                 # ad-hoc script
uv run --with pyyaml python -c "..."   # with a one-off dependency (e.g. validate a workflow YAML)
```
(CI runners invoke `python3` directly; this `uv` note is for local/agent use only.)

### Build
```bash
cmake --build build --config Debug
```
First-time configure (what `build-all.bat` does):
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON
```

### Build and Test All
```bash
scripts/build-all.bat            # Build everything + run all tests (Debug)
scripts/build-all.bat --release  # Build in Release mode
scripts/test-all.bat             # Run all tests (assumes already built)
```
`test-all.bat` runs the six suites below plus a `quiver_cli` smoke test; `build-all.bat` builds
and then runs the six suites (breakdown in `tests/CLAUDE.md`).

### Individual Tests
```bash
./build/bin/quiver_tests.exe      # C++ core library tests
./build/bin/quiver_c_tests.exe    # C API tests
bindings/julia/test/test.bat      # Julia tests
bindings/dart/test/test.bat       # Dart tests
bindings/js/test/test.bat         # JavaScript (Bun) tests
bindings/python/tests/test.bat    # Python tests
```

### Other Executables
```bash
./build/bin/quiver_cli.exe        # CLI entry point (see example/)
./build/bin/quiver_benchmark.exe  # Transaction benchmark - run manually, never in CI
```

### Regenerating FFI Bindings
When the C API changes:
```bash
scripts/generator.bat                    # all three generators in sequence, or individually:
bindings/julia/generator/generator.bat   # Julia (Clang.jl -> src/c_api.jl)
bindings/dart/generator/generator.bat    # Dart (ffigen -> lib/src/ffi/bindings.dart)
bindings/python/generator/generator.bat  # Python (prints cdecls as a diff aid for _c_api.py)
```
JS has no generator — update the hand-written symbol table in `bindings/js/src/loader.ts`.

## Build System

- **CMake ≥ 3.26, C++20.** Options: `QUIVER_BUILD_SHARED` (ON), `QUIVER_BUILD_TESTS` (ON),
  `QUIVER_BUILD_C_API` (OFF — the configure line above turns it on). When scikit-build-core
  drives the build (Python wheels) it defines the `SKBUILD` CMake variable, which forces
  C API ON and tests OFF.
- **Presets** (`CMakePresets.json`): configure `dev` (Debug, tests+C API), `release`,
  `windows-release` (VS 17 2022), `linux-release`; build presets for
  dev/release/windows-release/linux-release; test presets for dev/windows-release/linux-release.
  Presets build into `build/<presetName>/`; the plain `build/` dir is the manual configure above.
- **Dependencies** via FetchContent (`cmake/Dependencies.cmake`): sqlite3 v3.50.2
  (sjinks/sqlite3-cmake), tomlplusplus v3.4.0, spdlog v1.17.0, lua v5.4.8 (lua-cmake wrapper,
  interpreter/compiler off), sol2 v3.5.0, rapidcsv v8.92, argparse v3.2, googletest v1.17.0
  (tests only).
- **Targets**: `quiver` (core, alias `quiver::database`), `quiver_c` (alias
  `quiver::database_c`), `quiver_cli`, `quiver_tests`, `quiver_c_tests`, `quiver_benchmark`,
  `quiver_sandbox`. Outputs: executables/DLLs → `build/bin/`, libs → `build/lib/`.

## Versioning

`CMakeLists.txt` `project(... VERSION x.y.z)` is the single source of truth.
`scripts/assert_version.py` asserts that `bindings/python/pyproject.toml`,
`bindings/js/package.json`, `bindings/dart/pubspec.yaml`, and `bindings/julia/Project.toml`
all agree — bump all five together. The same script also writes them:
`scripts/assert_version.py bump major|minor|patch` rewrites all five and prints the new version,
refusing to run from a state where they already disagree. Normally you dispatch the **Bump
Version** workflow instead, which runs exactly that and opens the PR. `CHANGELOG.md` carries the
version too (`## [x.y.z] — unreleased` plus its compare link) but is edited by hand — the
release ritual for that file is not settled. Release flow: `.github/CLAUDE.md`.

## Code Style Tooling

- `scripts/format.bat` — C++ via the CMake `format` target (clang-format), then each binding's
  own `format.bat` (JuliaFormatter, dart format, ruff, biome).
- `scripts/tidy.bat` — `run-clang-tidy` over `build/compile_commands.json` (strips the MinGW-only
  `-fno-keep-inline-dllexport` flag first; skips `src/binary`).
- `.pre-commit-config.yaml` — trailing-whitespace, end-of-file, yaml/json checks, merge-conflict
  markers, large files (>1 MB), LF line endings, clang-format, cppcheck, cmake-format.
- `.gitattributes` enforces LF for `.cpp/.h/.dart/.jl/.py`. **Caution:** working-tree `.bat`
  files are CRLF — unix tools (sed et al.) silently convert them to LF and can break them;
  restore CRLF if touched.

## C++ Error Message Patterns

All `throw std::runtime_error(...)` in the C++ layer use exactly 3 message patterns:

**Pattern 1 -- Precondition failure:** `"Cannot {operation}: {reason}"`
```cpp
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
```
Validators thread the calling operation's name through so the `{operation}` is the public method
the user called (e.g., type mismatches report `"Cannot update_element: ..."`).

**Pattern 2 -- Not found:** `"{Entity} not found: {identifier}"`
```cpp
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");
```

**Pattern 3 -- Operation failure:** `"Failed to {operation}: {reason}"`
```cpp
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
```

No ad-hoc formats in the database core. Downstream layers (C API, bindings) can rely on
consistent error structure. Known exception: the binary/expression subsystem's metadata
validation throws descriptive messages (e.g. `"Number of labels must be positive, got 0"`)
that predate the pattern; new code should use the three patterns.

## Schema Conventions

### Configuration Table (Required)
Every schema must have a Configuration table:
```sql
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;
```

### Collections
```sql
CREATE TABLE MyCollection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    -- scalar attributes
) STRICT;
```

### Vector Tables
Named `{Collection}_vector_{name}` with composite PK:
```sql
CREATE TABLE Items_vector_values (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;
```

### Set Tables
Named `{Collection}_set_{name}` with UNIQUE constraint:
```sql
CREATE TABLE Items_set_tags (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    value TEXT NOT NULL,
    UNIQUE (id, value)
) STRICT;
```

### Time Series Tables
Named `{Collection}_time_series_{name}` with a dimension (ordering) column whose name starts with `date_` (e.g., `date_time`), stored as ISO 8601 text (`YYYY-MM-DDTHH:MM:SS`):
```sql
CREATE TABLE Items_time_series_data (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
```

### Time Series Files Tables
Named `{Collection}_time_series_files`, singleton table for file path references:
```sql
CREATE TABLE Items_time_series_files (
    data_file TEXT,
    metadata_file TEXT
) STRICT;
```

### Foreign Keys
Always use `ON DELETE CASCADE ON UPDATE CASCADE` for parent references.

## Core API

### Naming Convention
Public Database methods follow `verb_[category_]type[_by_id]`:
- **Verbs:** create, read, update, upsert, delete, get, list, has, query, describe, export, import
- **`_by_id` suffix:** Only for reads where both "all elements" and "single element" variants exist
- **`_by_label` suffix:** Label-addressed counterpart of an id-addressed write. Spelled out in
  **every** layer including the ones that could overload (C++, Julia, Lua) — one name everywhere,
  no type dispatch. The label form resolves and delegates to the id form; it never re-implements it.
- **Singular vs plural:** Type name matches return cardinality (`read_scalar_integers` returns vector, `read_scalar_integer_by_id` returns optional)
- **Examples:** `create_element`, `read_vector_floats_by_id`, `get_scalar_metadata`, `list_time_series_groups`

### Database Class
- Factory methods: `from_schema()`, `from_migrations()` — `DatabaseOptions` (`read_only`, `console_level`) exposed as optional parameters in every binding
- Transaction control: `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()`
- Dry runs: `begin_dry_run()`, `end_dry_run()`, `in_dry_run()` — one transaction that is always rolled back; while active the three transaction methods above are absorbed (no-ops) so nested callers compose. See the design decision below.
- CRUD: `create_element(collection, element)`, `update_element`, `delete_element`,
  `update_element_by_label(collection, label, element)`,
  `delete_element_by_label(collection, label)`
- Label-addressed writes: each `_by_label` form resolves the label within the collection
  (`Impl::resolve_label`) and delegates to its id counterpart, so everything past the lookup —
  CASCADE, the attribute writes, the validation — is the id form's. A label is unique per
  collection, not per database. Errors name the step that raised them: the lookup owns the
  Pattern 2 miss (`Element not found: label 'x' in collection 'C'`), while the delegated write
  names the id form (`Cannot update_element: ...`). Passing `label` among the attributes to
  `update_element_by_label` renames the element; the lookup has already run, so the write lands
  on the resolved id.
- Element count: `number_of_elements(collection)` returns the current row count from the
  collection's main table (`COUNT(*)`), not its maximum ID or group-row count. Any table in the
  schema is accepted, so naming a group table reports that table's own row count.
- Scalar/vector/set readers: `read_{scalar,vector,set}_{integers,floats,strings}(collection, attribute)` (+ `_by_id` variants). Scalar bulk readers return one entry per element with SQL NULLs preserved positionally (`std::optional` / `nothing`/`None`/`null`/`nil`); see the scalar-NULL design decision.
- Whole-group readers: `read_vector_group_by_id()` / `read_set_group_by_id()` — row-shaped
  `vector<map<string, Value>>` over all of a group's value columns, positionally aligned with SQL
  NULL cells preserved (`Value{nullptr}`). Use these for multi-column group reads: the dense
  per-column `_by_id` readers drop NULLs, so zipping them misaligns rows when a nullable column
  (e.g. an `ON DELETE SET NULL` relation) has empty cells. C API mirrors
  `read_time_series_group`'s columnar+mask shape (freed by `free_time_series_data`); Dart binds
  them natively; Julia/Python still compose per-column reads (null-dropping caveat applies there).
- Whole-group writers: `update_vector_group()` / `update_set_group()` — replace all of an element's
  rows in one **named** group; an empty row list clears it. The write counterpart of the readers
  above, and the unambiguous alternative to passing arrays through `update_element` /
  `create_element`: those route an array **by column name**, so when two groups of a collection
  share a column name (legal — `validate_no_duplicate_attributes` exempts FK columns) the array is
  written to *every* matching table, silently rewriting groups the caller never named. That
  fan-out is pinned by `UpdateElementSharedColumnNameWritesEveryMatchingGroup` and now logs a
  warning (`tests/schemas/valid/relations.sql` shares `parent_ref` across a vector and a set group).
  Prefer the group writers whenever one group is meant. Bound in **every layer**: C++, the C API
  (columnar + per-cell mask, same shape as `update_time_series_group`), Julia, Dart, Python, JS,
  and Lua. Validation lives in the C++ core and is shared by all of them: the caller's columns are
  checked against the group (the **union** of every row's keys, so a column named only in a later
  row is written and an unknown one still throws), `id` / `vector_index` are rejected as
  caller-supplied columns (they are derived, and emitting one would duplicate it in the INSERT
  where SQLite keeps the first occurrence — silently discarding the caller's value), the element id
  must exist (Pattern 2 `Element not found`), and validation precedes the DELETE so a rejected
  write cannot leave a cleared group even when `TransactionGuard` owns no transaction (inside a dry
  run or a caller-owned transaction). A **named column with no rows is an error, not a clear** —
  clearing is "no columns" — so a typo'd column name cannot destroy data; that guard sits in the C
  API decoder (the last layer where column names still exist) and in Lua, and covers
  `update_time_series_group` too.
  *Not yet fixed:* the deep fix for the array fan-out — rejecting `matches.size() > 1` when
  `delete_existing` — is still open. Only `bindings/julia/test/test_helper_maps.jl` depends on the
  fan-out, and via `create_element!` (the non-destructive half), so nothing blocks it; it is a
  breaking behaviour change rather than a bug fix.
- Time series: `read_time_series_group()`, `update_time_series_group()`, `upsert_time_series_row()` — group read/update use N typed value columns per group; `upsert_time_series_row` inserts or replaces a single row by its dimension key (`INSERT OR REPLACE`). All bindings expose group data **column-oriented** (`{column: [values]}`); updating with no data clears the group. Integer values are accepted for REAL columns (converted on insert). NULL cells round-trip through every layer: the C API carries a per-cell presence mask, the FFI bindings surface null-padded columns (`nothing`/`None`/`null`), and Lua uses plain `nil` holes with the row count taken from the dimension column(s) — see the design decision below.
- Time series row: `read_time_series_row(collection, group, attribute, date_time)` — one value per element using "last non-null value at or before date_time" semantics; null Value for elements with no matching data (bindings surface `nothing`/`null`/`None`/`nil`).
- Time series files: `has_time_series_files()`, `list_time_series_files_columns()`, `read_time_series_files()`, `update_time_series_files()`
- Metadata: `get_{scalar,vector,set,time_series}_metadata()` — group metadata is a unified `GroupMetadata` with `dimension_column` (populated for time series, empty for vectors/sets)
- List groups: `list_scalar_attributes()`, `list_vector_groups()`, `list_set_groups()`, `list_time_series_groups()`
- Query: `query_string/integer/float(sql, parameters = {})` - parameterized SQL with positional `?`
  placeholders. **`Row::get_float` widens an INTEGER value** (the same int64-for-REAL rule as the
  scalar typing policy), so it applies to every float read — `query_float`, `read_scalar_floats`,
  `read_scalar_float_by_id`, `read_vector_floats_by_id`, `read_set_floats_by_id` — not just queries.
  Otherwise `SELECT COUNT(*)` / `SUM(int_col)`, which SQLite answers as INTEGER, and an integer
  stored in a REAL column, all read back as "no value". `query_integer` does **not** narrow a REAL;
  that direction is lossy.
- Schema inspection — human-readable **text reports** (all return `std::string`): `describe()` (whole-DB overview: every collection, element counts, attribute/group names); `describe_collection(c)` (one collection's structure); `summarize_collection(c)` (per-scalar null/non-null counts + low-cardinality integer value distributions, per-group empty/non-empty counts). CSV: `export_csv()`, `import_csv()` with optional enum/date formatting via `CSVOptions`.
  **Export and import are symmetric on foreign keys**: a FK column is written as the referenced
  element's `label` and read back by label (self-references are excluded on both sides, since the
  target rows are the ones being rewritten). Export used to emit the raw integer id, which import
  then rejected — so any table with a relation could not round-trip. Floats are written with
  `std::to_chars` (shortest exact round-trip); `%g` was used first and silently truncated to 6
  significant digits.

### Element Class
Builder for element creation with fluent API:
```cpp
Element().set("label", "Item 1").set("value", 42).set("tags", {"a", "b"})
```

### Binary & Expression Subsystems (Julia + Lua)
`.qvr` binary file I/O with `.toml` metadata sidecars (`BinaryFile`, `CSVConverter`,
`BinaryMetadata`) and lazy arithmetic expressions over them (`Expression` DAGs with
broadcast, aggregation, and label projection, materialized via `save()`). Exposed in Julia
(FFI) and Lua (sol2). Full reference: `src/CLAUDE.md`.

### LuaRunner Class
Executes Lua scripts against a database; the `db` userdata exposes the same API surface
(see cross-layer tables below). `run(script)` returns the script's return value encoded as
**JSON** (empty string if it returned nothing) — every binding passes that string through
verbatim. Implementation notes: `src/CLAUDE.md`.

## Cross-Layer Naming Conventions

### Transformation Rules

- **C++ to C API:** Prefix `quiver_database_` to the C++ method name. Example: `create_element` -> `quiver_database_create_element`
- **C++ to Julia:** Same name. Add `!` suffix for mutating operations (create, update, delete). Example: `create_element` -> `create_element!`
- **C++ to Dart:** Convert `snake_case` to `camelCase`. Factory methods use named constructors: `from_schema` -> `Database.fromSchema()`
- **C++ to Python:** Same `snake_case` name. Factory methods are `@staticmethod`. Properties are regular methods (not `@property`). Create/update use `**kwargs`: `create_element("Collection", label="x")`.
- **C++ to JS:** Same camelCase rule as Dart, with `Csv` cased as `exportCsv`/`importCsv`.
- **C++ to Lua:** Same name exactly (1:1 match). Lua has no lifecycle methods (open/close) -- database is provided as `db` userdata by LuaRunner.

The rules are mechanical: given any C++ method name, you can derive the equivalent in any layer.

### Representative Cross-Layer Examples

| Category | C++ | C API | Julia | Dart | Lua |
|----------|-----|-------|-------|------|-----|
| Factory | `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | N/A |
| Open existing | `Database(path, options)` | `quiver_database_open()` | `open()` | `Database.open()` | N/A |
| Transaction | `begin_transaction()` | `quiver_database_begin_transaction()` | `begin_transaction!()` | `beginTransaction()` | `begin_transaction()` |
| Transaction | `commit()` | `quiver_database_commit()` | `commit!()` | `commit()` | `commit()` |
| Transaction | `rollback()` | `quiver_database_rollback()` | `rollback!()` | `rollback()` | `rollback()` |
| Transaction | `in_transaction()` | `quiver_database_in_transaction()` | `in_transaction()` | `inTransaction()` | `in_transaction()` |
| Dry run | `begin_dry_run()` | `quiver_database_begin_dry_run()` | `begin_dry_run!()` | `beginDryRun()` | `begin_dry_run()` |
| Dry run | `end_dry_run()` | `quiver_database_end_dry_run()` | `end_dry_run!()` | `endDryRun()` | `end_dry_run()` |
| Dry run | `in_dry_run()` | `quiver_database_in_dry_run()` | `in_dry_run()` | `inDryRun()` | `in_dry_run()` |
| Create | `create_element()` | `quiver_database_create_element()` | `create_element!()` | `createElement()` | `create_element()` |
| Read scalar | `read_scalar_integers()` | `quiver_database_read_scalar_integers()` | `read_scalar_integers()` | `readScalarIntegers()` | `read_scalar_integers()` |
| Read by Id | `read_scalar_integer_by_id()` | `quiver_database_read_scalar_integer_by_id()` | `read_scalar_integer_by_id()` | `readScalarIntegerById()` | N/A (use composites) |
| Update | `update_element()` | `quiver_database_update_element()` | `update_element!()` | `updateElement()` | `update_element()` |
| Update by label | `update_element_by_label()` | `quiver_database_update_element_by_label()` | `update_element_by_label!()` | `updateElementByLabel()` | `update_element_by_label()` |
| Delete | `delete_element()` | `quiver_database_delete_element()` | `delete_element!()` | `deleteElement()` | `delete_element()` |
| Delete by label | `delete_element_by_label()` | `quiver_database_delete_element_by_label()` | `delete_element_by_label!()` | `deleteElementByLabel()` | `delete_element_by_label()` |
| Element count | `number_of_elements()` | `quiver_database_number_of_elements()` | `number_of_elements()` | `numberOfElements()` | `number_of_elements()` |
| Metadata | `get_scalar_metadata()` | `quiver_database_get_scalar_metadata()` | `get_scalar_metadata()` | `getScalarMetadata()` | `get_scalar_metadata()` |
| List groups | `list_vector_groups()` | `quiver_database_list_vector_groups()` | `list_vector_groups()` | `listVectorGroups()` | `list_vector_groups()` |
| Time series read | `read_time_series_group()` | `quiver_database_read_time_series_group()` | `read_time_series_group()` | `readTimeSeriesGroup()` | `read_time_series_group()` |
| Time series row | `read_time_series_row()` | `quiver_database_read_time_series_row()` | `read_time_series_row()` | `readTimeSeriesRow()` | `read_time_series_row()` |
| Time series upsert row | `upsert_time_series_row()` | `quiver_database_upsert_time_series_row()` | `upsert_time_series_row!()` | `upsertTimeSeriesRow()` | `upsert_time_series_row()` |
| Time series update | `update_time_series_group()` | `quiver_database_update_time_series_group()` | `update_time_series_group!()` | `updateTimeSeriesGroup()` | `update_time_series_group()` |
| Vector group update | `update_vector_group()` | `quiver_database_update_vector_group()` | `update_vector_group!()` | `updateVectorGroup()` | `update_vector_group()` |
| Set group update | `update_set_group()` | `quiver_database_update_set_group()` | `update_set_group!()` | `updateSetGroup()` | `update_set_group()` |
| Query | `query_string()` | `quiver_database_query_string()` | `query_string()` | `queryString()` | `query_string()` |
| CSV | `export_csv()` | `quiver_database_export_csv()` | `export_csv()` | `exportCSV()` | `export_csv()` |
| Describe (text) | `describe()` | `quiver_database_describe()` | `describe()` | `describe()` | `describe()` |
| Describe collection | `describe_collection()` | `quiver_database_describe_collection()` | `describe_collection()` | `describeCollection()` | `describe_collection()` |
| Summarize collection | `summarize_collection()` | `quiver_database_summarize_collection()` | `summarize_collection()` | `summarizeCollection()` | `summarize_collection()` |

**Binary cross-layer examples (Julia + Lua subsystem):**

| Category | C++ | C API | Julia | Lua |
|----------|-----|-------|-------|-----|
| Open file | `BinaryFile::open_file(path, mode, md?)` | `quiver_binary_file_open_file()` | `open_file(path; mode, metadata=nothing)` | `db:open_file(path, mode, md?)` |
| Close | (destructor) | `quiver_binary_file_close()` | `close!(file)` | `file:close()` |
| Read | `binary_file.read(dims)` | `quiver_binary_file_read()` | `read(file; dims...)` | `file:read(dims, allow_nulls?)` |
| Write | `binary_file.write(data, dims)` | `quiver_binary_file_write()` | `write!(file; data=data, dims...)` | `file:write(data, dims)` |
| Get metadata | `binary_file.get_metadata()` | `quiver_binary_file_get_metadata()` | `get_metadata(file)` | `file:get_metadata()` |
| Get file path | `binary_file.get_file_path()` | `quiver_binary_file_get_file_path()` | `get_file_path(file)` | `file:get_file_path()` |
| Bin to CSV | `CSVConverter::bin_to_csv()` | `quiver_csv_converter_bin_to_csv()` | `bin_to_csv()` | `db:bin_to_csv(path, aggregate?)` |
| CSV to bin | `CSVConverter::csv_to_bin()` | `quiver_csv_converter_csv_to_bin()` | `csv_to_bin()` | `db:csv_to_bin(path)` |
| Metadata builder | `BinaryMetadata{}` | `quiver_binary_metadata_create()` | `Metadata(; kwargs...)` | `quiver.metadata{kwargs}` |
| Metadata from TOML | `BinaryMetadata::from_toml_content()` | `quiver_binary_metadata_from_toml()` | `from_toml_content()` | `quiver.metadata_from_toml()` |
| Metadata from Element | `BinaryMetadata::from_element()` | `quiver_binary_metadata_from_element()` | `from_element()` | `quiver.metadata_from_element()` |

The Lua **expression** surface mirrors Julia's: build from a file with `quiver.expression(file)` (or
operate on files directly), compose with the `+ - * /` operators and unary `-` (metamethods, with
scalar-on-either-side and `file_a + file_b` both supported), unary math via `quiver.abs/sqrt/log/exp`,
element-wise comparisons via `quiver.gt/lt/gte/lte/eq/neq` (free functions — Lua comparison
metamethods can't return an `Expression`; produce `1.0`/`0.0`, NaN operand → NaN), boolean logic
via the `&` / `|` / `~` operators (bitwise metamethods, since `and`/`or`/`not` are Lua keywords;
nonzero is true, unitless result, NaN propagates), `quiver.ifelse(cond, then, else)`, and the
methods `expr:aggregate(dim, op[, p])` /
`expr:aggregate_agents(op[, p])` / `expr:select_agents(labels)` / `expr:rename_agents({old=new})` /
`expr:save(path)` / `expr:metadata()`. Aggregation `op` is a **string**
(`"sum"/"mean"/"min"/"max"/"percentile"`) — Lua has no enums, mirroring JS's string-based surface.
Lua file I/O is db-scoped (`db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`) and `expr:save` paths
are sandboxed to the database directory (see Design Decisions).

### Binding-Only Convenience Methods

The bindings provide additional convenience methods that compose core operations. These have no direct C++ or C API counterpart. (JS deliberately keeps a string-based datetime surface — no DateTime wrappers.)

**DateTime wrappers (Julia, Dart, and Python):**

|             Julia             |           Dart            |             Python            |               Wraps               |
| ----------------------------- | ------------------------- | ----------------------------- | --------------------------------- |
| `read_scalar_date_time_by_id` | `readScalarDateTimeById`  | `read_scalar_date_time_by_id` | string read + date parsing        |
| `read_vector_date_time_by_id` | `readVectorDateTimesById` | `read_vector_date_time_by_id` | string vector read + date parsing |
| `read_set_date_time_by_id`    | `readSetDateTimesById`    | `read_set_date_time_by_id`    | string set read + date parsing    |
| `query_date_time`             | `queryDateTime`           | `query_date_time`             | string query + date parsing       |

**Composite read helpers (all five bindings):**

|        Julia         |       Dart        |        Python        |         Lua          |        JS         |                 Wraps                  |
| -------------------- | ----------------- | -------------------- | -------------------- | ----------------- | -------------------------------------- |
| `read_scalars_by_id` | `readScalarsById` | `read_scalars_by_id` | `read_scalars_by_id` | `readScalarsById` | `list_scalar_attributes` + typed reads |
| `read_vectors_by_id` | `readVectorsById` | `read_vectors_by_id` | `read_vectors_by_id` | `readVectorsById` | `list_vector_groups` + typed reads     |
| `read_sets_by_id`    | `readSetsById`    | `read_sets_by_id`    | `read_sets_by_id`    | `readSetsById`    | `list_set_groups` + typed reads        |
| `read_element_by_id` | `readElementById` | `read_element_by_id` | `read_element_by_id` | N/A               | scalars + vectors + sets merged        |

**Relation map helpers (Julia only):** `bindings/julia/src/helper_maps.jl` provides
`scalar_relation_map` / `set_relation_map`, which derive the FK column name from the naming
convention (`lowercase(collection_to) * "_" * relation_type`) and map each element to the
positional index of its related element. A documented Julia-only convenience for PSR's
modeling tooling — a deliberate exception to the thin-bindings rule, kept in the binding
because only Julia consumers use it.

**Transaction block wrappers (Julia, Dart, Python, and Lua):**

| Julia | Dart | Python | Lua | Wraps |
|-------|------|--------|-----|-------|
| `transaction(db) do db...end` | `db.transaction((db) {...})` | `with db.transaction():` | `db:transaction(function(db)...end)` | begin + fn + commit/rollback |
| `dry_run(db) do db...end` | `db.dryRun((db) {...})` | `with db.dry_run():` | `db:dry_run(function(db)...end)` | begin_dry_run + fn + end_dry_run |

**Multi-column group readers (Julia, Dart, and Python):**

| Julia | Dart | Python | Wraps |
|-------|------|--------|-------|
| `read_vector_group_by_id` | `readVectorGroupById` | `read_vector_group_by_id` | Julia/Python: metadata + per-column vector reads; Dart: native C++ `read_vector_group_by_id` (NULL-preserving) |
| `read_set_group_by_id` | `readSetGroupById` | `read_set_group_by_id` | Julia/Python: metadata + per-column set reads; Dart: native C++ `read_set_group_by_id` (NULL-preserving) |
