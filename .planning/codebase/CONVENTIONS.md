# Coding Conventions

**Analysis Date:** 2026-10-02

Authoritative sources: root `AGENTS.md` (principles, error patterns, cross-layer naming), plus nested
`src/AGENTS.md`, `src/c/AGENTS.md`, `bindings/{julia,dart,python,js}/AGENTS.md`, `tests/AGENTS.md`.
Keep the AGENTS.md nearest to a change up to date; add a `CHANGELOG.md` entry for user-visible changes.

## Naming Patterns

**Files:**
- C++ core: `snake_case.cpp`, split per operation area: `src/database_create.cpp`, `src/database_read.cpp`, `src/database_delete.cpp`, `src/database_time_series.cpp`. Private headers next to sources (`src/database_impl.h`, `src/schema_validator.h`); public headers in `include/quiver/` (`include/quiver/database.h`), C headers in `include/quiver/c/`.
- C API: mirrors core file split under `src/c/` (`src/c/database_create.cpp`), shared internals in `src/c/internal.h`, `src/c/database_helpers.h`.
- Julia: `bindings/julia/src/database_<area>.jl`; Dart: `bindings/dart/lib/src/database_<area>.dart` (`part of 'database.dart'`); Python: `bindings/python/src/quiverdb/<module>.py` (private modules prefixed `_`: `_helpers.py`, `_c_api.py`, `_loader.py`); JS: `bindings/js/src/<area>.ts` in kebab/short names (`create.ts`, `read.ts`, `time-series.ts`, `ffi-helpers.ts`).

**Functions (C++ public API):** `verb_[category_]type[_by_id]` in `lower_case`
- Verbs: create, read, update, upsert, delete, get, list, has, query, describe, export, import.
- Singular/plural matches cardinality: `read_scalar_integers` (vector) vs `read_scalar_integer_by_id` (optional).
- `_by_id` only when both all/single variants exist; `_by_label` for label-addressed writes, spelled out in every layer (no overload dispatch), always resolving then delegating to the id form:
```cpp
void Database::delete_element_by_label(const std::string& collection, const std::string& label) {
    delete_element(collection, impl_->resolve_label(collection, label, "delete_element_by_label"));
}
```

**Cross-layer derivation (mechanical, apply exactly):**
- C API: `quiver_database_` + C++ name (`quiver_database_create_element`).
- Julia: same name, `!` suffix for mutators (`create_element!`, `delete_element_by_label!`, `begin_transaction!`).
- Dart: camelCase, factories as named constructors (`Database.fromSchema()`).
- Python: same snake_case; factories `@staticmethod`; no `@property`; create/update take `**kwargs`.
- JS: camelCase like Dart, with `exportCsv`/`importCsv`.
- Lua: identical name to C++.

**Variables / members (enforced by `.clang-tidy` `readability-identifier-naming`):**
- Variables, parameters, functions, methods, namespaces, constexpr: `lower_case`.
- Private members: `lower_case_` suffix (`impl_`).
- Classes/structs/enums/enum constants: `CamelCase` (`LogLevel::Off`, `BinaryMetadata`).

**Types:**
- C API types: `quiver_<name>_t` (`quiver_database_t`, `quiver_error_t`, `quiver_database_options_t`); constants `QUIVER_OK`, `QUIVER_ERROR`, `QUIVER_LOG_OFF`.
- Bindings: `Database`, `Element`, `LuaRunner`; exceptions `DatabaseException` (Julia, Dart), `QuiverError` (Python, JS).

## Code Style

**Formatting:**
- Run `scripts/format.bat` (C++ via CMake `format` target, then each binding's `format.bat`).
- C++: `.clang-format`, clang-format **pinned 22.1.8** (CI via `uvx`, pre-commit `mirrors-clang-format`, CMake target prefers `clang-format-22`). LLVM base, `ColumnLimit: 120`, `IndentWidth: 4`, `PointerAlignment: Left`, `InsertBraces: true`, BlockIndent on all brackets (close paren on its own line), `BinPackArguments: false`, `BinPackParameters: OnePerLine`, `AllowShortFunctionsOnASingleLine: Empty`, `WrapNamespaceBodyWithEmptyLines: Always`, two spaces before trailing comments.
- Julia: JuliaFormatter via PSR `Style.jl` (`bindings/julia/format/format.jl`, `format.bat`).
- Dart: `dart format`, `page_width: 120`, `trailing_commas: preserve` (`bindings/dart/analysis_options.yaml`).
- Python: ruff (`bindings/python/ruff.toml`): `line-length = 120`, double quotes, LF, `target-version = "py313"`.
- JS: biome (`bindings/js/biome.json`): 2-space indent, `lineWidth: 100`, double quotes, semicolons always.
- All text files LF (`.gitattributes` `* text=auto eol=lf`); `tests/fixtures/*.csv` are `-text` byte-exact.

**Linting:**
- C++: `.clang-tidy` (`bugprone-*`, `modernize-*`, `performance-*`, naming, `readability-redundant-*`, `readability-simplify-*`), run via `scripts/tidy.bat` (skips `src/binary`). cppcheck in `.pre-commit-config.yaml`.
- Dart: `package:lints/recommended.yaml`, excludes generated `lib/src/ffi/bindings.dart`.
- Python: ruff lint selects only `I` (isort, `combine-as-imports`).
- JS: biome `recommended`. Do not drive-by fix pre-existing lint debt in untouched JS files.

**Bracket layout example (C API, `src/c/database_create.cpp`):**
```cpp
QUIVER_C_API quiver_error_t quiver_database_create_element(
    quiver_database_t* db,
    const char* collection,
    quiver_element_t* element,
    int64_t* out_id
) {
    QUIVER_REQUIRE(db, collection, element, out_id);
    ...
}
```

## Import Organization

**C++ order (`.clang-format` `IncludeBlocks: Regroup`):**
1. Main/project headers `"..."` (`"database_impl.h"`, `"quiver/database.h"`, `"test_utils.h"`)
2. Third-party `<lib/...>` (`<gtest/gtest.h>`, `<quiver/database.h>`, `<spdlog/spdlog.h>`, `<sqlite3.h>`)
3. Standard library `<bare>` (`<map>`, `<string>`)

**Python:** `from __future__ import annotations` first, then stdlib, third-party, first-party (`quiverdb`); ruff isort enforces.

**JS:** `bun:test` / `node:*` first, then relative `../src/index.ts`; use `import type` for types.

**Path aliases:** None.

## Error Handling

**C++ core — exactly 3 message patterns, `throw std::runtime_error(...)`:**
- Pattern 1 precondition: `"Cannot {operation}: {reason}"` — `{operation}` is the public method the user called; validators thread the op name through (`impl_->require_collection(collection, "delete_element")`).
- Pattern 2 not found: `"{Entity} not found: {identifier}"` — e.g. `"Element not found: label 'No Such Item' in collection 'Collection'"`.
- Pattern 3 operation failure: `"Failed to {operation}: {reason}"`.
- No ad-hoc formats (binary metadata validation is the legacy exception).
- Writers validate all input before their first INSERT/UPDATE/DELETE (no SAVEPOINTs).

**C API — catch-all + thread-local last error:**
```cpp
QUIVER_REQUIRE(db, collection, element, out_id);   // "Null argument: <name>"
try {
    *out_id = db->db.create_element(collection, element->element);
    return QUIVER_OK;
} catch (const std::exception& e) {
    quiver_set_last_error(e.what());
    return QUIVER_ERROR;
}
```
Results via out-parameters; return value is always `quiver_error_t`. One error channel: `quiver_get_last_error`.

**Bindings — one `check(err)` per binding, never craft messages:**
- Julia `bindings/julia/src/exceptions.jl`: `check(C.quiver_database_delete_element(db.ptr, collection, id))` throws `DatabaseException`.
- Dart `bindings/dart/lib/src/exceptions.dart`: `check(...)` throws `DatabaseException`.
- Python `bindings/python/src/quiverdb/_helpers.py`: `check(err)` raises `QuiverError`.
- JS `bindings/js/src/errors.ts`: `check(err)` throws `QuiverError`.
- Only exception: pre-FFI type-marshalling errors and boolean/DateTime strict-conversion errors, crafted locally and naming `collection.attribute` (`ArgumentError` Julia/Dart, `ValueError` Python, `RangeError` JS).

**Philosophy:** clean over defensive — assume callers obey contracts, no excessive null checks; RAII strictly.

## Logging

**Framework:** spdlog (`src/database.cpp`, logger on `Impl`), level from `DatabaseOptions::console_level` (`quiver::LogLevel`).

**Patterns:**
- `impl_->logger->debug(...)` at operation entry, `impl_->logger->info(...)` on success, `warn` for suspicious-but-legal behavior (e.g. array fan-out to multiple groups).
```cpp
impl_->logger->debug("Deleting element {} from collection: {}", id, collection);
...
impl_->logger->info("Deleted element {} from {}", id, collection);
```
- Bindings do not log, except a warning when `quiver_get_last_error()` is empty (Julia `@warn`, Dart `print`).

## Comments

**When to Comment:**
- Explain *why* (design decisions, rejected alternatives, invariants), not what. Long rationale belongs in the nearest AGENTS.md; code comments point at it.
- Tests carry a comment stating the behavior pinned, e.g. `// Deleting an unresolvable label throws "Element not found" rather than silently no-op'ing`.

**Doc comments:**
- C++: `//` comments above declarations; no Doxygen enforcement.
- Julia: docstrings `"""  check(err::C.quiver_error_t) ... """`.
- Dart: `///` (`/// Label-addressed counterpart of [deleteElement].`).
- Python: one-line docstrings `"""Delete an element by ID."""`; full type hints on all signatures.
- JS: `/** ... */` JSDoc on prototype methods.

## Function Design

**Size:** Small; each public C++ method in its area file, shared logic in `Impl` (`src/database_impl.h`: `require_collection`, `require_element`, `resolve_label`, `execute`, `prepare_group_data`).

**Parameters:** `const std::string&` for names; `int64_t` ids; `std::optional` for nullable values; options as designated-initializer struct `{.read_only = false, .console_level = quiver::LogLevel::Off}`.

**Return Values:** bulk reads return `std::vector<std::optional<T>>` aligned with `read_element_ids` (NULLs preserved positionally); `_by_id` returns `std::optional<T>`. C API returns `quiver_error_t`, data via `out_*` params and presence masks (`uint8_t*`), freed with matching `quiver_database_free_*`.

**Binding method shape (keep expanded, do NOT collapse into helpers):**
- Dart: extension per area (`extension DatabaseDelete on Database`), `_ensureNotClosed()`, `Arena` + `try/finally arena.releaseAll()`.
- Python: `self._ensure_open()`, `lib = get_lib()`, `check(lib.quiver_...(self._ptr, s.encode("utf-8"), ...))`.
- JS: `Database.prototype.x = function (this: Database, ...) {...}` with a `declare x: (...) => void;` in `bindings/js/src/database.ts`; `toCString`, `BigInt(id)`.
- Julia: `function delete_element!(db::Database, collection::String, id::Int64) check(...); return nothing end`.

## Module Design

**Exports:** C++ public API via `include/quiver/*.h` with `QUIVER_API` (`include/quiver/export.h`); C via `QUIVER_C_API` inside `extern "C" { }`. Pimpl only to hide private deps (`Database::impl_`); value types use Rule of Zero.

**Barrel Files:** `include/quiver/quiver.h`; `bindings/js/src/index.ts`; `bindings/python/src/quiverdb/__init__.py`; `bindings/dart/lib/quiverdb.dart`; `bindings/julia/src/Quiver.jl` (includes all files).

**Rules:** logic lives in C++; bindings stay thin. Every public C++ method is bound to C API and all bindings + Lua (binary/expression: Julia + Lua only). Delete unused code, do not deprecate. Breaking changes acceptable (0.x minor bump).

---

*Convention analysis: 2026-10-02*
