# 17 — read_time_series_row C API: presence mask so every binding returns null for no data

**Batch** 3 · **Severity** high · **Breaking** yes. It breaks three groups of callers. (1) Every C caller of `quiver_database_read_time_series_row`: a new `uint8_t** out_mask` out-parameter must be passed and freed. (2) Every Julia / Dart / Python / JS caller that read the old `0` / `NaN` for "no data": it now gets `nothing` / `null` / `None` / `null`. (3) Julia callers that relied on the element type: `Vector{Int64}` / `Vector{Float64}` become `Vector{Union{Nothing, T}}`. · **Size** M · **Layers** C API, Julia FFI + wrapper, Dart FFI + wrapper, Python cdef + wrapper, JS loader + wrapper, Lua (test only), C API/binding tests, docs, CHANGELOG
**Depends on** none. Plan 04 lands earlier in numeric order and adds one C API test that calls this function. Item 7 of "Tests → C API" extends that test if it exists.
**Overlaps with**
- **04**: adds `TEST(DatabaseCApi, ReadTimeSeriesRowRejectsMultiDimensionGroup)` to the same C API test file. That test must gain the `out_mask` argument here. 04 also appends sentences to the same root `AGENTS.md` "Time series row" bullet and the same Dart doc comment and Python docstring. Keep 04's sentences.
- **18**: later edits `bindings/js/src/time-series.ts` and `loader.ts`, `bindings/julia/src/database_read.jl` and `bindings/python/src/quiverdb/database.py`, in other functions.
- **25**: later replaces the `date_time.strftime(...)` line in Python `read_time_series_row`. Leave that line as it is.
- **36 / 40**: later add `finally` frees to the neighbouring `read_time_series_group` (Julia) and the Dart group decoders. They do not touch `readTimeSeriesRow`.
- **69**: fixes handle leaks elsewhere in `tests/test_c_api_database_time_series_row.cpp` (the upsert section).
- **23**: edits `src/c/database_helpers.h`'s group marshaller and `src/c/AGENTS.md`. Its maintainer note says "do not touch read_time_series_row". Neither plan edits the other's code.
- **57 / 53**: later rewrite parts of the C++ core `Database::read_time_series_row`. This plan does not touch the core.

## Why

The C++ core already returns a null for an element with no data at or before the date. `src/database_time_series.cpp`, `Database::read_time_series_row` (currently ~L314-317):

```cpp
    for (auto element_id : element_ids) {
        auto it = id_value_map.find(element_id);
        result.push_back(it != id_value_map.end() ? it->second : Value{nullptr});
    }
```

Lua passes that null through as `nil` (`src/lua_runner.cpp`, `read_time_series_row_lua`, `value_to_lua_object`). The C API drops it. `src/c/database_time_series.cpp`, `quiver_database_read_time_series_row` (currently ~L90-107):

```cpp
        case QUIVER_DATA_TYPE_INTEGER: {
            auto* arr = new int64_t[values.size()];
            for (size_t i = 0; i < values.size(); ++i) {
                arr[i] = std::holds_alternative<int64_t>(values[i]) ? std::get<int64_t>(values[i]) : 0;
            }
            ...
        case QUIVER_DATA_TYPE_FLOAT: {
            ...
                arr[i] = std::holds_alternative<double>(values[i]) ? std::get<double>(values[i])
                                                                   : std::numeric_limits<double>::quiet_NaN();
```

`include/quiver/c/database.h` (currently ~L477) documents that sentinel: `// For elements with no matching data: INTEGER -> 0, FLOAT -> NaN, STRING/DATE_TIME -> NULL pointer`. All four FFI bindings copy the numeric arrays straight through:

- Julia `copy(unsafe_wrap(...))` (`bindings/julia/src/database_read.jl`, `read_time_series_row`)
- Dart `List<Object?>.generate(count, (i) => ptr[i])` (`bindings/dart/lib/src/database_read.dart`, `readTimeSeriesRow`)
- Python `[int_ptr[i] for i in range(count)]` (`bindings/python/src/quiverdb/database.py`, `read_time_series_row`)
- JS `decodeInt64Array(...)` (`bindings/js/src/time-series.ts`, `readTimeSeriesRow`)

**Reproduction** (`tests/schemas/valid/mixed_time_series.sql`). "Sensor 1" has one row on `2024-01-01` with `humidity = 0` and `temperature = 20.5`. "Sensor 2" has no rows. Read at `2024-01-01`:

| Layer | `humidity` | `temperature` |
|---|---|---|
| C++ core / Lua | `[0, null]` / `{0, nil}` | `[20.5, null]` / `{20.5, nil}` |
| Julia / Dart / Python / JS today | `[0, 0]` | `[20.5, NaN]` |

Sensor 2's missing value cannot be told apart from Sensor 1's stored `0`, so data is lost without any error. `DatabaseCApi.ReadTimeSeriesRowBeforeAllData` passes today (I ran `quiver_c_tests.exe --gtest_filter=DatabaseCApi.ReadTimeSeriesRow*`: 7 passed), and it pins the `NaN` with `EXPECT_TRUE(std::isnan(floats[0]))`.

The code contradicts the contract in five places:

- Root `AGENTS.md` ("Time series row" bullet): "null Value for elements with no matching data (bindings surface `nothing`/`null`/`None`/`nil`)".
- `docs/time_series.md` ("Rules"): "Querying at `2020` returns `[1.0, nothing]`". Julia actually returns `[1.0, NaN]`.
- The Dart doc comment: "elements with no matching data yield `null`".
- The Python docstring: "elements with no matching data yield None".
- JS's declared return type `(number | string | null)[]`.

Principles violated:
- **Homogeneity.** Lua and C++ say null; the FFI bindings say 0/NaN.
- **Logic in C++, bindings thin.** No binding can recover the null after the C boundary.
- The same class of bug that the "Scalar bulk reads preserve NULLs positionally" design decision removed from the bulk scalar readers.

## Constraints and decisions

- **Maintainer decision (binding):** "BREAKING. Allocate the mask for every data type (uniform decode). Julia returns Vector{Optional{T}} (T from data_type, also on the empty path) and remove that section from bindings/julia/type_stability_followup.md. Hand-edit Dart bindings.dart (do not run ffigen, see bindings/dart/AGENTS.md), Python _c_api.py, JS loader.ts; regenerate Julia c_api.jl. Move Python frees into try/finally."
- **House pattern to copy:** `quiver_database_read_scalar_integers` / `_floats` (`include/quiver/c/database.h` ~L91-107, `src/c/database_helpers.h` `read_scalars_masked_impl`). A parallel `uint8_t** out_mask`, where `mask[i] == 0` means null and the data slot then holds a placeholder. It is freed by the existing `quiver_database_free_mask`. `*out_mask` is NULL when the count is 0. The out-param goes **between `out_values` and `out_count`**, as in the scalar readers.
- `src/c/AGENTS.md` currently says "(The row API keeps its sentinel encoding; only the columnar group API uses the presence mask.)". That note was a scoping remark in e0f8465 (#208, verified with `git log -S`). It is not a root Design Decision or a Do-Not-Fix item, and it contradicts the root contract, so this plan removes it.
- **Dart** (`bindings/dart/AGENTS.md`, "The checked-in `bindings.dart` predates the pinned ffigen"): regenerating rewrites the whole file and breaks downstream enum comparisons. C API changes are therefore hand-added "in the file's existing style". **Never run `scripts/generator.bat`**, because it runs the Dart generator too. Run only the Julia generator.
- **Julia** (`bindings/julia/AGENTS.md`): `src/c_api.jl` is generated. Regenerate it with `bindings/julia/generator/generator.bat`.
- **JS** (`bindings/js/AGENTS.md`): the symbol table in `loader.ts` is hand-written. Out-params are passed as the TypedArray (`alloc.buf`), never as a precomputed `ptr()`. A mask is read with `new Uint8Array(toArrayBuffer(...))`, never with a `DataView` over native memory.
- **Julia type rule** (`bindings/julia/type_stability_followup.md`, "Guiding principle"): "Where a `nothing` can also mean 'no such row / no data / unknown', the optional is inherent". So `read_time_series_row` always returns `Vector{Optional{T}}`, never a concrete `Vector{T}` keyed on `not_null`.
- **Versioning:** 0.11.0 is unreleased and is already the minor bump over 0.10.9. The **BREAKING** entry goes under `## [0.11.0] — unreleased` and **no manifest version changes**. This corrects both verifiers, who asked for a minor bump.
- **Lua is already correct** (`nil` via `value_to_lua_object`). No Lua code changes. One Lua test is added so every layer pins the same contract.
- **Error messages:** this plan adds no new message. The new `QUIVER_REQUIRE` argument produces the standard `Null argument: out_mask`.

Corrections to the finding and verifiers:
- The finding suggested tightening Dart/Python/JS return types to `List<int?>` / `list[int | None]` / `(number | null)[]`. That is wrong: the same reader returns strings for TEXT/DATE_TIME columns. The return types stay as they are: `List<Object?>`, `list`, `(number | string | null)[]`.
- The finding said "regenerate Dart bindings". The Dart file is hand-edited instead (see above).
- The verifiers missed that `tests/test_c_api_database_time_series_row.cpp` still needs `<cmath>`: `NAN` is used at ~L937 in an upsert test. Keep that include. `src/c/database_time_series.cpp`'s `<limits>` does become unused (its only use was `quiet_NaN`), so remove it.

Alternatives considered and rejected:
- **Python-only NaN→None.** It leaves INTEGER lossy, leaves Julia/Dart/JS disagreeing, and puts null inference in a binding.
- **Doc-only fix (document the sentinels).** It would make a lossy INTEGER sentinel the official contract, against the root contract and `docs/time_series.md`.
- **No mask for STRING/DATE_TIME** (the `read_scalar_strings` precedent). The maintainer chose one uniform decode for every type.
- **Deleting the unreachable `default:` branch** of the C switch. `to_c_data_type` throws first, so the branch is dead, but deleting it is out of scope. It stays and frees the mask before throwing.
- **Widening int64→double for a FLOAT column** in the C switch (like `Row::get_float`). A REAL column never yields an int64 Value from SQLite (REAL affinity converts on read), so it is not needed. The existing exact-alternative test is kept.

## Changes

### 1. `include/quiver/c/database.h` — comment and declaration of `quiver_database_read_time_series_row` (currently ~L472-489)

Current:

```c
// Read time series row - returns one value per element for a specific attribute at a given date_time
// Uses "last non-null value at or before date_time" lookup semantics
// out_data_type: attribute's data type (QUIVER_DATA_TYPE_*)
// out_values: typed array (int64_t* for INTEGER, double* for FLOAT, char** for STRING/DATE_TIME)
// out_count: number of elements in the collection
// For elements with no matching data: INTEGER -> 0, FLOAT -> NaN, STRING/DATE_TIME -> NULL pointer
// Free out_values with the typed free function matching *out_data_type:
//   INTEGER -> quiver_database_free_integer_array
//   FLOAT -> quiver_database_free_float_array
//   STRING/DATE_TIME -> quiver_database_free_string_array
QUIVER_C_API quiver_error_t quiver_database_read_time_series_row(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* group,
                                                                 const char* attribute,
                                                                 const char* date_time,
                                                                 int* out_data_type,
                                                                 void** out_values,
                                                                 size_t* out_count);
```

New:

```c
// Read time series row - returns one value per element for a specific attribute at a given date_time
// Uses "last non-null value at or before date_time" lookup semantics
// out_data_type: attribute's data type (QUIVER_DATA_TYPE_*)
// out_values: typed array (int64_t* for INTEGER, double* for FLOAT, char** for STRING/DATE_TIME)
// out_mask: presence mask, returned for every data type. out_mask[i] == 0 means the element has no
// data at or before date_time; out_values[i] is then a placeholder (0 / 0.0 / NULL char*) to ignore
// out_count: number of elements in the collection; 0 leaves out_values and out_mask NULL
// Free out_values with the typed free function matching *out_data_type:
//   INTEGER -> quiver_database_free_integer_array
//   FLOAT -> quiver_database_free_float_array
//   STRING/DATE_TIME -> quiver_database_free_string_array
// and out_mask with quiver_database_free_mask
QUIVER_C_API quiver_error_t quiver_database_read_time_series_row(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* group,
                                                                 const char* attribute,
                                                                 const char* date_time,
                                                                 int* out_data_type,
                                                                 void** out_values,
                                                                 uint8_t** out_mask,
                                                                 size_t* out_count);
```

### 2. `include/quiver/c/database.h` — the `quiver_database_free_mask` comment (currently ~L535)

Current: `// Memory cleanup for the presence mask returned by the integer/float scalar readers`
New:

```c
// Memory cleanup for the presence mask returned by the integer/float scalar readers and by
// quiver_database_read_time_series_row
```

### 3. `src/c/database_time_series.cpp` — `quiver_database_read_time_series_row` (currently ~L58-129) and includes

(a) Delete `#include <limits>` (currently L6). Its only use was `std::numeric_limits<double>::quiet_NaN()` in this function. Keep the other includes.

(b) Replace the whole function with:

```cpp
QUIVER_C_API quiver_error_t quiver_database_read_time_series_row(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* group,
                                                                 const char* attribute,
                                                                 const char* date_time,
                                                                 int* out_data_type,
                                                                 void** out_values,
                                                                 uint8_t** out_mask,
                                                                 size_t* out_count) {
    QUIVER_REQUIRE(db, collection, group, attribute, date_time, out_data_type, out_values, out_mask, out_count);

    try {
        // The C++ layer validates collection/group/attribute and throws the
        // canonical errors; afterwards the metadata lookup below cannot miss.
        auto values = db->db.read_time_series_row(collection, group, attribute, date_time);

        auto metadata = db->db.get_time_series_metadata(collection, group);
        quiver::DataType attr_type{};
        for (const auto& vc : metadata.value_columns) {
            if (vc.name == attribute) {
                attr_type = vc.data_type;
                break;
            }
        }

        *out_count = values.size();
        *out_data_type = to_c_data_type(attr_type);

        if (values.empty()) {
            *out_values = nullptr;
            *out_mask = nullptr;
            return QUIVER_OK;
        }

        // One mask for every data type: mask[i] == 0 means no data at or before date_time
        // (a null Value from the core), and the data slot is a placeholder.
        auto* mask = new uint8_t[values.size()];
        switch (*out_data_type) {
        case QUIVER_DATA_TYPE_INTEGER: {
            auto* arr = new int64_t[values.size()];
            for (size_t i = 0; i < values.size(); ++i) {
                const auto* value = std::get_if<int64_t>(&values[i]);
                arr[i] = value ? *value : 0;
                mask[i] = value ? 1 : 0;
            }
            *out_values = arr;
            break;
        }
        case QUIVER_DATA_TYPE_FLOAT: {
            auto* arr = new double[values.size()];
            for (size_t i = 0; i < values.size(); ++i) {
                const auto* value = std::get_if<double>(&values[i]);
                arr[i] = value ? *value : 0.0;
                mask[i] = value ? 1 : 0;
            }
            *out_values = arr;
            break;
        }
        case QUIVER_DATA_TYPE_STRING:
        case QUIVER_DATA_TYPE_DATE_TIME: {
            auto** arr = new char*[values.size()];
            for (size_t i = 0; i < values.size(); ++i) {
                const auto* value = std::get_if<std::string>(&values[i]);
                arr[i] = value ? quiver::string::new_c_str(*value) : nullptr;
                mask[i] = value ? 1 : 0;
            }
            *out_values = arr;
            break;
        }
        default:
            delete[] mask;
            throw std::runtime_error("Cannot read_time_series_row: unknown data type " +
                                     std::to_string(*out_data_type));
        }
        *out_mask = mask;

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

Why:
- The mask is set per type from the same `get_if` that fills the data slot, so `mask[i] == 1` exactly when the slot holds a real value.
- The `default:` branch cannot be reached (`to_c_data_type` throws first), but it now frees the mask it would otherwise leak.
- `QUIVER_REQUIRE` supports up to 9 arguments (`src/c/internal.h`, `QUIVER_REQUIRE_9`), and this call uses exactly 9.

### 4. `bindings/julia/src/c_api.jl` — regenerate (currently ~L416-418)

Run `bindings/julia/generator/generator.bat` (it needs `julia +1.12.5` via juliaup). The only expected diff is this function:

```julia
function quiver_database_read_time_series_row(db, collection, group, attribute, date_time, out_data_type, out_values, out_mask, out_count)
    @ccall libquiver_c.quiver_database_read_time_series_row(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, group::Ptr{Cchar}, attribute::Ptr{Cchar}, date_time::Ptr{Cchar}, out_data_type::Ptr{Cint}, out_values::Ptr{Ptr{Cvoid}}, out_mask::Ptr{Ptr{UInt8}}, out_count::Ptr{Csize_t})::quiver_error_t
end
```

If the generator cannot run, hand-edit exactly these two lines, since they are what the generator emits (compare `quiver_database_read_scalar_integers` at ~L226-228). If `git diff bindings/julia/src/c_api.jl` shows other hunks, an earlier plan skipped its regeneration. The hunks still reflect the current headers, so keep them, but say so in the commit message.

### 5. `bindings/julia/src/database_read.jl` — `read_time_series_row` (currently ~L698-742)

Replace the whole function with:

```julia
function read_time_series_row(db::Database, collection::String, group::String, attribute::String; date_time::DateTime)
    out_data_type = Ref{Cint}(0)
    out_values = Ref{Ptr{Cvoid}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    dt_str = date_time_to_string(date_time)

    check(
        C.quiver_database_read_time_series_row(
            db.ptr, collection, group, attribute, dt_str,
            out_data_type, out_values, out_mask, out_count,
        ),
    )

    count = out_count[]
    data_type = out_data_type[]

    # Always Vector{Optional{T}} with T keyed on the column's data type, empty or not: a `nothing`
    # (mask 0) means "no data at or before date_time", so the optional is inherent to this reader.
    if data_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
        count == 0 && return Optional{Int64}[]
        int_ptr = reinterpret(Ptr{Int64}, out_values[])
        values = unsafe_wrap(Array, int_ptr, count)
        mask = unsafe_wrap(Array, out_mask[], count)
        result = Optional{Int64}[mask[i] != 0 ? values[i] : nothing for i in 1:count]
        C.quiver_database_free_integer_array(int_ptr)
        C.quiver_database_free_mask(out_mask[])
        return result
    elseif data_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
        count == 0 && return Optional{Float64}[]
        float_ptr = reinterpret(Ptr{Float64}, out_values[])
        values = unsafe_wrap(Array, float_ptr, count)
        mask = unsafe_wrap(Array, out_mask[], count)
        result = Optional{Float64}[mask[i] != 0 ? values[i] : nothing for i in 1:count]
        C.quiver_database_free_float_array(float_ptr)
        C.quiver_database_free_mask(out_mask[])
        return result
    elseif data_type == Cint(C.QUIVER_DATA_TYPE_STRING) || data_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME)
        count == 0 && return Optional{String}[]
        str_ptr_ptr = reinterpret(Ptr{Ptr{Cchar}}, out_values[])
        str_ptrs = unsafe_wrap(Array, str_ptr_ptr, count)
        mask = unsafe_wrap(Array, out_mask[], count)
        # Never unsafe_string a masked-out (NULL) pointer.
        result = Optional{String}[mask[i] != 0 ? unsafe_string(str_ptrs[i]) : nothing for i in 1:count]
        C.quiver_database_free_string_array(str_ptr_ptr, Csize_t(count))
        C.quiver_database_free_mask(out_mask[])
        return result
    end

    return throw(ArgumentError("Unsupported data type $(data_type) for attribute '$attribute'"))
end
```

Why:
- The old early-return block returned `Int64[]` / `Float64[]` / `Optional{String}[]` / `Any[]` for the empty path. Each branch now returns its own `Optional{T}[]`, so the element type never depends on whether data was found. The C API sets `*out_data_type` even for an empty collection (step 3), which is what makes this possible.
- The straight-line frees match `read_scalar_integers` in the same file. Plan 36 owns `finally` for `read_time_series_group` only.

### 6. `bindings/dart/lib/src/ffi/bindings.dart` — hand-edit `quiver_database_read_time_series_row` (currently ~L2378-2427)

Do **not** run ffigen. Replace the whole block, from `int quiver_database_read_time_series_row(` through the `>();` that closes `_quiver_database_read_time_series_row`, with:

```dart
  int quiver_database_read_time_series_row(
    ffi.Pointer<quiver_database_t> db,
    ffi.Pointer<ffi.Char> collection,
    ffi.Pointer<ffi.Char> group,
    ffi.Pointer<ffi.Char> attribute,
    ffi.Pointer<ffi.Char> date_time,
    ffi.Pointer<ffi.Int> out_data_type,
    ffi.Pointer<ffi.Pointer<ffi.Void>> out_values,
    ffi.Pointer<ffi.Pointer<ffi.Uint8>> out_mask,
    ffi.Pointer<ffi.Size> out_count,
  ) {
    return _quiver_database_read_time_series_row(
      db,
      collection,
      group,
      attribute,
      date_time,
      out_data_type,
      out_values,
      out_mask,
      out_count,
    );
  }

  late final _quiver_database_read_time_series_rowPtr =
      _lookup<
        ffi.NativeFunction<
          ffi.Int32 Function(
            ffi.Pointer<quiver_database_t>,
            ffi.Pointer<ffi.Char>,
            ffi.Pointer<ffi.Char>,
            ffi.Pointer<ffi.Char>,
            ffi.Pointer<ffi.Char>,
            ffi.Pointer<ffi.Int>,
            ffi.Pointer<ffi.Pointer<ffi.Void>>,
            ffi.Pointer<ffi.Pointer<ffi.Uint8>>,
            ffi.Pointer<ffi.Size>,
          )
        >
      >('quiver_database_read_time_series_row');
  late final _quiver_database_read_time_series_row = _quiver_database_read_time_series_rowPtr
      .asFunction<
        int Function(
          ffi.Pointer<quiver_database_t>,
          ffi.Pointer<ffi.Char>,
          ffi.Pointer<ffi.Char>,
          ffi.Pointer<ffi.Char>,
          ffi.Pointer<ffi.Char>,
          ffi.Pointer<ffi.Int>,
          ffi.Pointer<ffi.Pointer<ffi.Void>>,
          ffi.Pointer<ffi.Pointer<ffi.Uint8>>,
          ffi.Pointer<ffi.Size>,
        )
      >();
```

It has three insertion points (the wrapper parameters and call, the `NativeFunction` type, and the `asFunction` type), and every one needs the new line in the same position.

### 7. `bindings/dart/lib/src/database_read.dart` — `readTimeSeriesRow` (currently ~L1279-1340)

Keep the doc comment exactly as it is, including any sentence plan 04 appended. It already says "elements with no matching data yield `null`", which becomes true. Replace the method body with:

```dart
  List<Object?> readTimeSeriesRow(
    String collection,
    String group,
    String attribute,
    DateTime dateTime,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outDataType = arena<Int>();
      final outValues = arena<Pointer<Void>>();
      final outMask = arena<Pointer<Uint8>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_time_series_row(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          dateTimeToString(dateTime).toNativeUtf8(allocator: arena).cast(),
          outDataType,
          outValues,
          outMask,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      // mask[i] == 0: no data at or before [dateTime]; the data slot is a
      // placeholder and is never read.
      final mask = outMask.value;
      switch (outDataType.value) {
        case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
          final ptr = outValues.value.cast<Int64>();
          final result = List<Object?>.generate(
            count,
            (i) => mask[i] != 0 ? ptr[i] : null,
          );
          bindings.quiver_database_free_integer_array(ptr);
          bindings.quiver_database_free_mask(mask);
          return result;
        case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
          final ptr = outValues.value.cast<Double>();
          final result = List<Object?>.generate(
            count,
            (i) => mask[i] != 0 ? ptr[i] : null,
          );
          bindings.quiver_database_free_float_array(ptr);
          bindings.quiver_database_free_mask(mask);
          return result;
        default:
          // STRING or DATE_TIME; never toDartString a masked-out (NULL) pointer
          final ptr = outValues.value.cast<Pointer<Char>>();
          final result = List<Object?>.generate(
            count,
            (i) => mask[i] != 0 ? ptr[i].cast<Utf8>().toDartString() : null,
          );
          bindings.quiver_database_free_string_array(ptr, count);
          bindings.quiver_database_free_mask(mask);
          return result;
      }
    } finally {
      arena.releaseAll();
    }
  }
```

The return type stays `List<Object?>`: the reader also returns `String`s.

### 8. `bindings/python/src/quiverdb/_c_api.py` — cdef (currently ~L305-307)

Current:

```c
    quiver_error_t quiver_database_read_time_series_row(quiver_database_t* db,
        const char* collection, const char* group, const char* attribute,
        const char* date_time, int* out_data_type, void** out_values, size_t* out_count);
```

New:

```c
    quiver_error_t quiver_database_read_time_series_row(quiver_database_t* db,
        const char* collection, const char* group, const char* attribute,
        const char* date_time, int* out_data_type, void** out_values, uint8_t** out_mask,
        size_t* out_count);
```

(`quiver_database_free_mask` is already declared, at ~L181.)

### 9. `bindings/python/src/quiverdb/database.py` — `read_time_series_row` (currently ~L1483-1531)

Keep the signature (`-> list`), the docstring (including plan 04's sentence) and the `date_time.strftime(...)` line (plan 25 owns it). Replace the body after the docstring with:

```python
        self._ensure_open()
        lib = get_lib()
        out_data_type = ffi.new("int*")
        out_values = ffi.new("void**")
        out_mask = ffi.new("uint8_t**")
        out_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_time_series_row(
                self._ptr,
                collection.encode("utf-8"),
                group.encode("utf-8"),
                attribute.encode("utf-8"),
                date_time.strftime("%Y-%m-%dT%H:%M:%S").encode("utf-8"),
                out_data_type,
                out_values,
                out_mask,
                out_count,
            )
        )
        count = out_count[0]
        if count == 0 or out_values[0] == ffi.NULL:
            return []
        data_type = out_data_type[0]
        # mask[i] falsy: no data at or before date_time; the data slot is a placeholder
        mask = out_mask[0]
        if data_type == DataType.INTEGER:
            int_ptr = ffi.cast("int64_t*", out_values[0])
            try:
                return [int_ptr[i] if mask[i] else None for i in range(count)]
            finally:
                lib.quiver_database_free_integer_array(int_ptr)
                lib.quiver_database_free_mask(mask)
        if data_type == DataType.FLOAT:
            float_ptr = ffi.cast("double*", out_values[0])
            try:
                return [float_ptr[i] if mask[i] else None for i in range(count)]
            finally:
                lib.quiver_database_free_float_array(float_ptr)
                lib.quiver_database_free_mask(mask)
        # STRING or DATE_TIME; never ffi.string a masked-out (NULL) pointer
        str_ptr = ffi.cast("char**", out_values[0])
        try:
            return [ffi.string(str_ptr[i]).decode("utf-8") if mask[i] else None for i in range(count)]
        finally:
            lib.quiver_database_free_string_array(str_ptr, count)
            lib.quiver_database_free_mask(mask)
```

This is the `read_scalar_integers` shape (`try: return ... finally: free both`), once per type. A `UnicodeDecodeError` in the string branch no longer leaks the native arrays.

### 10. `bindings/js/src/loader.ts` — symbol table (currently ~L134)

Current: `  quiver_database_read_time_series_row: { args: [P, BUF, BUF, BUF, BUF, P, P, P], returns: I32 },`
New: `  quiver_database_read_time_series_row: { args: [P, BUF, BUF, BUF, BUF, P, P, P, P], returns: I32 },`

(`quiver_database_free_mask` is already present, at ~L183.)

### 11. `bindings/js/src/time-series.ts` — `Database.prototype.readTimeSeriesRow` (currently ~L127-179)

Replace the whole function with:

```ts
Database.prototype.readTimeSeriesRow = function (
  this: Database,
  collection: string,
  group: string,
  attribute: string,
  dateTime: string,
): (number | string | null)[] {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const attrBuf = toCString(attribute);
  const dtBuf = toCString(dateTime);
  const outDataType = new Uint8Array(4);
  const outValues = allocPtrOut();
  const outMask = allocPtrOut();
  const outCount = allocUint64Out();

  check(
    lib.quiver_database_read_time_series_row(
      this._handle,
      collBuf.buf,
      grpBuf.buf,
      attrBuf.buf,
      dtBuf.buf,
      outDataType,
      outValues.buf,
      outMask.buf,
      outCount.buf,
    ),
  );

  const count = readUint64Out(outCount);
  const valuesPtr = readPtrOut(outValues);
  if (count === 0 || !valuesPtr) return [];

  // mask[i] === 0: no data at or before dateTime; the data slot is a placeholder, never read.
  const maskPtr = readPtrOut(outMask);
  const mask = new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, count));
  const dataType = new DataView(outDataType.buffer).getInt32(0, true);
  switch (dataType) {
    case DATA_TYPE_INTEGER: {
      const result = decodeInt64Array(valuesPtr, count).map((v, i) => (mask[i] ? v : null));
      lib.quiver_database_free_integer_array(valuesPtr);
      lib.quiver_database_free_mask(maskPtr);
      return result;
    }
    case DATA_TYPE_FLOAT: {
      const result = decodeFloat64Array(valuesPtr, count).map((v, i) => (mask[i] ? v : null));
      lib.quiver_database_free_float_array(valuesPtr);
      lib.quiver_database_free_mask(maskPtr);
      return result;
    }
    default: {
      // STRING or DATE_TIME; never build a CString from a masked-out (NULL) pointer
      const result: (string | null)[] = new Array(count);
      for (let i = 0; i < count; i++) {
        result[i] = mask[i]
          ? new CString(read.ptr(valuesPtr as Pointer, i * 8) as Pointer).toString()
          : null;
      }
      lib.quiver_database_free_string_array(valuesPtr, BigInt(count));
      lib.quiver_database_free_mask(maskPtr);
      return result;
    }
  }
};
```

`toArrayBuffer`, `CString`, `read` and `Pointer` are already imported at the top of the file. The `mask` view points into native memory, and every read of it happens before `quiver_database_free_mask`.

### 12. Lua — no code change

`read_time_series_row_lua` (`src/lua_runner.cpp`) calls the C++ core directly and already yields `nil`. `bindings/js/src/lua-api.ts` already says "Elements with no matching data yield `nil`", so it stays as it is.

## Tests

No new schema files. Every new test uses `tests/schemas/valid/mixed_time_series.sql` (`Sensor` plus `Sensor_time_series_readings`: `temperature REAL`, `humidity INTEGER`, `status TEXT`), which is already used across suites.

### C API — `tests/test_c_api_database_time_series_row.cpp`

Every call to `quiver_database_read_time_series_row` in this file gains `&out_mask` (or `nullptr` in the null-argument case) between `&out_values` and `&out_count`. Declare `uint8_t* out_mask = nullptr;` next to `void* out_values = nullptr;` in each test. Find the calls with `grep -n "quiver_database_read_time_series_row" tests/test_c_api_database_time_series_row.cpp`. **Keep `#include <cmath>`**, because `NAN` is still used at ~L937.

1. `TEST(DatabaseCApi, ReadTimeSeriesRow)`
   - After each read's `ASSERT_EQ(out_count, 2);` add `EXPECT_EQ(out_mask[0], 1);` and `EXPECT_EQ(out_mask[1], 1);`.
   - After each `quiver_database_free_float_array(floats);` add `quiver_database_free_mask(out_mask);`. There are two reads, so two frees.
2. `TEST(DatabaseCApi, ReadTimeSeriesRowBeforeAllData)`: replace everything from the comment line through the end of the test body. Old:
   ```cpp
       // Query before any data: value should be NaN (null sentinel for float)
       int out_type = 0;
       void* out_values = nullptr;
       size_t out_count = 0;
       auto err = quiver_database_read_time_series_row(
           db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_count);
       EXPECT_EQ(err, QUIVER_OK);
       ASSERT_EQ(out_count, 1);
       EXPECT_EQ(out_type, QUIVER_DATA_TYPE_FLOAT);

       auto* floats = static_cast<double*>(out_values);
       EXPECT_TRUE(std::isnan(floats[0]));

       quiver_database_free_float_array(floats);
       quiver_database_close(db);
   ```
   New:
   ```cpp
       // Query before any data: the element is masked out (no data at or before the date)
       int out_type = 0;
       void* out_values = nullptr;
       uint8_t* out_mask = nullptr;
       size_t out_count = 0;
       auto err = quiver_database_read_time_series_row(
           db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, &out_mask, &out_count);
       EXPECT_EQ(err, QUIVER_OK);
       ASSERT_EQ(out_count, 1);
       EXPECT_EQ(out_type, QUIVER_DATA_TYPE_FLOAT);
       EXPECT_EQ(out_mask[0], 0);

       quiver_database_free_float_array(static_cast<double*>(out_values));
       quiver_database_free_mask(out_mask);
       quiver_database_close(db);
   ```
3. `TEST(DatabaseCApi, ReadTimeSeriesRowEmptyCollection)`
   - Declare the mask with a non-NULL sentinel so the test proves the C API writes NULL:
     ```cpp
         uint8_t mask_sentinel = 1;
         uint8_t* out_mask = &mask_sentinel;
     ```
   - Pass `&out_mask`, and after `EXPECT_EQ(out_values, nullptr);` add `EXPECT_EQ(out_mask, nullptr);`.
4. `TEST(DatabaseCApi, ReadTimeSeriesRowMultiColumnInteger)`
   - After `EXPECT_EQ(ints[0], 70);` add `EXPECT_EQ(out_mask[0], 1);`, and after `quiver_database_free_integer_array(ints);` add `quiver_database_free_mask(out_mask);`.
   - After `EXPECT_STREQ(strings[0], "ok");` add `EXPECT_EQ(out_mask[0], 1);`, and after `quiver_database_free_string_array(strings, out_count);` add `quiver_database_free_mask(out_mask);`.
5. `TEST(DatabaseCApi, ReadTimeSeriesRowNullArguments)`
   - Declare `uint8_t* out_mask = nullptr;` and add `&out_mask` to all eight existing calls.
   - Then add the ninth case before `quiver_database_close(db);`:
     ```cpp
         EXPECT_EQ(quiver_database_read_time_series_row(
                       db, "Collection", "data", "value", "2024-01-01", &out_type, &out_values, nullptr, &out_count),
                   QUIVER_ERROR);
         EXPECT_STREQ(quiver_get_last_error(), "Null argument: out_mask");
     ```
6. `TEST(DatabaseCApi, ReadTimeSeriesRowAttributeNotFound)` and `TEST(DatabaseCApi, ReadTimeSeriesRowGroupNotFound)`: declare `out_mask` and pass `&out_mask`. No other change.
7. Plan 04's `TEST(DatabaseCApi, ReadTimeSeriesRowRejectsMultiDimensionGroup)`, if present: declare `uint8_t* out_mask = nullptr;`, pass `&out_mask`, and after `EXPECT_EQ(out_values, nullptr);` add `EXPECT_EQ(out_mask, nullptr);`.
8. **New**, added after `ReadTimeSeriesRowMultiColumnInteger`:

```cpp
TEST(DatabaseCApi, ReadTimeSeriesRowNoDataIsMaskedForEveryType) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("mixed_time_series.sql").c_str(), &options, &db),
              QUIVER_OK);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t tmp_id = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id);
    quiver_element_destroy(config);

    quiver_element_t* s1 = nullptr;
    ASSERT_EQ(quiver_element_create(&s1), QUIVER_OK);
    quiver_element_set_string(s1, "label", "Sensor 1");
    int64_t id1 = 0;
    quiver_database_create_element(db, "Sensor", s1, &id1);
    quiver_element_destroy(s1);

    quiver_element_t* s2 = nullptr;
    ASSERT_EQ(quiver_element_create(&s2), QUIVER_OK);
    quiver_element_set_string(s2, "label", "Sensor 2");  // no time series rows
    int64_t id2 = 0;
    quiver_database_create_element(db, "Sensor", s2, &id2);
    quiver_element_destroy(s2);

    // Sensor 1 stores humidity 0: a real value the old INTEGER sentinel could not be told apart from.
    const char* col_names[] = {"date_time", "temperature", "humidity", "status"};
    int col_types[] = {
        QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_STRING};
    const char* dts[] = {"2024-01-01"};
    double temps[] = {20.5};
    int64_t humids[] = {0};
    const char* stats[] = {"ok"};
    const void* data[] = {dts, temps, humids, stats};
    ASSERT_EQ(quiver_database_update_time_series_group(
                  db, "Sensor", "readings", id1, col_names, col_types, data, nullptr, 4, 1),
              QUIVER_OK);

    int out_type = 0;
    void* out_values = nullptr;
    uint8_t* out_mask = nullptr;
    size_t out_count = 0;

    // INTEGER
    ASSERT_EQ(quiver_database_read_time_series_row(
                  db, "Sensor", "readings", "humidity", "2024-01-01", &out_type, &out_values, &out_mask, &out_count),
              QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_INTEGER);
    ASSERT_EQ(out_count, 2);
    EXPECT_EQ(out_mask[0], 1);
    EXPECT_EQ(static_cast<int64_t*>(out_values)[0], 0);
    EXPECT_EQ(out_mask[1], 0);
    quiver_database_free_integer_array(static_cast<int64_t*>(out_values));
    quiver_database_free_mask(out_mask);

    // FLOAT
    ASSERT_EQ(quiver_database_read_time_series_row(db,
                                                   "Sensor",
                                                   "readings",
                                                   "temperature",
                                                   "2024-01-01",
                                                   &out_type,
                                                   &out_values,
                                                   &out_mask,
                                                   &out_count),
              QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_FLOAT);
    ASSERT_EQ(out_count, 2);
    EXPECT_EQ(out_mask[0], 1);
    EXPECT_DOUBLE_EQ(static_cast<double*>(out_values)[0], 20.5);
    EXPECT_EQ(out_mask[1], 0);
    quiver_database_free_float_array(static_cast<double*>(out_values));
    quiver_database_free_mask(out_mask);

    // STRING: masked too (one decode for every type); the data slot is a NULL char*
    ASSERT_EQ(quiver_database_read_time_series_row(
                  db, "Sensor", "readings", "status", "2024-01-01", &out_type, &out_values, &out_mask, &out_count),
              QUIVER_OK);
    EXPECT_EQ(out_type, QUIVER_DATA_TYPE_STRING);
    ASSERT_EQ(out_count, 2);
    auto** strings = static_cast<char**>(out_values);
    EXPECT_EQ(out_mask[0], 1);
    EXPECT_STREQ(strings[0], "ok");
    EXPECT_EQ(out_mask[1], 0);
    EXPECT_EQ(strings[1], nullptr);
    quiver_database_free_string_array(strings, out_count);
    quiver_database_free_mask(out_mask);

    quiver_database_close(db);
}
```

(`clang-format` will re-wrap the calls. The layout above does not matter.)

Before the fix the whole file fails to compile, because the header has no `out_mask`. In behaviour, `EXPECT_EQ(out_mask[1], 0)` for `humidity` is exactly what the old encoding could not express: the old API returned `0` for both sensors.

### C++ core — `tests/test_database_time_series_row.cpp`

No change. `ReadTimeSeriesRowBeforeAllData` and `ReadTimeSeriesRowMixedElements` already pin `std::nullptr_t` for no data, and the core does not change.

### Lua — `tests/test_lua_runner_time_series.cpp`

Add after `TEST_F(LuaRunnerTest, ReadTimeSeriesRow)` (and after plan 04's `ReadTimeSeriesRowRejectsMultiDimensionGroup` if it is there):

```cpp
TEST_F(LuaRunnerTest, ReadTimeSeriesRowNoDataIsNil) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Config" })
        local id1 = db:create_element("Sensor", { label = "Sensor 1" })
        db:create_element("Sensor", { label = "Sensor 2" })
        db:update_time_series_group("Sensor", "readings", id1, {
            date_time = { "2024-01-01T00:00:00" },
            temperature = { 20.5 },
            humidity = { 0 },
            status = { "ok" },
        })

        for _, attribute in ipairs({ "temperature", "humidity", "status" }) do
            local row = db:read_time_series_row("Sensor", "readings", attribute, "2024-01-01T00:00:00")
            assert(row[1] ~= nil, attribute .. ": Sensor 1 has data")
            assert(row[2] == nil, attribute .. ": Sensor 2 has no data, got " .. tostring(row[2]))
        end
        local humidity = db:read_time_series_row("Sensor", "readings", "humidity", "2024-01-01T00:00:00")
        assert(humidity[1] == 0, "a stored 0 is a value, got " .. tostring(humidity[1]))
    )");
}
```

This passes before and after the fix. It pins the layer that was already right, so every layer now asserts the same contract.

### Julia — `bindings/julia/test/test_database_time_series_row.jl`

Change these existing assertions:

| Testset | Old (line ~) | New |
|---|---|---|
| "Read Time Series Row" | `@test result isa Vector{Float64}` (~L173) | `@test result isa Vector{Quiver.Optional{Float64}}` |
| "Read Time Series Row With Missing Elements" | `@test result isa Vector{Float64}` (~L205) | `@test result isa Vector{Quiver.Optional{Float64}}` |
| "Read Time Series Row - Before All Data" | comment `# Query before any data: should return NaN for the float attribute` (~L230) and `@test isnan(result[1])` (~L233) | comment `# Query before any data: the element has no value, so the entry is nothing`; add `@test result isa Vector{Quiver.Optional{Float64}}` before `@test length(result) == 1`; replace the `isnan` line with `@test result[1] === nothing` |
| "Read Time Series Row - Empty Collection" | `@test result isa Vector{Float64}` (~L246) | `@test result isa Vector{Quiver.Optional{Float64}}` |
| "Read Time Series Row - Mixed Elements" | comment `# Item 1 has data, Item 2 doesn't (NaN sentinel)` (~L264) and `@test isnan(result[2])` (~L268) | comment `# Item 1 has data, Item 2 doesn't (nothing)`; `@test result[2] === nothing` |
| "Read Time Series Row - Multi-Column Integer" | `@test humids isa Vector{Int64}` (~L289) | `@test humids isa Vector{Quiver.Optional{Int64}}` |
| same | `@test temps isa Vector{Float64}` (~L299) | `@test temps isa Vector{Quiver.Optional{Float64}}` |

Add a new testset after "Read Time Series Row - String Null Distinguished From Empty" (the last one in the file):

```julia
    @testset "Read Time Series Row - No Data Is Nothing In Every Type" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id1 = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")
        Quiver.create_element!(db, "Sensor"; label = "Sensor 2")  # no data

        Quiver.update_time_series_group!(db, "Sensor", "readings", id1;
            date_time = ["2024-01-02T00:00:00"],
            temperature = [20.5],
            humidity = [0],
            status = ["ok"],
        )

        # A stored 0 and "no data" are distinguishable.
        humids = Quiver.read_time_series_row(db, "Sensor", "readings", "humidity"; date_time = DateTime(2024, 1, 2))
        @test humids isa Vector{Quiver.Optional{Int64}}
        @test humids[1] == 0
        @test humids[2] === nothing

        temps = Quiver.read_time_series_row(db, "Sensor", "readings", "temperature"; date_time = DateTime(2024, 1, 2))
        @test temps isa Vector{Quiver.Optional{Float64}}
        @test temps[1] == 20.5
        @test temps[2] === nothing

        # Before the first row even Sensor 1 has no data; the element type does not change.
        before = Quiver.read_time_series_row(db, "Sensor", "readings", "humidity"; date_time = DateTime(2024, 1, 1))
        @test before isa Vector{Quiver.Optional{Int64}}
        @test all(isnothing, before)

        Quiver.close!(db)
    end
```

It fails before the fix: `humids isa Vector{Quiver.Optional{Int64}}` is false (the old type was `Vector{Int64}`), and `humids[2]` was `0`.

### Dart — `bindings/dart/test/database_time_series_row_test.dart`

Add inside `group('Read Time Series Row', ...)`, after `test('readTimeSeriesRow returns empty list without elements', ...)` (and after plan 04's multi-dimension test if present):

```dart
    test('readTimeSeriesRow returns null for an element with no data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        final id1 = db.createElement('Sensor', {'label': 'Sensor 1'});
        db.createElement('Sensor', {'label': 'Sensor 2'}); // no rows

        db.updateTimeSeriesGroup('Sensor', 'readings', id1, {
          'date_time': ['2024-01-02T00:00:00'],
          'temperature': [20.5],
          'humidity': [0],
          'status': ['ok'],
        });

        // A stored 0 and "no data" are distinguishable.
        final at = DateTime(2024, 1, 2);
        expect(
          db.readTimeSeriesRow('Sensor', 'readings', 'humidity', at),
          equals([0, null]),
        );
        expect(
          db.readTimeSeriesRow('Sensor', 'readings', 'temperature', at),
          equals([20.5, null]),
        );
        expect(
          db.readTimeSeriesRow('Sensor', 'readings', 'status', at),
          equals(['ok', null]),
        );

        // Before the first row even Sensor 1 has no data.
        final before = DateTime(2024, 1, 1);
        expect(
          db.readTimeSeriesRow('Sensor', 'readings', 'humidity', before),
          equals([null, null]),
        );
        expect(
          db.readTimeSeriesRow('Sensor', 'readings', 'temperature', before),
          equals([null, null]),
        );
      } finally {
        db.close();
      }
    });
```

It fails before the fix: `humidity` read back as `[0, 0]` and `temperature` as `[20.5, NaN]`.

### Python — `bindings/python/tests/test_database_time_series_row.py`

Add to `class TestReadTimeSeriesRow`, after `test_read_time_series_row_unknown_attribute_raises` (and after plan 04's test if present). It uses the conftest fixture `mixed_time_series_db` and the module's existing `_create_sensor` helper:

```python
    def test_read_time_series_row_no_data_is_none(self, mixed_time_series_db: Database) -> None:
        """An element with no row at or before the date reads None in every column type, never 0 or nan."""
        id1 = _create_sensor(mixed_time_series_db, "Sensor 1")
        _create_sensor(mixed_time_series_db, "Sensor 2")  # no rows
        mixed_time_series_db.update_time_series_group(
            "Sensor",
            "readings",
            id1,
            {"date_time": ["2024-01-02T00:00:00"], "temperature": [20.5], "humidity": [0], "status": ["ok"]},
        )

        # A stored 0 and "no data" are distinguishable.
        at = datetime(2024, 1, 2)
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "humidity", at) == [0, None]
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "temperature", at) == [20.5, None]
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "status", at) == ["ok", None]

        # Before the first row even Sensor 1 has no data.
        before = datetime(2024, 1, 1)
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "humidity", before) == [None, None]
        assert mixed_time_series_db.read_time_series_row("Sensor", "readings", "temperature", before) == [None, None]
```

It fails before the fix: `[0, 0] != [0, None]`, and `[20.5, nan] != [20.5, None]`.

### JS — `bindings/js/test/database-time-series-row.test.ts`

Add inside `describe("readTimeSeriesRow", ...)`, after `test("returns empty array when the collection has no elements", ...)` (and after plan 04's test if present). `MIXED_TS_SCHEMA` is already defined in the file:

```ts
  test("returns null for an element with no data, in every column type", () => {
    const db = Database.fromSchema(":memory:", MIXED_TS_SCHEMA);
    try {
      const id1 = db.createElement("Sensor", { label: "Sensor 1" });
      db.createElement("Sensor", { label: "Sensor 2" }); // no rows

      db.updateTimeSeriesGroup("Sensor", "readings", id1, {
        date_time: ["2024-01-02T00:00:00"],
        temperature: [20.5],
        humidity: [0],
        status: ["ok"],
      });

      // A stored 0 and "no data" are distinguishable.
      const at = "2024-01-02T00:00:00";
      expect(db.readTimeSeriesRow("Sensor", "readings", "humidity", at)).toEqual([0, null]);
      expect(db.readTimeSeriesRow("Sensor", "readings", "temperature", at)).toEqual([20.5, null]);
      expect(db.readTimeSeriesRow("Sensor", "readings", "status", at)).toEqual(["ok", null]);

      // Before the first row even Sensor 1 has no data.
      const before = "2024-01-01T00:00:00";
      expect(db.readTimeSeriesRow("Sensor", "readings", "humidity", before)).toEqual([null, null]);
      expect(db.readTimeSeriesRow("Sensor", "readings", "temperature", before)).toEqual([null, null]);
    } finally {
      db.close();
    }
  });
```

It fails before the fix: `[0, 0]` and `[20.5, NaN]`.

## Docs and changelog

### `src/c/AGENTS.md` — "Multi-Column Time Series", the row bullet (currently ~L198-201)

Old:

```
- `quiver_database_read_time_series_row()` returns a single `void*` array whose element type the
  caller dispatches on via `out_data_type`. Null entries are encoded per type: `FLOAT` → NaN,
  `STRING`/`DATE_TIME` → NULL `char*`, `INTEGER` → 0. (The row API keeps its sentinel encoding; only
  the columnar group API uses the presence mask.)
```

New:

```
- `quiver_database_read_time_series_row()` returns a single `void*` array whose element type the
  caller dispatches on via `out_data_type`, plus a parallel `uint8_t** out_mask` for **every** type
  (`mask[i] == 0` = no data at or before `date_time`; the data slot is then a placeholder: `INTEGER`
  0, `FLOAT` 0.0, `STRING`/`DATE_TIME` NULL `char*`). Unlike `read_scalar_strings`, strings are
  masked too, so every binding decodes one way. The data array is freed by the typed free function
  and the mask by `quiver_database_free_mask`; both are NULL for an empty collection. The old
  0 / NaN sentinels are gone: a stored 0 was indistinguishable from "no data".
```

### Root `AGENTS.md` — "Core API" → "Database Class", the "Time series row" bullet (currently L611)

Append this sentence at the **end** of the bullet, after any sentences plan 04 added:

```
The C API carries that null as a presence mask (`out_mask`, one entry per element for every data type, freed by `quiver_database_free_mask`), and Julia returns `Vector{Optional{T}}` for every column type.
```

The existing "null Value ... (bindings surface `nothing`/`null`/`None`/`nil`)" wording is now true, so keep it.

### `bindings/julia/type_stability_followup.md`

Delete the whole section, from the heading `### \`read_time_series_row\` — fix the real instability (different bug)` (currently L34) through the end of its paragraph ("...not about nullability.", currently L43), plus the blank line after it. The fix is done, and the section's premise ("`nothing` for elements with no matching data") was false until now. Leave every other section unchanged.

### `bindings/julia/AGENTS.md` — "Time-series group NULLs" bullet (currently ~L66-72)

Old fragment: `` `Vector{Union{T, Nothing}}` **always** (type-stable, like the `Optional{String}` precedent in
  `read_time_series_row`) — a NULL cell is `nothing`; ``
New fragment: `` `Vector{Union{T, Nothing}}` **always** (type-stable, like `read_time_series_row`) — a NULL cell is `nothing`; ``

Then add a new bullet directly after that bullet:

```
- **`read_time_series_row` always returns `Vector{Optional{T}}`**, `T` keyed on the returned
  `data_type` (`Int64` / `Float64` / `String` for STRING and DATE_TIME), on the empty path too. Its
  `nothing` means "no data at or before `date_time`", so the optional is inherent — never narrow it
  by `not_null`. It decodes the C API's `out_mask` (returned for every data type, freed with
  `quiver_database_free_mask`) and never `unsafe_string`s a masked-out pointer.
```

### `bindings/dart/AGENTS.md`

1. In the "checked-in `bindings.dart` predates the pinned ffigen" bullet, old:
   ```
     `quiver_database_upsert_time_series_row` plus its `_by_label` form, and
     `quiver_database_update_relation` plus its `_by_label` form.
   ```
   new:
   ```
     `quiver_database_upsert_time_series_row` plus its `_by_label` form,
     `quiver_database_update_relation` plus its `_by_label` form, and the `out_mask` parameter of
     `quiver_database_read_time_series_row`.
   ```
2. At the end of the "**Scalar bulk NULLs**" bullet, after "(regenerate via ffigen; clear `.dart_tool` caches on C-API changes).", append: `` `readTimeSeriesRow` decodes the same kind of mask, which the C API returns for every column type (mask 0 = no data at or before the date → `null`; the string branch never `toDartString`s a masked-out pointer). ``

### `bindings/python/AGENTS.md` — "**Scalar bulk NULLs**" bullet (currently ~L53-57)

After "`_c_api.py` carries the mask out-param on the two numeric readers plus `quiver_database_free_mask`.", append: `` `read_time_series_row` decodes the same kind of mask, which the C API returns for **every** column type (strings included): mask 0 (no data at or before the date) → `None`, and each type branch frees the data array and the mask in a `finally`. ``

### `bindings/js/AGENTS.md` — "**Scalar bulk NULLs**" bullet (currently ~L82-86)

Old: `` `loader.ts` carries the
  mask arg on the two numeric symbols + `quiver_database_free_mask` (hand-maintained, no generator). ``
New: `` `loader.ts` carries the
  mask arg on the two numeric symbols and on `quiver_database_read_time_series_row`, plus
  `quiver_database_free_mask` (hand-maintained, no generator). `readTimeSeriesRow` gates every column
  type on its mask the same way (mask 0 = no data at or before the date → `null`) and builds a
  `CString` only for an unmasked slot. ``

### `docs/time_series.md` — "### Reading a single row"

The "Rules" paragraph and its `[1.0, nothing]` example are already correct, so leave them (plan 04's added paragraph stays too). After the Julia code block that ends with `date_time = DateTime(2020),\n)` and its closing fence, add:

```
An element with no data at or before `date_time` reads as `nothing`, so in Julia the result is
always a `Vector{Union{T, Nothing}}`, `T` taken from the attribute's type (`Int64`, `Float64` or
`String`), even when every element has data.
```

### Binding doc comments

No change is needed. The Dart doc ("elements with no matching data yield `null`") and the Python docstring ("elements with no matching data yield None") become true. JS has no doc comment. Do not add one.

### `CHANGELOG.md` — `## [0.11.0] — unreleased` → `### Changed`

Append as the **last bullet of `### Changed`**, directly above `### Fixed`. Keep bullets earlier plans added.

```markdown
- **BREAKING — `read_time_series_row()` returns null, not `0` / `NaN`, for an element with no
  data.** The C++ core and Lua always did. The C API collapsed the missing value into a sentinel
  (`0` for an INTEGER column, `NaN` for a REAL one), so Julia, Dart, Python and JS returned a `0`
  that could not be told apart from a stored `0`, and a `NaN` their own docs did not mention.
  `quiver_database_read_time_series_row` now takes a `uint8_t** out_mask` out-parameter between
  `out_values` and `out_count`, filled for every data type (`out_mask[i] == 0` = no data at or
  before `date_time`) and freed with `quiver_database_free_mask`. Every binding maps it to
  `nothing` / `null` / `None`. In Julia the result is now `Vector{Union{Nothing, T}}` for every
  column type, `T` from the attribute's type (`Int64`, `Float64` or `String`), including an empty
  result. It used to be `Vector{Int64}` / `Vector{Float64}` for numeric columns.

  *Adapt:* C callers pass `&out_mask` and free it with `quiver_database_free_mask`. Replace
  `isnan(x)` / `x == 0` no-data checks with a null check (`x === nothing`, `x == null`,
  `x is None`). Julia code typed on `Vector{Float64}` / `Vector{Int64}` must accept the `Union`
  element type (`something.(v, NaN)` gives back the old `Vector{Float64}` for a REAL column).
```

## Verification

Run from the repo root (`C:\Development\Quiver\quiver3`), in order:

1. `cmake --build build --config Debug`. It must compile cleanly. `quiver_c_tests` will not compile until every call in the C API test file has `out_mask`.
2. `./build/bin/quiver_c_tests.exe --gtest_filter="DatabaseCApi.ReadTimeSeriesRow*"`. All must pass, including the new `DatabaseCApi.ReadTimeSeriesRowNoDataIsMaskedForEveryType` (and plan 04's `ReadTimeSeriesRowRejectsMultiDimensionGroup` if present).
3. `./build/bin/quiver_tests.exe --gtest_filter="*ReadTimeSeriesRow*"`. The core tests pass unchanged, and `LuaRunnerTest.ReadTimeSeriesRowNoDataIsNil` passes.
4. `bindings/julia/generator/generator.bat`, then `git diff bindings/julia/src/c_api.jl`. The only hunk should be `quiver_database_read_time_series_row` (see step 4 of Changes). Do **not** run `scripts/generator.bat` or `bindings/dart/generator/generator.bat`.
5. `bindings/julia/test/test.bat`. The testset "Read Time Series Row - No Data Is Nothing In Every Type" and the updated row testsets must pass.
6. Dart: delete `bindings/dart/.dart_tool/hooks_runner/` and `bindings/dart/.dart_tool/lib/` so the hook rebuilds the native library. Then run `cd bindings/dart && dart analyze` (no new issues) and `bindings/dart/test/test.bat`. The test "readTimeSeriesRow returns null for an element with no data" must pass.
7. `bindings/python/tests/test.bat`. `test_read_time_series_row_no_data_is_none` must pass.
8. `bindings/js/test/test.bat`. The test "returns null for an element with no data, in every column type" must pass. `lua-api-sync.test.ts` is unaffected and must pass.
9. `cd bindings/js && bun run lint src/time-series.ts src/loader.ts test/database-time-series-row.test.ts`. There must be no new diagnostics in these files.
10. `scripts/format.bat`. Afterwards `git status` must list only the files this plan touched.
11. Consistency greps. Each must return nothing:
    - `grep -rn "NaN (null sentinel\|INTEGER -> 0, FLOAT -> NaN\|keeps its sentinel encoding" include src tests`
    - `grep -n "isnan" bindings/julia/test/test_database_time_series_row.jl`
    - `grep -n "fix the real instability" bindings/julia/type_stability_followup.md`
12. `scripts/test-all.bat`. Every suite must be green.

## Acceptance criteria

- [ ] `quiver_database_read_time_series_row` takes `uint8_t** out_mask` between `out_values` and `out_count`. It fills the mask for INTEGER, FLOAT, STRING and DATE_TIME, and sets it NULL for an empty collection. The header comment documents the mask and `quiver_database_free_mask`.
- [ ] `#include <limits>` is removed from `src/c/database_time_series.cpp`. `<cmath>` stays in the C API test file.
- [ ] `c_api.jl` is regenerated. `bindings.dart` is hand-edited in three places, `_c_api.py` is updated, and `loader.ts` has 9 args.
- [ ] Julia returns `Vector{Optional{Int64|Float64|String}}` on every path, including an empty collection.
- [ ] Dart, Python and JS return `null` / `None` / `null` for masked entries, and keep their return types.
- [ ] Python frees the data array and the mask in a `finally` in every type branch.
- [ ] The C API tests assert on the mask; no `isnan` assertion remains for this reader. There is a new null-argument case for `out_mask` and a new every-type no-data test.
- [ ] The new no-data tests exist in Lua, Julia, Dart, Python and JS. The old Julia `isnan` and concrete-type assertions are updated.
- [ ] `src/c/AGENTS.md`, root `AGENTS.md`, `bindings/{julia,dart,python,js}/AGENTS.md`, `type_stability_followup.md` and `docs/time_series.md` are edited as written above.
- [ ] `CHANGELOG.md` has the **BREAKING** entry as the last bullet of `### Changed` under 0.11.0. No manifest version is changed.
- [ ] `scripts/test-all.bat` is green.

## Pitfalls

- **Argument order.** `out_mask` goes between `out_values` and `out_count` in all of these places:
  - the C header and implementation
  - `c_api.jl`
  - the three Dart spots
  - `_c_api.py`
  - `loader.ts`
  - every call site

  Python's cdef and the JS loader are untyped `P` pointers, so a swap there is not a compile error. The C side would write a `size_t` into the mask slot and crash or corrupt memory.
- **Do not run `scripts/generator.bat`.** It also runs the Dart ffigen generator, which rewrites `bindings.dart` wholesale and turns the int-constant classes into enums (`bindings/dart/AGENTS.md`). Run only `bindings/julia/generator/generator.bat`.
- **Stale Dart native build.** If the old DLL is still in `.dart_tool/hooks_runner/`, the Dart tests call the 8-argument C function with 9 arguments and fail in confusing ways. Clear the cache first (Verification step 6).
- **The binding suites load `build/bin`.** Build the C library first (step 1). Python and JS put `build/bin` on `PATH` in their `test.bat`, and Julia resolves the in-tree `build/`.
- **`QUIVER_REQUIRE` supports at most 9 arguments** (`src/c/internal.h`). This call now uses all 9.
- **`<cmath>` in the C API test file is still needed** for `NAN` at ~L937 (an upsert test). Only `src/c/database_time_series.cpp` loses an include (`<limits>`).
- **Tests that pin the old behaviour and must change:**
  - C API: `ReadTimeSeriesRowBeforeAllData` (`std::isnan`), plus every call in the file (signature)
  - Julia: the seven assertions in the table above (`isnan` ×2, `isa Vector{Float64}` ×4, `isa Vector{Int64}` ×1)
  - Nothing in Dart, Python or JS pinned the sentinel.
- **Julia's empty path depends on `*out_data_type`** being set even when the collection is empty. It is set before the empty return in step 3, so keep that order.
- **JS `toArrayBuffer` returns a view into native memory.** Every read of `mask` must finish before `quiver_database_free_mask`, which is why each `.map` / loop runs before the frees.
- **Python `mask[i]`** on a `uint8_t*` cdata is an `int`, so test its truthiness. Do not compare it to bytes.
- **Line endings.** No `.bat` file is edited. If one is touched by accident, restore CRLF.
- **Non-STRICT edge case (unchanged in spirit).** A stored value whose SQLite storage class differs from the declared column type (possible only in a non-STRICT table, e.g. `1.5` in an INTEGER column) now reads as masked (null). It used to read as the 0 / NaN sentinel. No shipped schema can produce it.
- **The C++ core and Lua must not change.** If a Lua or core row test fails, something else broke.

## Out of scope

- The C++ core `Database::read_time_series_row` (multi-dimension guard and NULL join: plan 04; group lookup: plan 57; `execute` move: plan 53).
- `finally` frees for Julia `read_time_series_group` (plan 36) and the Dart group decoders (plan 40). The Dart `readTimeSeriesRow` keeps straight-line frees like `readScalarIntegers`.
- Python datetime formatting of the `date_time` argument (plan 25).
- The C API group marshaller's REAL→INTEGER narrowing (plan 23; its note says not to touch this reader).
- JS whole-group readers and any shared mask-decode helper in `group-columns.ts` (plan 18).
- Tightening the Python / Dart / JS return annotations to per-type lists (rejected: the reader also returns strings).
- Julia's concrete-vs-optional conversion for `read_time_series_group` (still tracked in `type_stability_followup.md`; no plan).
- Deleting the unreachable `default:` branch of the C switch.
- Lua code and `bindings/js/src/lua-api.ts` (already correct: `nil`).
