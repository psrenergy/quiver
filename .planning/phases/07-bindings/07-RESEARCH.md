# Phase 7: Bindings - Research

**Researched:** 2026-02-22
**Domain:** FFI binding layer -- Julia, Dart, Lua wrappers for CSV export
**Confidence:** HIGH

## Summary

Phase 7 is a pure binding-layer phase. The C++ `export_csv` and C API `quiver_database_export_csv` are complete and tested (19 C++ tests, 19 C API tests). All three bindings already have stale 3-parameter CSV stubs that call the old C API signature. The new C API signature takes 5 parameters (collection, group, path, opts). The FFI generators have already been run, so the Julia `c_api.jl` and Dart `bindings.dart` both have the correct 5-param function declarations and the `quiver_csv_export_options_t` struct. The core work is: (1) replace the stale binding wrappers with new implementations that marshal native options maps to the flat C struct, and (2) add the Lua `export_csv` binding in `lua_runner.cpp` which bypasses the C API entirely (calls C++ directly via sol2).

**Primary recommendation:** Implement three parallel binding updates -- Julia `database_csv.jl`, Dart `database_csv.dart`, Lua `lua_runner.cpp` -- each converting language-native map types to the appropriate options format, then test each binding with the shared `csv_export.sql` schema.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Julia: keyword arguments -- `export_csv(db, collection, group, path; enum_labels=Dict(...), date_time_format="...")`
- Dart: named parameters -- `exportCSV(collection, group, path, enumLabels: {...}, dateTimeFormat: '...')`
- Lua: optional table argument -- `db:export_csv(collection, group, path, {enum_labels = {...}, date_time_format = '...'})`
- All options fully implicit when using defaults -- `export_csv(db, coll, "", path)` works with no options argument
- Empty string `""` for scalar export -- matches C/C++ API exactly, group is always required
- No null/nothing/nil alternatives -- consistent with upstream API
- No binding-layer validation -- pass through to C API, let C++ surface errors
- Native nested map literals in each language for enum_labels
- Integer keys only for enum values
- Unknown attribute names in enum_labels silently ignored
- No shorthand for scalar export -- always require all 4 positional args
- One function per binding: `export_csv` (Julia/Lua), `exportCSV` (Dart)
- Julia uses `build_csv_options(kwargs...)` pattern (mirrors existing `build_options` in `database.jl`)

### Claude's Discretion
- Dart and Lua internal marshaling approach -- whether to use a separate builder helper or marshal directly inside export function
- Internal struct conversion implementation details
- Test file organization within each binding's test suite

### Deferred Ideas (OUT OF SCOPE)
None
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BIND-01 | Julia `export_csv(db, collection, group, path, options)` with default options | Julia FFI struct `quiver_csv_export_options_t` already generated in `c_api.jl` (line 47-54); `build_csv_options` pattern mirrors existing `build_options` (line 11-20 of `database.jl`); stale 3-param stub in `database_csv.jl` needs replacement |
| BIND-02 | Dart `exportCSV(collection, group, path, options)` with default options | Dart FFI struct `quiver_csv_export_options_t` already generated in `bindings.dart` (line 3105-3118); 5-param `quiver_database_export_csv` binding exists; stale 2-param wrapper in `database_csv.dart` needs replacement; `quiver_csv_export_options_default` missing from Dart FFI -- must either regenerate or construct manually |
| BIND-03 | Lua `export_csv(collection, group, path, options)` with default options | Lua bypasses C API -- calls `Database::export_csv()` C++ method directly via sol2; needs new lambda in `bind_database()` in `lua_runner.cpp`; sol2 `sol::optional<sol::table>` pattern already used for `query_string` params |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Julia `@ccall` | Julia 1.5+ | Foreign function interface | Built-in, zero dependencies |
| Dart `dart:ffi` + `package:ffi` | Dart 3.x | Foreign function interface | Official Dart FFI |
| sol2 | (bundled) | C++/Lua binding library | Already used throughout `lua_runner.cpp` |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Julia `CEnum` | (project dep) | C enum interop | Already used in `c_api.jl` |
| Dart `ffigen` | (project dep) | Auto-generate FFI bindings from C headers | When C API changes |

### Alternatives Considered
None -- all technology choices are established by the existing codebase.

## Architecture Patterns

### Pattern 1: Julia `build_csv_options(kwargs...)` -- Keyword-to-C-Struct Marshaling

**What:** A helper function that takes Julia keyword arguments and constructs a `Ref{quiver_csv_export_options_t}` by allocating temporary C arrays for the nested enum_labels map.

**When to use:** When calling `export_csv` with options.

**Existing model** (from `bindings/julia/src/database.jl` lines 11-20):
```julia
function build_options(kwargs...)
    options = Ref(C.quiver_database_options_default())
    if haskey(kwargs, :read_only)
        options[].read_only = kwargs[:read_only] ? 1 : 0
    end
    if haskey(kwargs, :console_level)
        options[].console_level = kwargs[:console_level]
    end
    return options
end
```

**CSV version must:**
1. Start with a default struct (zeroed fields)
2. If `date_time_format` kwarg present, convert String to C pointer
3. If `enum_labels` kwarg present (a `Dict{String, Dict{Int, String}}`), flatten into parallel arrays:
   - `enum_attribute_names`: `Vector{Ptr{Cchar}}` (one per attribute)
   - `enum_entry_counts`: `Vector{Csize_t}` (entries per attribute)
   - `enum_values`: `Vector{Int64}` (all values concatenated)
   - `enum_labels`: `Vector{Ptr{Cchar}}` (all labels concatenated)
4. Pin all GC-managed memory with `GC.@preserve` to prevent collection during the C call

**Key insight:** Unlike `build_options` (simple scalar fields), `build_csv_options` requires managing temporary C string allocations that must outlive the C API call. The function should return both the struct ref and any temporary allocations, or the caller must use `GC.@preserve` on the intermediate arrays.

### Pattern 2: Dart Arena-Based Marshaling

**What:** Dart uses `Arena` allocator for temporary FFI memory. All allocations within an arena are freed in a single `releaseAll()` call.

**Existing model** (from `bindings/dart/lib/src/database.dart`):
```dart
final arena = Arena();
try {
  // allocate in arena, call C API
} finally {
  arena.releaseAll();
}
```

**CSV version must:**
1. Allocate `quiver_csv_export_options_t` struct in arena
2. If `enumLabels` provided, flatten `Map<String, Map<int, String>>` into parallel arrays allocated in arena
3. Set struct fields to point to arena-allocated arrays
4. Call C API
5. Arena cleanup frees everything

### Pattern 3: Lua sol2 Direct C++ Call

**What:** Lua binding bypasses C API entirely, calling `Database::export_csv()` directly via sol2 lambda.

**Existing model** (from `lua_runner.cpp`, e.g., `query_string`):
```cpp
"query_string",
[](Database& self, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
    return query_string_to_lua(self, sql, params, s);
},
```

**CSV version must:**
1. Accept `sol::optional<sol::table>` for the options argument
2. If table present, extract `enum_labels` (nested table) and `date_time_format` (string)
3. Convert Lua nested table `{attr = {[1] = "Low", [2] = "High"}}` to C++ `CSVExportOptions` struct directly
4. Call `self.export_csv(collection, group, path, opts)`

**Key design decision (from STATE.md):** "Lua bypasses C API for options (sol2 reads Lua table directly to C++ struct)"

### Anti-Patterns to Avoid
- **Creating new C API functions for options construction:** The C API already has `quiver_csv_export_options_default()` and the flat struct. Bindings marshal directly; no intermediate C API helpers needed.
- **Binding-layer validation:** Per locked decision, do not validate group names, attribute names, or enum keys in the binding layer. Let C++ surface errors.
- **GC-unsafe string passing in Julia:** Julia strings passed to `@ccall` as `Ptr{Cchar}` are only guaranteed valid during the call. For structs populated before the call, manual string pinning is required.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Julia C string allocation | Manual `malloc`/`pointer` | `Base.unsafe_convert` + `GC.@preserve` or `pointer(vector)` on `Vector{UInt8}` | GC-safe, automatic lifetime |
| Dart native memory | Manual `calloc`/`free` | `Arena` allocator | Single cleanup, no leaks |
| Lua table-to-C++ conversion | Intermediate C API calls | sol2 `table.get<T>()` + `table.for_each()` | Direct C++ access, no marshaling overhead |

**Key insight:** Each binding language has an established memory management pattern already used throughout this codebase. Follow the existing patterns exactly.

## Common Pitfalls

### Pitfall 1: Julia GC Collecting Strings During C Call
**What goes wrong:** Julia string temporaries passed as `Ptr{Cchar}` into a struct get garbage collected before the C function reads them.
**Why it happens:** The `@ccall` macro automatically pins direct arguments, but struct fields pointing to Julia-managed memory are not pinned.
**How to avoid:** Use `GC.@preserve` on all intermediate string/array objects that back pointer fields in the struct. Keep the `GC.@preserve` block around the entire C API call.
**Warning signs:** Intermittent crashes, corrupted CSV output, memory access violations during tests.

### Pitfall 2: Dart FFI Missing `quiver_csv_export_options_default`
**What goes wrong:** The Dart FFI bindings (`bindings.dart`) have the `quiver_csv_export_options_t` struct and `quiver_database_export_csv` function, but do NOT have `quiver_csv_export_options_default()` -- it was declared in `csv.h` which is not listed in ffigen's `include-directives`.
**Why it happens:** The `ffigen.yaml` `include-directives` lists `database.h` but not `csv.h`. The struct is found because it is referenced by `quiver_database_export_csv`'s parameter type, but standalone functions from `csv.h` are excluded.
**How to avoid:** Either (a) add `csv.h` to ffigen's `include-directives` and regenerate, or (b) construct default options manually in Dart (zero out all pointer fields, set `date_time_format` to empty string, set `enum_attribute_count` to 0). Option (b) is simpler and avoids generator config changes.
**Warning signs:** Compilation error when trying to call `bindings.quiver_csv_export_options_default()`.

### Pitfall 3: Stale Binding Stubs Still Have Old Signatures
**What goes wrong:** The current Julia `database_csv.jl` has `export_csv(db, table, path)` (3 params) and Dart `database_csv.dart` has `exportCSV(table, path)` (2 params on Database). These call old C API signatures that no longer exist.
**Why it happens:** Phase 6 updated the C API from 4 params (no options) to 5 params (with options), and ran FFI generators to update the low-level bindings. But the high-level wrapper functions were not updated -- that is Phase 7's job.
**How to avoid:** Completely replace the stale function bodies. Do not try to extend them. The new functions have different parameter lists (collection+group+path vs table+path).
**Warning signs:** Immediate compilation/link errors if testing against current C API.

### Pitfall 4: Lua Integer vs Float Ambiguity for Enum Keys
**What goes wrong:** Lua numbers are all doubles by default. When iterating a table like `{[1] = "Low", [2] = "High"}`, the keys may come through as `double` not `int64_t`.
**Why it happens:** Lua 5.4 does have integers, but sol2 type deduction can vary depending on how values are written in Lua tables.
**How to avoid:** When extracting enum integer keys from the Lua table, explicitly cast using `key.as<int64_t>()`. sol2 handles the conversion correctly for integer-valued doubles.
**Warning signs:** Wrong enum resolution, or missing mappings.

### Pitfall 5: Dart Struct Field Setting Order
**What goes wrong:** Dart FFI `Struct` fields accessed via `ref.field = value` syntax. Setting pointer fields on a stack-allocated struct requires the struct to be allocated in the Arena first.
**Why it happens:** `arena<quiver_csv_export_options_t>()` allocates native memory and returns a `Pointer<quiver_csv_export_options_t>`. Fields are set via `.ref.field = value`.
**How to avoid:** Follow the existing pattern: `final optsPtr = arena<quiver_csv_export_options_t>(); optsPtr.ref.date_time_format = ...;`
**Warning signs:** Segfaults or Dart assertion errors during struct population.

## Code Examples

### Julia: `build_csv_options` and `export_csv`
```julia
function build_csv_options(kwargs...)
    # Start with zeroed struct
    opts = Ref(C.quiver_csv_export_options_t(
        C_NULL,   # date_time_format
        C_NULL,   # enum_attribute_names
        C_NULL,   # enum_entry_counts
        C_NULL,   # enum_values
        C_NULL,   # enum_labels
        0         # enum_attribute_count
    ))

    # Collect temporaries for GC.@preserve
    temps = Any[]

    if haskey(kwargs, :date_time_format)
        fmt = kwargs[:date_time_format]
        push!(temps, fmt)
        opts[].date_time_format = Base.unsafe_convert(Ptr{Cchar}, fmt)
    else
        empty_str = ""
        push!(temps, empty_str)
        opts[].date_time_format = Base.unsafe_convert(Ptr{Cchar}, empty_str)
    end

    if haskey(kwargs, :enum_labels)
        # Flatten Dict{String, Dict{Int, String}} to parallel arrays
        enum_dict = kwargs[:enum_labels]
        attr_names = collect(keys(enum_dict))
        # ... flatten into c_attr_names, c_entry_counts, c_values, c_labels ...
        # Push all arrays to temps for GC safety
    end

    return opts, temps
end

function export_csv(db::Database, collection::String, group::String, path::String; kwargs...)
    opts, temps = build_csv_options(kwargs)
    GC.@preserve temps begin
        check(C.quiver_database_export_csv(db.ptr, collection, group, path, opts))
    end
    return nothing
end
```

### Dart: `exportCSV` with Arena marshaling
```dart
void exportCSV(
  String collection,
  String group,
  String path, {
  Map<String, Map<int, String>>? enumLabels,
  String? dateTimeFormat,
}) {
  _ensureNotClosed();
  final arena = Arena();
  try {
    final optsPtr = arena<quiver_csv_export_options_t>();

    // date_time_format
    optsPtr.ref.date_time_format = (dateTimeFormat ?? '').toNativeUtf8(allocator: arena).cast();

    // enum_labels marshaling
    if (enumLabels != null && enumLabels.isNotEmpty) {
      // Flatten Map<String, Map<int, String>> to parallel arrays
      final attrNames = enumLabels.keys.toList();
      // ... allocate arena arrays for attribute_names, entry_counts, values, labels ...
      optsPtr.ref.enum_attribute_count = attrNames.length;
    } else {
      optsPtr.ref.enum_attribute_names = nullptr;
      optsPtr.ref.enum_entry_counts = nullptr;
      optsPtr.ref.enum_values = nullptr;
      optsPtr.ref.enum_labels = nullptr;
      optsPtr.ref.enum_attribute_count = 0;
    }

    check(bindings.quiver_database_export_csv(
      _ptr,
      collection.toNativeUtf8(allocator: arena).cast(),
      group.toNativeUtf8(allocator: arena).cast(),
      path.toNativeUtf8(allocator: arena).cast(),
      optsPtr,
    ));
  } finally {
    arena.releaseAll();
  }
}
```

### Lua: `export_csv` binding in sol2
```cpp
"export_csv",
[](Database& self,
   const std::string& collection,
   const std::string& group,
   const std::string& path,
   sol::optional<sol::table> opts_table) {
    quiver::CSVExportOptions opts;
    if (opts_table) {
        auto& t = *opts_table;
        if (auto fmt = t.get<sol::optional<std::string>>("date_time_format")) {
            opts.date_time_format = *fmt;
        }
        if (auto enums = t.get<sol::optional<sol::table>>("enum_labels")) {
            enums->for_each([&](sol::object attr_key, sol::object attr_value) {
                auto attr_name = attr_key.as<std::string>();
                auto& attr_map = opts.enum_labels[attr_name];
                attr_value.as<sol::table>().for_each([&](sol::object k, sol::object v) {
                    attr_map[k.as<int64_t>()] = v.as<std::string>();
                });
            });
        }
    }
    self.export_csv(collection, group, path, opts);
},
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `export_csv(db, table, path)` (3-param Julia) | `export_csv(db, collection, group, path; opts...)` (4-pos + kwargs) | Phase 6->7 | Julia stub must be fully rewritten |
| `exportCSV(table, path)` (2-param Dart) | `exportCSV(collection, group, path, {opts})` (3-pos + named) | Phase 6->7 | Dart stub must be fully rewritten |
| No Lua `export_csv` | `db:export_csv(collection, group, path, opts_table)` | Phase 7 | New binding added to `lua_runner.cpp` |

**Deprecated/outdated:**
- `import_csv` stubs exist in both Julia and Dart but are out of scope (import_csv is a future requirement)

## Open Questions

1. **Dart FFI `quiver_csv_export_options_default` missing**
   - What we know: The function is declared in `csv.h` which is included by `database.h`, but ffigen's `include-directives` filter excludes standalone functions from `csv.h`
   - What's unclear: Whether regenerating with `csv.h` added to `include-directives` would be cleaner than manual struct construction
   - Recommendation: Add `csv.h` to ffigen `include-directives` and regenerate. This is the consistent approach. However, since the Dart wrapper will construct the struct manually in the Arena anyway (to populate it with user-provided values), the default factory function is not strictly needed. Either approach works; adding to generator config is the principled choice for consistency with Julia where the default function IS available.

## Sources

### Primary (HIGH confidence)
- Codebase inspection: `include/quiver/c/csv.h` -- C API struct layout and field documentation
- Codebase inspection: `include/quiver/csv.h` -- C++ CSVExportOptions struct
- Codebase inspection: `src/c/database_csv.cpp` -- `convert_options()` function showing flat-to-nested conversion
- Codebase inspection: `bindings/julia/src/c_api.jl` -- Julia FFI declarations including `quiver_csv_export_options_t` struct (lines 47-54) and `quiver_database_export_csv` 5-param call (line 397-399)
- Codebase inspection: `bindings/dart/lib/src/ffi/bindings.dart` -- Dart FFI struct (lines 3105-3118) and 5-param function (lines 2350-2387)
- Codebase inspection: `bindings/julia/src/database.jl` -- `build_options` pattern (lines 11-20)
- Codebase inspection: `src/lua_runner.cpp` -- sol2 binding patterns, `sol::optional<sol::table>` usage
- Codebase inspection: `bindings/julia/src/database_csv.jl` -- stale 3-param stub
- Codebase inspection: `bindings/dart/lib/src/database_csv.dart` -- stale 2-param stub
- Codebase inspection: `tests/test_database_csv.cpp` -- 19 C++ tests defining expected behavior
- Codebase inspection: `tests/schemas/valid/csv_export.sql` -- test schema

### Secondary (MEDIUM confidence)
- Project decisions in `.planning/STATE.md` -- "Lua bypasses C API for options"
- CLAUDE.md architecture documentation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all libraries and patterns already established in codebase
- Architecture: HIGH -- patterns directly observable in existing binding code for other features (transactions, queries)
- Pitfalls: HIGH -- identified through direct inspection of current code state and FFI generator configuration

**Research date:** 2026-02-22
**Valid until:** 2026-03-22 (stable -- no external dependency changes expected)
