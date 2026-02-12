# Domain Pitfalls: Time Series Update Interface Redesign

**Domain:** C FFI multi-column interface redesign with Julia/Dart/Lua binding migration
**Researched:** 2026-02-12
**Confidence:** HIGH (based on direct codebase analysis of all four layers)

## Context

The current `update_time_series_group` C API accepts 2 parallel arrays (`const char* const* date_times`, `const double* values`) for a single-column time series. The redesign adds support for N typed columns through the C boundary, changes Julia bindings from `Dict[]` to kwargs, and changes Dart bindings from `List<Map>` to named parameters. The C++ layer already supports multi-column via `std::vector<std::map<std::string, Value>>`, so the redesign primarily affects the C API and bindings.

---

## Critical Pitfalls

Mistakes that cause memory corruption, data loss, or require rewrites.

### Pitfall 1: Partial Allocation Leak on Exception in Multi-Column C API

**What goes wrong:** The new C API `update_time_series_group` must marshal N columns of varying types from parallel arrays into `std::vector<std::map<std::string, Value>>`. If column 3 of 5 causes an exception (e.g., invalid type tag, null pointer dereference), columns 1 and 2 have already been converted and pushed into the vector. The existing pattern catches the exception and returns `QUIVER_ERROR`, but the caller's allocated arrays are never freed because the C API was not responsible for them on the input side. This is fine for input arrays (caller owns them). However, the real danger is on the **read** side: if the new `read_time_series_group` allocates output arrays for columns 1 and 2, then fails on column 3, the partially allocated arrays leak because the caller never receives pointers to free them.

**Why it happens:** The current `read_time_series_group` allocates exactly 2 arrays (`out_date_times` and `out_values`). The new multi-column version must allocate N arrays of varying types. If allocation or conversion of column K fails, columns 0..K-1 are leaked because: (1) the function returns `QUIVER_ERROR`, (2) the caller checks error and skips the free call, (3) the partially-filled output pointers may or may not have been written.

**Evidence from codebase:** Look at `database_time_series.cpp` lines 79-80 -- the current read allocates `new char*[rows.size()]` and `new double[rows.size()]` separately. If the second `new` throws `std::bad_alloc`, the first allocation leaks. This is tolerable for 2 allocations (very unlikely) but becomes a real risk with N allocations for N columns.

**Consequences:** Memory leaks in long-running processes. Under address sanitizer, intermittent test failures. In production bindings (Julia/Dart), leaked native memory not tracked by GC.

**Prevention:**
1. Allocate all output arrays in a single allocation block and write them to output parameters only after all succeed. Use a local cleanup guard:
   ```cpp
   // Allocate everything, only assign to out_* after all succeed
   std::vector<void*> col_data;  // local, RAII-cleaned on exception
   for (size_t c = 0; c < col_count; ++c) {
       col_data.push_back(allocate_column(c, row_count));
   }
   // All allocations succeeded, now write to out params (no throw possible)
   for (size_t c = 0; c < col_count; ++c) {
       out_columns[c] = col_data[c];
   }
   ```
2. Alternatively, use a struct-based return (a `quiver_time_series_result_t`) with a single free function, keeping alloc/free co-located and atomic.
3. Never write to caller-visible output parameters until all allocations have succeeded.

**Detection:** Run address sanitizer on read tests with schemas that have 3+ value columns. Test the error path where column N-1 has invalid data type.

**Phase to address:** C API redesign phase (the multi-column read/update implementation).

---

### Pitfall 2: Type Tag / Data Array Mismatch Across C Boundary

**What goes wrong:** The multi-column C API will use parallel arrays: `column_names[]`, `column_types[]`, and `column_data[]` (where `column_data[i]` is `void*` pointing to a typed array). If `column_types[i]` says `QUIVER_DATA_TYPE_INTEGER` but `column_data[i]` actually points to a `double[]`, the `static_cast` in the C API silently reinterprets the bytes, producing garbage values. There is no runtime check possible because `void*` erases all type information.

**Why it happens:** The project already has this pattern in `convert_params()` (database_query.cpp lines 10-36) for parameterized queries. That function handles single values per parameter (`*static_cast<const int64_t*>(param_values[i])`). The time series redesign extends this to arrays of values per column, amplifying the risk: a type mismatch corrupts not one value but `row_count` values. The bindings (Julia/Dart) construct these parallel arrays, and any bug in the marshaling code silently corrupts data.

**Evidence from codebase:** The `convert_params` function at `database_query.cpp:17` does `*static_cast<const int64_t*>(param_values[i])` -- if `param_types[i]` is wrong, this is undefined behavior. The same pattern scaled to arrays of `row_count` elements is catastrophic.

**Consequences:** Silent data corruption. Written time series values are wrong. Tests may pass if they only test the happy path with correct types. Production data is silently mangled.

**Prevention:**
1. In the C API, validate column types against the schema metadata *before* casting. The C API already calls `db->db.get_time_series_metadata()` (line 128 of `database_time_series.cpp`) -- use this to verify that the caller's type tags match the actual column types.
2. Add explicit validation: if `column_types[i]` does not match `metadata.value_columns[i].data_type`, set error and return `QUIVER_ERROR` before any data is accessed.
3. In bindings, derive column types from metadata automatically rather than requiring the caller to specify them. The metadata lookup already exists at the C API level.
4. Consider eliminating the `void*` pattern entirely: instead of `column_types[] + column_data[]`, use separate typed parameters per column, similar to how update_vector_integers/floats/strings are separate functions. This trades API surface area for type safety.

**Detection:** Write tests where column types are deliberately wrong. If the test produces garbage instead of an error, the validation is missing.

**Phase to address:** C API design phase. This is a design decision, not an implementation detail. Choose the API shape before writing code.

---

### Pitfall 3: Julia GC Collecting Intermediate Arrays During ccall

**What goes wrong:** When Julia marshals kwargs to parallel C arrays, it creates intermediate Julia arrays (`Vector{Ptr{Cchar}}`, `Vector{Int64}`, `Vector{Float64}`) that hold pointers to native memory or converted values. If these intermediates are not rooted with `GC.@preserve`, the garbage collector can collect them during the `@ccall`, causing the C function to read freed memory.

**Why it happens:** The current `update_time_series_group!` in `database_update.jl` (lines 138-167) already demonstrates the correct pattern: it uses `GC.@preserve date_time_cstrings` to keep the string buffer array alive during the ccall. But the redesign adds N columns of varying types, requiring N separate `GC.@preserve` blocks or a single block preserving all intermediates. The risk is that during the rewrite, some columns' intermediates are accidentally left unpreserved.

**Evidence from codebase:** The existing `update_vector_strings!` (line 81-87) uses `GC.@preserve cstrings` to protect the string-to-Cstring conversion results. The current pattern works for a single column. With N string columns, N separate conversion arrays must all be preserved simultaneously.

**Consequences:** Intermittent segfaults in Julia tests. The GC is nondeterministic, so the bug may only manifest under memory pressure, making it extremely difficult to reproduce. Tests pass 99% of the time but fail sporadically in CI.

**Prevention:**
1. Wrap all intermediate arrays in a single `GC.@preserve` block:
   ```julia
   all_preserved = []
   for (name, values) in kwargs
       if values isa Vector{<:AbstractString}
           cstrings = [Base.cconvert(Cstring, s) for s in values]
           push!(all_preserved, cstrings)
       end
   end
   GC.@preserve all_preserved begin
       # build pointer arrays and call ccall here
   end
   ```
2. Keep string conversion and ccall in the same `GC.@preserve` scope. Never separate them across function boundaries.
3. For numeric arrays (`Vector{Int64}`, `Vector{Float64}`), Julia guarantees the array memory is stable during ccall (no GC relocation for primitive arrays), but only if the array is still referenced. Ensure a local variable holds it.

**Detection:** Run Julia tests with `GC.gc()` calls inserted between array construction and ccall. If there is a preservation bug, forced GC will trigger it deterministically.

**Phase to address:** Julia binding redesign phase. Must be reviewed in code review with explicit checklist item: "All intermediate arrays preserved during ccall."

---

### Pitfall 4: Breaking the Existing Test Suite Incrementally

**What goes wrong:** The redesign changes function signatures at the C API level (`quiver_database_update_time_series_group` gains new parameters). All 1,213 existing tests must continue passing throughout the migration. If the C API signature changes before all binding tests are updated, the entire CI pipeline breaks and stays broken for the duration of the migration, creating a "big bang" commit.

**Why it happens:** The project has 4 test suites (C++, C API, Julia, Dart) plus Lua tests. The C API header is consumed by the Dart ffigen generator and the Julia `c_api.jl` declarations. A signature change in `include/quiver/c/database.h` immediately breaks:
1. `tests/test_c_api_database_time_series.cpp` (all 15 time series C API tests)
2. `bindings/julia/src/c_api.jl` line 316-318 (ccall declaration)
3. `bindings/dart/lib/src/ffi/bindings.dart` (auto-generated)
4. All Julia/Dart test files that call the function

The C++ layer is unaffected (it already uses `std::vector<std::map<std::string, Value>>`).

**Consequences:** Multi-day period where CI is broken. Developers cannot verify other changes. Partial work must be merged, creating instability. Worse, if the migration is abandoned partway, rolling back is difficult.

**Prevention:**
1. **Add new, don't modify.** Introduce `quiver_database_update_time_series_group_v2` (or `_multi` or `_columnar`) alongside the existing function. Old tests continue passing. New tests exercise the new API. Only remove the old function after all bindings have migrated.
2. Alternatively, make the new C API signature backward-compatible: if `column_count == 0` and a single `date_times` + `values` pair is provided, use the old code path.
3. Each layer migrates independently: C API first (with backward compat), then Julia, then Dart, then Lua, each with passing tests at every step.
4. Use `scripts/test-all.bat` as the gate for every single commit during migration.

**Detection:** If more than one test suite is failing simultaneously, the migration is being done wrong. Each commit should break zero or at most one test suite (the one being actively migrated).

**Phase to address:** This is a *process* pitfall, not a code pitfall. It affects the phase ordering in the roadmap. The roadmap must enforce incremental migration with backward compatibility.

---

### Pitfall 5: Free Function Mismatch for Variable-Column Read Results

**What goes wrong:** The current `quiver_database_free_time_series_data(char** date_times, double* values, size_t row_count)` frees exactly 2 arrays: one `char**` (with per-element string deallocation) and one `double*`. The new multi-column read returns N arrays of varying types. Each array has different deallocation requirements: `int64_t*` uses `delete[]`, `double*` uses `delete[]`, `char**` requires iterating and freeing each string then freeing the array. A single generic free function must know the type of each column to free it correctly.

**Why it happens:** The existing alloc/free pattern co-locates allocation and deallocation (per CLAUDE.md principle). The free function is hardcoded to the allocation shape. With variable columns, the free function must accept type information to know how to deallocate each column. Passing type info to a free function is unusual and error-prone.

**Evidence from codebase:** Compare `quiver_database_free_time_series_data` (line 152-163 of `database_time_series.cpp`) which knows the exact layout (char** + double*), vs `free_vectors_impl` template (database_helpers.h:50-61) which is generic for numeric types but not for mixed types. There is no existing pattern for freeing N columns of heterogeneous types.

**Consequences:** Memory leaks (if string columns are freed as `delete[]` instead of per-element `delete[]` then `delete[]`), double-free (if numeric columns are freed with string deallocation logic), or heap corruption.

**Prevention:**
1. **Return a struct, not raw arrays.** Define `quiver_time_series_data_t` that bundles all column data, types, and counts. Provide a single `quiver_database_free_time_series_data_v2(quiver_time_series_data_t*)` that knows how to free each column based on stored type information.
   ```c
   typedef struct {
       char** dimension_values;   // date/time strings
       size_t row_count;
       quiver_data_type_t* column_types;
       char** column_names;
       void** column_data;  // each points to typed array
       size_t column_count;
   } quiver_time_series_data_t;
   ```
2. The free function iterates `column_count`, switches on `column_types[i]`, and frees appropriately. The type information travels with the data, eliminating caller error.
3. Keep the struct opaque to the caller. Provide accessor functions if needed.

**Detection:** Run address sanitizer on all read tests. Write a test that reads a multi-column time series with mixed types (integer + float + string columns) and verifies clean deallocation.

**Phase to address:** C API design phase. This must be decided before implementation because it determines the entire API shape.

---

## Moderate Pitfalls

Mistakes that cause bugs, rework, or test failures but are recoverable.

### Pitfall 6: Kwargs Column Name to Schema Column Name Mismatch

**What goes wrong:** Julia kwargs use Symbol keys (`:temperature`, `:humidity`). The user writes `update_time_series_group!(db, "Sensor", "temperature", id; date_time=dates, temperature=temps)`. The kwargs key `temperature` must exactly match the SQL column name in `Sensor_time_series_temperature`. If the column is named `temp_celsius` but the kwarg is `temperature`, the data silently goes to the wrong column or the C++ layer throws "column not found."

**Why it happens:** Julia kwargs are Symbols converted to Strings. There is no compile-time or IDE-assisted validation that the kwarg name matches a real column. The current `Dict{String, Any}` interface at least makes the column name visually explicit as a string key. Kwargs feel more natural but hide the mapping.

**Prevention:**
1. Validate column names against metadata in the binding before calling the C API. The binding can call `get_time_series_metadata` to get the list of valid column names and reject unknown kwargs early with a clear error.
2. Keep the error message specific: "Unknown column 'temperature' in time series group 'temp'. Valid columns: date_time, temp_celsius, humidity".
3. In Dart, named parameters are defined at compile time in the function signature, so this validation happens naturally. But for dynamic schemas, Dart must use a Map parameter anyway (named parameters cannot be dynamically defined).

**Detection:** Write a test that passes an invalid column name via kwargs and verifies the error message names the invalid column and lists valid ones.

**Phase to address:** Julia and Dart binding redesign phases.

---

### Pitfall 7: Dart Named Parameters Cannot Be Dynamic

**What goes wrong:** The project context says "Dart from List<Map> to named parameters." But Dart named parameters are compile-time constructs -- you cannot dynamically define named parameters based on schema metadata. A time series group with columns `[temperature, humidity]` would need `void updateTimeSeriesGroup({required List<double> temperature, required List<double> humidity})`, but the column names and types vary per schema. This is impossible with Dart named parameters.

**Why it happens:** Confusion between Dart named parameters (static, declared in function signature) and Python-style kwargs (dynamic, runtime-determined). Dart does not support the latter.

**Consequences:** If the Dart API is designed around named parameters, it only works for a fixed set of known column names, defeating the purpose of a schema-flexible library.

**Prevention:**
1. **Accept that Dart must use a Map.** The Dart API should be `void updateTimeSeriesGroup(String collection, String group, int id, Map<String, List<Object>> columns)` where keys are column names and values are typed lists.
2. Alternatively, use a builder pattern:
   ```dart
   db.updateTimeSeriesGroup('Sensor', 'temperature', id,
     TimeSeriesData()
       ..addStringColumn('date_time', dateTimes)
       ..addDoubleColumn('temperature', temps));
   ```
3. Document why named parameters are not used: column names come from the schema, not the code.

**Detection:** If the Dart API design review proposes named parameters, flag this immediately.

**Phase to address:** API design phase (before implementation). This is a design decision.

---

### Pitfall 8: Read Side Returning Columnar Data but Caller Expecting Row-Oriented

**What goes wrong:** If the new `read_time_series_group` C API returns columnar data (parallel arrays per column), but bindings transpose to row-oriented `Dict[]` / `List<Map>`, the transposition has O(rows * columns) cost and creates N*M Dict/Map objects. For large time series (100K rows, 5 columns), this creates 500K map entries, causing significant memory and CPU overhead.

**Why it happens:** The C++ layer returns row-oriented data (`std::vector<std::map<std::string, Value>>`). The C API must flatten this to C-compatible arrays. The most natural C representation is columnar (parallel arrays). The bindings must reconstruct row-oriented structures for the user.

**Evidence from codebase:** The current Dart `readVectorGroupById` and Julia `read_vector_group_by_id` both do column-to-row transposition (see Julia `database_read.jl` lines 536-547 and Dart `database_read.dart` lines 873-883).

**Prevention:**
1. Accept the transposition cost and optimize: allocate rows vector with capacity, pre-size dicts.
2. Consider offering both columnar and row-oriented read APIs at the binding level. Power users who process large time series column-wise can skip transposition.
3. For the C API, columnar is correct: it avoids per-row allocation overhead and is more cache-friendly for the C layer.

**Detection:** Benchmark read operations with 10K and 100K row time series. If transposition takes more than 10% of total read time, consider offering columnar read in bindings.

**Phase to address:** C API and binding implementation phases.

---

### Pitfall 9: Lua Table-Based Interface Inconsistency

**What goes wrong:** Lua currently receives and returns time series as arrays of tables (Lua tables, i.e., `{ {date_time = "...", value = 1.0}, ... }`). If the C++ `update_time_series_group` changes its signature but the Lua sol2 binding is not updated, Lua scripts silently pass wrong data shapes. Unlike Julia/Dart which go through the C API, Lua binds directly to C++ via sol2 lambdas.

**Why it happens:** Lua bindings are manually maintained in `lua_runner.cpp`. The `update_time_series_group_from_lua` function (line 1058-1069) manually iterates a sol::table and calls `lua_table_to_value_map()`. If the C++ interface changes to accept columnar data instead of row maps, the Lua binding must be completely rewritten, not just updated.

**Evidence from codebase:** `lua_runner.cpp` line 1063-1068 shows the current pattern: iterate `rows` table by integer index, convert each row to a `std::map<std::string, Value>`.

**Prevention:**
1. Keep the C++ `update_time_series_group` accepting `std::vector<std::map<std::string, Value>>`. The C++ interface is already correct for multi-column. Only the C API needs redesign.
2. The Lua binding should continue calling the C++ method directly with the same row-map interface. No change needed in Lua unless the C++ interface changes.
3. If the C++ interface does change, update Lua in the same commit. The Lua sol2 bindings are only ~50 lines per function and can be updated quickly.

**Detection:** Run Lua tests after every C++ interface change. Lua tests are fast and catch binding mismatches immediately.

**Phase to address:** C++ core phase (if C++ interface changes) or Lua binding phase.

---

### Pitfall 10: Dimension Column Handling Inconsistency

**What goes wrong:** The dimension column (`date_time`) has special semantics: it is the ordering/primary key column, not a "value" column. The current C API hardcodes this as `const char* const* date_times` (always string, always separate). The new multi-column API must decide: is the dimension column one of the N columns, or is it still a separate parameter? Inconsistency here causes confusion in every binding.

**Why it happens:** The schema allows the dimension column to have any `date_`-prefixed name (`date_time`, `date_recorded`, etc.) and the C++ layer discovers it dynamically from metadata. The current C API hardcodes "date_times" as the parameter name. The new API must handle arbitrary dimension column names.

**Evidence from codebase:** `database_time_series.cpp` lines 64-67 and 126-130 show the C API looking up the dimension column name from metadata. The multi_time_series.sql schema uses `date_time` for both groups, but there is no guarantee all schemas use the same name.

**Prevention:**
1. Keep the dimension column as a separate, explicitly-named parameter in the C API. It is always a string array. The value columns are the variable-count typed arrays.
   ```c
   quiver_database_update_time_series_group(
       db, collection, group, id,
       dimension_values,  // always const char* const*, always string
       column_names, column_types, column_data, column_count,  // value columns
       row_count);
   ```
2. In bindings, require the dimension column kwarg/key to match the schema's dimension column name. Validate early.
3. Document clearly: the dimension column is always string, always separate, always required.

**Detection:** Test with a schema where the dimension column is named something other than `date_time` (e.g., `date_recorded`). Verify that both read and update work correctly.

**Phase to address:** C API design phase.

---

## Minor Pitfalls

### Pitfall 11: FFI Generator Not Regenerated After C API Changes

**What goes wrong:** After changing `quiver_database_update_time_series_group` signature in `include/quiver/c/database.h`, the developer forgets to run `bindings/julia/generator/generator.bat` and `bindings/dart/generator/generator.bat`. The old function signatures remain in `c_api.jl` and `bindings.dart`, causing link errors or silent data corruption.

**Prevention:** Add generator re-run to the checklist for every C API change. Ideally, the build system detects header changes and triggers regeneration.

---

### Pitfall 12: Row Count Validation Across All Column Arrays

**What goes wrong:** The new C API accepts N column arrays, each with `row_count` elements. If column A has 100 elements but column B has 99 (off-by-one), the C API reads past the end of column B's array. There is no way to detect this from the C API side because C arrays do not carry their length.

**Prevention:**
1. Trust the caller (consistent with project philosophy of "clean code over defensive code").
2. But document the contract clearly: "All column data arrays must have exactly `row_count` elements."
3. In bindings, validate array lengths before calling the C API:
   ```julia
   for (name, values) in kwargs
       length(values) == row_count || throw(ArgumentError("Column '$name' has $(length(values)) elements, expected $row_count"))
   end
   ```

---

### Pitfall 13: Empty Time Series Edge Cases with Multi-Column

**What goes wrong:** The current API handles empty time series by passing `nullptr` for both arrays and `0` for count. With N columns, the convention must be consistent: are all column arrays `nullptr`, or is the entire column descriptor `nullptr`?

**Prevention:** Define and test the empty case explicitly. Convention: if `row_count == 0`, all column data pointers may be `nullptr`. The free function must handle this gracefully (check for nullptr before iterating).

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| C++ core interface | Changing `update_time_series_group` signature breaks Lua sol2 bindings | Keep C++ accepting `vector<map<string, Value>>`, only change C API |
| C API design | void* column data with type tags enables silent corruption | Validate types against schema metadata before casting |
| C API design | No struct-based return leads to complex free functions | Use `quiver_time_series_data_t` result struct |
| C API implementation | Partial allocation leak on exception path | Allocate all arrays before writing to out-params |
| Julia bindings | GC collecting string intermediates during ccall | Single `GC.@preserve` block for all intermediate arrays |
| Julia bindings | kwargs column names not validated against schema | Call `get_time_series_metadata` and validate before ccall |
| Dart bindings | Assuming named parameters can be schema-dynamic | Use Map or builder pattern, not named parameters |
| Dart bindings | Arena freed too early when constructing multi-column arrays | Keep single Arena for entire operation, release in finally |
| Lua bindings | Lua table structure changes silently | Lua binds to C++, not C API; keep C++ interface stable |
| All bindings | FFI generators not re-run after header change | Regenerate immediately, diff, test-all.bat |
| Test migration | Changing C API signature breaks all 4 test suites at once | Add new function alongside old, migrate incrementally |

## Decision Matrix: API Shape

The single most consequential design decision is the C API shape. Every other pitfall cascades from this choice.

| Approach | Type Safety | Memory Safety | Binding Complexity | Recommendation |
|----------|-------------|---------------|-------------------|----------------|
| **A: Separate typed functions** (`_integers`, `_floats`, `_strings` per column) | HIGH | HIGH | HIGH (N ccalls per update) | NO -- too many FFI crossings for multi-column |
| **B: Parallel arrays with type tags** (`column_types[] + void* column_data[]`) | LOW | LOW | MEDIUM | NO -- void* is a footgun, project already has this with query params and it works but does not scale |
| **C: Struct-based** (`quiver_time_series_columns_t` with typed column descriptors) | MEDIUM | HIGH | LOW (one ccall, one free) | YES -- type info travels with data, single alloc/free point |
| **D: Per-column calls inside a transaction** (begin_update, add_column, commit_update) | HIGH | HIGH | MEDIUM | MAYBE -- more C API surface but each call is type-safe |

**Recommendation:** Approach C (struct-based). It provides the best balance of safety and simplicity. The struct bundles column names, types, and data pointers together, and the free function can iterate the struct to deallocate correctly. This is consistent with the existing `quiver_group_metadata_t` pattern which already bundles variable-length typed metadata.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Partial allocation leak | LOW | Add address sanitizer to CI; fix leak; re-test |
| Type tag mismatch corruption | HIGH | Data must be re-written; add validation; re-test with bad types |
| Julia GC segfault | MEDIUM | Add `GC.@preserve`; difficult to reproduce -- add forced GC in tests |
| Broken test suite | LOW | Add backward-compat old function; migrate incrementally |
| Free function mismatch | HIGH | Address sanitizer + valgrind; audit all alloc/free paths |
| Kwargs column mismatch | LOW | Add metadata validation in binding; clear error message |
| Dart named params impossible | LOW | Switch to Map; design review catches early |
| Row-column transposition perf | LOW | Benchmark; add columnar API if needed |
| Lua binding stale | LOW | Run Lua tests; update sol2 binding |
| Dimension column inconsistency | MEDIUM | Standardize as separate param; test with non-standard dim names |

## Sources

- Direct analysis of `src/c/database_time_series.cpp` (279 lines) -- current alloc/free patterns
- Direct analysis of `src/c/database_query.cpp` (197 lines) -- existing void*/type-tag pattern for parameterized queries
- Direct analysis of `src/c/database_helpers.h` (145 lines) -- existing marshaling templates
- Direct analysis of `bindings/julia/src/database_update.jl` (201 lines) -- current Julia time series update with `GC.@preserve`
- Direct analysis of `bindings/dart/lib/src/database_update.dart` (374 lines) -- current Dart Arena-based FFI
- Direct analysis of `src/lua_runner.cpp` lines 1058-1069 -- current Lua sol2 time series binding
- Direct analysis of `tests/test_c_api_database_time_series.cpp` (548 lines) -- 15 existing C API time series tests
- Direct analysis of `include/quiver/c/database.h` (462 lines) -- existing C API surface
- Direct analysis of `tests/schemas/valid/multi_time_series.sql` -- multi-group schema (single value column per group)
- Existing project PITFALLS.md (v1.0 milestone) -- Pitfall 2 (C API memory ownership) and Pitfall 3 (binding desynchronization) are directly relevant and amplified by this redesign

---
*Pitfalls research for: Time series update interface redesign (multi-column C API + kwargs bindings)*
*Researched: 2026-02-12*
