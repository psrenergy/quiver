# 22 — Query C API: one function per C++ query method (drop the _params split)

**Batch** 3 · **Severity** m · **Breaking** yes, for direct C API callers only: `quiver_database_query_{string,integer,float}` gain the three parameter arguments, and the `quiver_database_query_{string,integer,float}_params` functions are gone. No binding's public API changes (Julia, Dart, Python and JS keep their exact signatures). · **Size** M · **Layers** C API, Julia FFI (generated) + wrapper, Dart FFI (hand-edited) + wrapper, Python cdef + wrapper, JS loader + wrapper, C API tests, binding tests, docs/changelog
**Depends on** none.
**Overlaps with**
- **21** (delete dead C API symbols): also edits `bindings/julia/src/c_api.jl` (regenerated), `bindings/dart/lib/src/ffi/bindings.dart` (hand-edited), `bindings/python/src/quiverdb/_c_api.py`, `src/c/AGENTS.md` and the hand-edit list in `bindings/dart/AGENTS.md`. Different declarations and sentences. Anchor every edit by symbol name, not line number.
- **17, 18, 20, 23**: also change C headers and the same FFI files (`c_api.jl`, `bindings.dart`, `_c_api.py`, `loader.ts`). Different symbols.
- **30** (Python stale docstrings, `_c_api.py` header, a duplicate test): may touch `_c_api.py`'s header comment and Python tests. This plan edits only the query cdef block and renames one test in `tests/test_database_query.py`.
- **33** (JS bigint in query parameters): adds a `bigint` branch inside the loop of `marshalParams` in `bindings/js/src/query.ts`. This plan changes that function's early return and return value, not its loop. Whichever lands second rebases onto the other.
- **69** (C API test leaks and raw `delete[]` frees): edits `tests/test_c_api_database_query.cpp` too (the `delete[] value;` lines in `QueryStringReturnsValue` and `QueryStringWithParams`). **Leave those `delete[]` lines untouched here**, plan 69 owns them.
- **25, 28**: mention Python `_marshal_params` but both leave it unchanged. This plan also leaves its body unchanged.

## Why

C++ has one method per query type, with parameters defaulting to empty (`include/quiver/database.h`, currently ~L240):

```cpp
std::optional<std::string> query_string(const std::string& sql, const std::vector<Value>& parameters = {});
```

Lua binds one function per type too. The C API has **two** per type (`include/quiver/c/database.h`, currently ~L557-599):

```c
// Query methods - execute SQL and return first row's first column
QUIVER_C_API quiver_error_t quiver_database_query_string(quiver_database_t* db,
                                                         const char* sql,
                                                         char** out_value,
                                                         int* out_has_value);
...
// Parameterized query methods
...
QUIVER_C_API quiver_error_t quiver_database_query_string_params(quiver_database_t* db,
                                                                const char* sql,
                                                                const int* param_types,
                                                                const void* const* param_values,
                                                                size_t param_count,
                                                                char** out_value,
                                                                int* out_has_value);
```

The plain bodies in `src/c/database_query.cpp` (currently ~L43-105) are the `_params` bodies (~L109-193) minus the `convert_params` call. The `_params` form already accepts `(NULL, NULL, 0)`: its array check is gated on the count,

```cpp
    if (param_count > 0) {
        QUIVER_REQUIRE(param_types, param_values);
    }
```

and `tests/test_c_api_database_query.cpp` already calls it that way (`QueryParamsNullDb`, `QueryParameterCountMismatch`). In C++ both forms run the same `db.query_*(sql, {})`, so the plain form adds nothing.

Every binding pays for the split with a branch, and the branches disagree:

- JS, `bindings/js/src/query.ts` (~L85, ~L118, ~L157): `if (parameters && parameters.length > 0)` → `_params`, else plain.
- Python, `bindings/python/src/quiverdb/database.py` (~L429, ~L466, ~L504): `if parameters is not None and len(parameters) > 0`.
- Dart, `bindings/dart/lib/src/database_query.dart` (~L16, ~L63, ~L114): `if (parameters == null)` → plain. So an **empty** Dart list goes to `_params`, while an empty JS or Python list goes to the plain form.
- Julia, `bindings/julia/src/database_query.jl`: ten methods for five names. `query_string`/`query_integer`/`query_float` each have a 2-arg method (~L47-105) that copies the result handling of its 3-arg twin (~L113-204), and `query_boolean` (~L85, ~L173) and `query_date_time` (~L212, ~L222) are each written twice only to cover both arities.

The split also breaks the root `AGENTS.md` naming rule "C++ to C API: Prefix `quiver_database_` to the C++ method name": the cross-layer table lists `query_string()` → `quiver_database_query_string()`, but that C function cannot carry the parameters the C++ method takes.

**Second defect, same file: the parameter errors name an operation that does not exist.** `convert_params` (`src/c/database_query.cpp`, ~L10-37) hardcodes:

```cpp
                throw std::runtime_error("Cannot query: parameter at index " + std::to_string(i) +
                                         " has null string value");
...
            throw std::runtime_error("Cannot query: unknown parameter type " + std::to_string(param_types[i]));
```

Reproduction (today):

```cpp
int param_types[] = {999};
int64_t dummy = 0;
const void* param_values[] = {&dummy};
quiver_database_query_integer_params(db, "SELECT 1", param_types, param_values, 1, &value, &has_value);
// quiver_get_last_error() == "Cannot query: unknown parameter type 999"
```

No public method is called `query`. Root `AGENTS.md` "C++ Error Message Patterns", Pattern 1: "Validators thread the calling operation's name through so the `{operation}` is the public method the user called". The C API's other decoder already does it: `unmarshal_group_columns_to_rows(const char* caller, ...)` in `src/c/database_helpers.h` (~L217) builds `std::string("Cannot ") + caller + ": ..."`, and all eight call sites pass the C++ public name.

Principles violated: "Delete unused code", "Homogeneity" (bindings disagree on which path an empty list takes), the C++→C naming rule, and the Pattern 1 `{operation}` rule.

## Constraints and decisions

- **Maintainer decision (binding):** "BREAKING (C only; binding APIs unchanged). Also thread the operation name into convert_params so messages say "Cannot query_string:" (error-message#5). JS passes null (not ptr of an empty array) with count 0. Julia collapses to one method per type with parameters::Vector = [] (also query_boolean/query_date_time)."
- **Root `AGENTS.md` Versioning / task facts:** 0.11.0 is unreleased and already a minor bump over 0.10.9. The entry goes under `## [0.11.0] — unreleased`, prefixed **BREAKING**. **Do not bump any manifest.** (Corrects the policy verifier, who asked for a minor bump.)
- **Root `AGENTS.md` "Error Messages":** messages live in the C++/C API layer. Both messages here are owned by the C API (`convert_params`), so they change there. No binding crafts them. No binding can trigger them either: every binding marshals only the four known type tags and never a NULL string pointer.
- **`bindings/dart/AGENTS.md`, "The checked-in `bindings.dart` predates the pinned ffigen (20.1.1)":** regenerating turns `quiver_data_type_t` / `quiver_error_t` / `quiver_log_level_t` into Dart enums and breaks hub. **Hand-edit `bindings.dart` in its existing style. Do not run `bindings/dart/generator/generator.bat` or `scripts/generator.bat`** (the latter runs all three generators). (Corrects the original proposal and the policy verifier, who said "regenerate Julia/Dart".)
- **`bindings/julia/AGENTS.md`:** "`src/c_api.jl` GENERATED low-level FFI module (do not hand-edit; regenerate)". Run `bindings/julia/generator/generator.bat` only. Also: "Always `GC.@preserve`: refs produced by `marshal_params` ... must stay inside a `GC.@preserve refs ...` block spanning the ccall". Keep that wrapper.
- **`bindings/python/AGENTS.md`:** `_c_api.py` is "Hand-written CFFI cdef declarations (kept in sync manually)". Hand-edit.
- **`bindings/js/AGENTS.md`:** "No generator — when the C API changes, add the symbol to `src/loader.ts` by hand". The Bun FFI gotchas are load-bearing: pass TypedArrays (not `ptr()` numbers) as pointer args. "Bun turns `null` into a NULL pointer for a `"pointer"` slot" (the idiom `group-columns.ts` and `updateRelation` already use).
  - Verified with Bun 1.3.14: `ptr(new Uint8Array(0))` does **not** throw. It *returns* a `TypeError` object ("ArrayBufferView must have a length > 0. A pointer to empty memory doesn't work"). So an empty list must never reach the buffer path. That is why JS passes `null, null, 0n`.
- **Root `AGENTS.md` "Do Not Fix":** "Collapsing per-method FFI boilerplate in Dart/Python into closure-parameterized helpers" is rejected. This plan adds no helper. It deletes a redundant C entry point and a branch per method, and each query method keeps its own expanded FFI call.
- **Root `AGENTS.md` "Self-Updating":** update `src/c/AGENTS.md` (file map + "Parameterized Queries" section), `bindings/dart/AGENTS.md` (hand-edit record + query shape), `bindings/js/AGENTS.md` (the NULL idiom for no parameters).
- **C++ core and Lua are unchanged.** C++ already has one method per type, and Lua binds C++ directly (`src/lua_runner.cpp`), so neither sees the C API.

Alternatives considered and rejected:
- *Keep the `_params` suffix and delete only the plain forms.* The C++→C prefix rule would still fail, and the cross-layer table would stay wrong.
- *Keep both C forms and only align the four bindings' branch conditions.* Leaves the duplicated C bodies and the naming mismatch.
- *Drop the `convert_params` rewording from this change* (policy verifier's point 2). Overruled by the maintainer note.
- *Fold the three C bodies into one template.* Adds an abstraction for three short bodies, and the C API's house style is one plain function per entry point.
- *Put the "no parameters" branch at each call site in each binding.* That keeps a branch per method. The marshal helper handles it once instead (policy verifier's point 1).
- *Change the Dart signature to `[List<Object?> parameters = const []]` or the Python one to `parameters: list = []`.* Changes a public signature (an explicit `null`/`None` would stop working). The maintainer said binding APIs stay unchanged.

## Changes

Do the steps in this order: C API (1-2), C API tests (3-4) and build, then FFI declarations (5-8), then binding wrappers (9-12), then docs (see "Docs and changelog").

### 1. `include/quiver/c/database.h`: one declaration per type

Anchor: the block starting at the comment `// Query methods - execute SQL and return first row's first column` (currently ~L557) and ending with the `quiver_database_query_float_params` declaration (~L599), right before `// Schema inspection — human-readable text reports.`

Current (the whole block):

```c
// Query methods - execute SQL and return first row's first column
QUIVER_C_API quiver_error_t quiver_database_query_string(quiver_database_t* db,
                                                         const char* sql,
                                                         char** out_value,
                                                         int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_integer(quiver_database_t* db,
                                                          const char* sql,
                                                          int64_t* out_value,
                                                          int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_float(quiver_database_t* db,
                                                        const char* sql,
                                                        double* out_value,
                                                        int* out_has_value);

// Parameterized query methods
// param_types[i]: QUIVER_DATA_TYPE_INTEGER (0), QUIVER_DATA_TYPE_FLOAT (1),
//                 QUIVER_DATA_TYPE_STRING (2), QUIVER_DATA_TYPE_NULL (4)
// param_values[i]: pointer to int64_t, double, const char*, or NULL
QUIVER_C_API quiver_error_t quiver_database_query_string_params(quiver_database_t* db,
... (three _params declarations)
                                                               int* out_has_value);
```

New (replaces the whole block):

```c
// Query methods - execute SQL and return the first row's first column.
// Parameters bind positionally to `?` placeholders. A query without parameters passes
// (NULL, NULL, 0): the arrays are only read when param_count > 0.
// param_types[i]: QUIVER_DATA_TYPE_INTEGER (0), QUIVER_DATA_TYPE_FLOAT (1),
//                 QUIVER_DATA_TYPE_STRING (2), QUIVER_DATA_TYPE_NULL (4)
// param_values[i]: pointer to int64_t, double, const char*, or NULL
QUIVER_C_API quiver_error_t quiver_database_query_string(quiver_database_t* db,
                                                         const char* sql,
                                                         const int* param_types,
                                                         const void* const* param_values,
                                                         size_t param_count,
                                                         char** out_value,
                                                         int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_integer(quiver_database_t* db,
                                                          const char* sql,
                                                          const int* param_types,
                                                          const void* const* param_values,
                                                          size_t param_count,
                                                          int64_t* out_value,
                                                          int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_float(quiver_database_t* db,
                                                        const char* sql,
                                                        const int* param_types,
                                                        const void* const* param_values,
                                                        size_t param_count,
                                                        double* out_value,
                                                        int* out_has_value);
```

Why: one C function per C++ method, named by the prefix rule. The argument order is exactly the old `_params` order, so every renamed caller only drops the suffix.

### 2. `src/c/database_query.cpp`: delete the plain bodies, rename the `_params` bodies, thread the caller name

Replace the whole file with:

```cpp
#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"

#include <stdexcept>
#include <string>
#include <vector>

// Converts the C parameter arrays to std::vector<Value>. `caller` is the C++ method the C function
// forwards to, so a Pattern 1 error names the operation the user called (the same convention as
// unmarshal_group_columns_to_rows).
static std::vector<quiver::Value>
convert_params(const char* caller, const int* param_types, const void* const* param_values, size_t param_count) {
    std::vector<quiver::Value> parameters;
    parameters.reserve(param_count);
    for (size_t i = 0; i < param_count; ++i) {
        switch (param_types[i]) {
        case QUIVER_DATA_TYPE_INTEGER:
            parameters.emplace_back(*static_cast<const int64_t*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_FLOAT:
            parameters.emplace_back(*static_cast<const double*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_STRING:
            if (!param_values[i]) {
                throw std::runtime_error(std::string("Cannot ") + caller + ": parameter at index " +
                                         std::to_string(i) + " has null string value");
            }
            parameters.emplace_back(std::string(static_cast<const char*>(param_values[i])));
            break;
        case QUIVER_DATA_TYPE_NULL:
            parameters.emplace_back(nullptr);
            break;
        default:
            throw std::runtime_error(std::string("Cannot ") + caller + ": unknown parameter type " +
                                     std::to_string(param_types[i]));
        }
    }
    return parameters;
}

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_query_string(quiver_database_t* db,
                                                         const char* sql,
                                                         const int* param_types,
                                                         const void* const* param_values,
                                                         size_t param_count,
                                                         char** out_value,
                                                         int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);
    if (param_count > 0) {
        QUIVER_REQUIRE(param_types, param_values);
    }

    try {
        auto parameters = convert_params("query_string", param_types, param_values, param_count);
        auto result = db->db.query_string(sql, parameters);
        if (result.has_value()) {
            *out_value = quiver::string::new_c_str(*result);
            *out_has_value = 1;
        } else {
            *out_value = nullptr;
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_integer(quiver_database_t* db,
                                                          const char* sql,
                                                          const int* param_types,
                                                          const void* const* param_values,
                                                          size_t param_count,
                                                          int64_t* out_value,
                                                          int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);
    if (param_count > 0) {
        QUIVER_REQUIRE(param_types, param_values);
    }

    try {
        auto parameters = convert_params("query_integer", param_types, param_values, param_count);
        auto result = db->db.query_integer(sql, parameters);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_float(quiver_database_t* db,
                                                        const char* sql,
                                                        const int* param_types,
                                                        const void* const* param_values,
                                                        size_t param_count,
                                                        double* out_value,
                                                        int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);
    if (param_count > 0) {
        QUIVER_REQUIRE(param_types, param_values);
    }

    try {
        auto parameters = convert_params("query_float", param_types, param_values, param_count);
        auto result = db->db.query_float(sql, parameters);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
```

Why: the bodies are the old `_params` bodies unchanged (also unifies the blank line the old string/float bodies lacked before `try`), only renamed. `convert_params` takes the C++ method name, so the messages become `Cannot query_string: ...`, `Cannot query_integer: ...`, `Cannot query_float: ...`.

### 3. `tests/test_c_api_database_query.cpp`: migrate every call

**3a. The twelve plain-form tests.** In each, insert `nullptr, nullptr, 0,` between the `sql` argument and the first out-param. Keep everything else, including the `delete[] value;` lines (plan 69 owns those). clang-format (step "Verification") reflows the argument lists.

| Test (current line) | Current call | New call |
| --- | --- | --- |
| `QueryStringReturnsValue` (~L28) | `quiver_database_query_string(db, "SELECT string_attribute FROM Configuration WHERE label = 'Test Label'", &value, &has_value)` | `quiver_database_query_string(db, "SELECT string_attribute FROM Configuration WHERE label = 'Test Label'", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryStringReturnsNoValueWhenEmpty` (~L48) | `quiver_database_query_string(db, "SELECT string_attribute FROM Configuration WHERE 1 = 0", &value, &has_value)` | `quiver_database_query_string(db, "SELECT string_attribute FROM Configuration WHERE 1 = 0", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryStringNullDb` (~L60) | `quiver_database_query_string(nullptr, "SELECT 1", &value, &has_value)` | `quiver_database_query_string(nullptr, "SELECT 1", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryStringNullSql` (~L72) | `quiver_database_query_string(db, nullptr, &value, &has_value)` | `quiver_database_query_string(db, nullptr, nullptr, nullptr, 0, &value, &has_value)` |
| `QueryIntegerReturnsValue` (~L98) | `quiver_database_query_integer(db, "SELECT integer_attribute FROM Configuration WHERE label = 'Test'", &value, &has_value)` | `... WHERE label = 'Test'", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryIntegerReturnsNoValueWhenEmpty` (~L116) | `quiver_database_query_integer(db, "SELECT integer_attribute FROM Configuration WHERE 1 = 0", &value, &has_value)` | `... WHERE 1 = 0", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryIntegerCount` (~L147) | `quiver_database_query_integer(db, "SELECT COUNT(*) FROM Configuration", &value, &has_value)` | `quiver_database_query_integer(db, "SELECT COUNT(*) FROM Configuration", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryIntegerNullDb` (~L159) | `quiver_database_query_integer(nullptr, "SELECT 1", &value, &has_value)` | `quiver_database_query_integer(nullptr, "SELECT 1", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryFloatReturnsValue` (~L183) | `quiver_database_query_float(db, "SELECT float_attribute FROM Configuration WHERE label = 'Test'", &value, &has_value)` | `... WHERE label = 'Test'", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryFloatReturnsNoValueWhenEmpty` (~L202) | `quiver_database_query_float(db, "SELECT float_attribute FROM Configuration WHERE 1 = 0", &value, &has_value)` | `... WHERE 1 = 0", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryFloatAverage` (~L234) | `quiver_database_query_float(db, "SELECT AVG(float_attribute) FROM Configuration", &value, &has_value)` | `quiver_database_query_float(db, "SELECT AVG(float_attribute) FROM Configuration", nullptr, nullptr, 0, &value, &has_value)` |
| `QueryFloatNullDb` (~L246) | `quiver_database_query_float(nullptr, "SELECT 1.0", &value, &has_value)` | `quiver_database_query_float(nullptr, "SELECT 1.0", nullptr, nullptr, 0, &value, &has_value)` |

**3b. Drop the `_params` suffix** in every remaining call, arguments unchanged: `QueryStringWithParams` (~L274), `QueryIntegerWithParams` (~L310), `QueryFloatWithParams` (~L345), `QueryWithIntegerParam` (~L381), `QueryWithNullParam` (~L414), `QueryParamsNoMatch` (~L443), `QueryParamsNullStringElement` (~L471), `QueryParamsNullArraysWithCount` (~L487), `QueryParameterCountMismatch` (~L536). Example, `QueryParameterCountMismatch`:

```cpp
    auto err = quiver_database_query_string_params(
        db, "SELECT label FROM Configuration WHERE id = ?", nullptr, nullptr, 0, &value, &has_value);
```
→
```cpp
    auto err = quiver_database_query_string(
        db, "SELECT label FROM Configuration WHERE id = ?", nullptr, nullptr, 0, &value, &has_value);
```

**3c. Delete `QueryParamsNullDb`** (~L453-458). After 3a it is byte-for-byte the same call as `QueryIntegerNullDb`:

```cpp
TEST(DatabaseCApiQuery, QueryParamsNullDb) {
    int64_t value = 0;
    int has_value = 0;
    auto err = quiver_database_query_integer_params(nullptr, "SELECT 1", nullptr, nullptr, 0, &value, &has_value);
    EXPECT_EQ(err, QUIVER_ERROR);
}
```

**3d. `QueryParamsNullStringElement`** (~L460): pin the full message. Current tail:

```cpp
    EXPECT_EQ(err, QUIVER_ERROR);

    quiver_database_close(db);
```

New tail:

```cpp
    EXPECT_EQ(err, QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot query_string: parameter at index 0 has null string value");

    quiver_database_close(db);
```

**3e. Replace `QueryParamsUnknownType`** (~L501-519) with a test that covers all three entry points. Its old assertion was `EXPECT_NE(msg.find("unknown parameter type"), std::string::npos)` on the integer form only. New test, in the same position:

```cpp
TEST(DatabaseCApiQuery, QueryParamsUnknownTypeNamesTheCalledFunction) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int param_types[] = {999};  // bogus type
    int64_t dummy = 0;
    const void* param_values[] = {&dummy};
    int has_value = 0;

    char* string_value = nullptr;
    EXPECT_EQ(quiver_database_query_string(db, "SELECT ?", param_types, param_values, 1, &string_value, &has_value),
              QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot query_string: unknown parameter type 999");

    int64_t integer_value = 0;
    EXPECT_EQ(
        quiver_database_query_integer(db, "SELECT ?", param_types, param_values, 1, &integer_value, &has_value),
        QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot query_integer: unknown parameter type 999");

    double float_value = 0.0;
    EXPECT_EQ(quiver_database_query_float(db, "SELECT ?", param_types, param_values, 1, &float_value, &has_value),
              QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot query_float: unknown parameter type 999");

    quiver_database_close(db);
}
```

`convert_params` throws before anything is written to the out-params, so `string_value` stays `nullptr` and nothing leaks.

### 4. `tests/test_c_api_database_csv_import.cpp`: rename the one other caller

In `TEST(DatabaseCApiCSV, ImportCSV_Scalar_SelfReferenceFK_ReImport)`, lambda `query_int_by_label` (currently ~L526):

```cpp
        EXPECT_EQ(quiver_database_query_integer_params(db, sql, &param_type, &param_val, 1, &out, &has), QUIVER_OK);
```
→
```cpp
        EXPECT_EQ(quiver_database_query_integer(db, sql, &param_type, &param_val, 1, &out, &has), QUIVER_OK);
```

Now build (`cmake --build build --config Debug`) and run the C API filter from "Verification" before touching the bindings.

### 5. Julia FFI: `bindings/julia/src/c_api.jl` (generated)

Run from the repo root:

```
bindings/julia/generator/generator.bat
```

(It runs `julia +1.12.5 --project=<generator dir> generator.jl`.) Expected diff, and nothing else:

- The three 4-argument wrappers `quiver_database_query_string(db, sql, out_value, out_has_value)`, `quiver_database_query_integer(...)` and `quiver_database_query_float(...)` (currently ~L484-494) are removed.
- The three `_params` wrappers (~L496-506) are renamed. The first becomes:

```julia
function quiver_database_query_string(db, sql, param_types, param_values, param_count, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_query_string(db::Ptr{quiver_database_t}, sql::Ptr{Cchar}, param_types::Ptr{Cint}, param_values::Ptr{Ptr{Cvoid}}, param_count::Csize_t, out_value::Ptr{Ptr{Cchar}}, out_has_value::Ptr{Cint})::quiver_error_t
end
```

If `git diff bindings/julia/src/c_api.jl` shows hunks for other functions, an earlier plan changed a header without regenerating. Keep those hunks (the generated file must match the headers) and say so in the PR, do not hand-revert them.

### 6. Dart FFI: `bindings/dart/lib/src/ffi/bindings.dart` (hand-edit, do NOT run ffigen)

Anchor: after the `quiver_database_import_csv` block (its `_quiver_database_import_csv = ... .asFunction<...>();`, currently ~L2833) and before `int quiver_database_describe(`.

**6a.** Delete the three plain blocks (currently ~L2835-2932), from

```dart
  int quiver_database_query_string(
    ffi.Pointer<quiver_database_t> db,
    ffi.Pointer<ffi.Char> sql,
    ffi.Pointer<ffi.Pointer<ffi.Char>> out_value,
    ffi.Pointer<ffi.Int> out_has_value,
  ) {
```

through the end of the plain float block:

```dart
  late final _quiver_database_query_float = _quiver_database_query_floatPtr
      .asFunction<
        int Function(
          ffi.Pointer<quiver_database_t>,
          ffi.Pointer<ffi.Char>,
          ffi.Pointer<ffi.Double>,
          ffi.Pointer<ffi.Int>,
        )
      >();
```

**6b.** In the three `_params` blocks that follow (currently ~L2940-3077), drop `_params` from every name: the public method name, the call to the private member, the `Ptr` member, the `_lookup` string and the private member. Nothing else changes. The resulting string block is:

```dart
  int quiver_database_query_string(
    ffi.Pointer<quiver_database_t> db,
    ffi.Pointer<ffi.Char> sql,
    ffi.Pointer<ffi.Int> param_types,
    ffi.Pointer<ffi.Pointer<ffi.Void>> param_values,
    int param_count,
    ffi.Pointer<ffi.Pointer<ffi.Char>> out_value,
    ffi.Pointer<ffi.Int> out_has_value,
  ) {
    return _quiver_database_query_string(
      db,
      sql,
      param_types,
      param_values,
      param_count,
      out_value,
      out_has_value,
    );
  }

  late final _quiver_database_query_stringPtr =
      _lookup<
        ffi.NativeFunction<
          ffi.Int32 Function(
            ffi.Pointer<quiver_database_t>,
            ffi.Pointer<ffi.Char>,
            ffi.Pointer<ffi.Int>,
            ffi.Pointer<ffi.Pointer<ffi.Void>>,
            ffi.Size,
            ffi.Pointer<ffi.Pointer<ffi.Char>>,
            ffi.Pointer<ffi.Int>,
          )
        >
      >('quiver_database_query_string');
  late final _quiver_database_query_string = _quiver_database_query_stringPtr
      .asFunction<
        int Function(
          ffi.Pointer<quiver_database_t>,
          ffi.Pointer<ffi.Char>,
          ffi.Pointer<ffi.Int>,
          ffi.Pointer<ffi.Pointer<ffi.Void>>,
          int,
          ffi.Pointer<ffi.Pointer<ffi.Char>>,
          ffi.Pointer<ffi.Int>,
        )
      >();
```

The integer and float blocks are the same edit: `quiver_database_query_integer_params` → `quiver_database_query_integer` (out type `ffi.Pointer<ffi.Int64>`), `quiver_database_query_float_params` → `quiver_database_query_float` (out type `ffi.Pointer<ffi.Double>`). The `late final _x = _xPtr` lines then have the same length as the deleted plain ones, so `dart format` leaves them alone.

Check: `git grep -n "query_\(string\|integer\|float\)_params" -- bindings/dart` prints nothing.

### 7. Python cdef: `bindings/python/src/quiverdb/_c_api.py`

Anchor: inside `ffi.cdef("""...""")`, the two comment-headed groups after the `quiver_database_in_dry_run` declaration (currently ~L279-297).

Current:

```
    // Query methods - simple
    quiver_error_t quiver_database_query_string(quiver_database_t* db,
        const char* sql, char** out_value, int* out_has_value);
    quiver_error_t quiver_database_query_integer(quiver_database_t* db,
        const char* sql, int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_query_float(quiver_database_t* db,
        const char* sql, double* out_value, int* out_has_value);

    // Query methods - parameterized
    quiver_error_t quiver_database_query_string_params(quiver_database_t* db,
        const char* sql, const int* param_types, void**  param_values,
        size_t param_count, char** out_value, int* out_has_value);
    quiver_error_t quiver_database_query_integer_params(quiver_database_t* db,
        const char* sql, const int* param_types, void** param_values,
        size_t param_count, int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_query_float_params(quiver_database_t* db,
        const char* sql, const int* param_types, void** param_values,
        size_t param_count, double* out_value, int* out_has_value);
```

New:

```
    // Query methods - parameters bind to `?` placeholders; (NULL, NULL, 0) for none
    quiver_error_t quiver_database_query_string(quiver_database_t* db,
        const char* sql, const int* param_types, void** param_values,
        size_t param_count, char** out_value, int* out_has_value);
    quiver_error_t quiver_database_query_integer(quiver_database_t* db,
        const char* sql, const int* param_types, void** param_values,
        size_t param_count, int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_query_float(quiver_database_t* db,
        const char* sql, const int* param_types, void** param_values,
        size_t param_count, double* out_value, int* out_has_value);
```

Keep `void** param_values` (not the header's `const void* const*`). That is the existing declaration, and `_marshal_params` builds a `void*[]`. Only the double space in the old string declaration goes.

### 8. JS symbol table: `bindings/js/src/loader.ts`

`const querySymbols` (currently ~L92-99). Current:

```ts
const querySymbols = {
  quiver_database_query_string: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_query_integer: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_query_float: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_query_string_params: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
  quiver_database_query_integer_params: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
  quiver_database_query_float_params: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
} as const;
```

New:

```ts
const querySymbols = {
  quiver_database_query_string: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
  quiver_database_query_integer: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
  quiver_database_query_float: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
} as const;
```

The `_params` entries must go: `dlopen` fails when a listed symbol is not exported, so a stale entry breaks every JS test.

### 9. Julia wrapper: `bindings/julia/src/database_query.jl`

Keep `marshal_params` (~L1-39) unchanged. Replace everything after it (the ten query methods, ~L41-224) with these five definitions:

```julia
"""
    query_string(db::Database, sql::String, parameters::Vector = []) -> Optional{String}

Execute a SQL query and return the first column of the first row as a String.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_string(db::Database, sql::String, parameters::Vector = [])
    param_types, param_values, refs = marshal_params(parameters)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    GC.@preserve refs check(
        C.quiver_database_query_string(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(parameters),
            out_value,
            out_has_value,
        ),
    )

    if out_has_value[] == 0 || out_value[] == C_NULL
        return nothing
    end
    result = unsafe_string(out_value[])
    C.quiver_database_free_string(out_value[])
    return result
end

"""
    query_integer(db::Database, sql::String, parameters::Vector = []) -> Optional{Int64}

Execute a SQL query and return the first column of the first row as an Int64.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_integer(db::Database, sql::String, parameters::Vector = [])
    param_types, param_values, refs = marshal_params(parameters)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    GC.@preserve refs check(
        C.quiver_database_query_integer(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(parameters),
            out_value,
            out_has_value,
        ),
    )

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

"""
    query_boolean(db::Database, sql::String, parameters::Vector = []) -> Optional{Bool}

Execute a SQL query and return the first column of the first row as a Bool.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_boolean(db::Database, sql::String, parameters::Vector = [])
    return _integer_to_boolean(query_integer(db, sql, parameters))
end

"""
    query_float(db::Database, sql::String, parameters::Vector = []) -> Optional{Float64}

Execute a SQL query and return the first column of the first row as a Float64.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_float(db::Database, sql::String, parameters::Vector = [])
    param_types, param_values, refs = marshal_params(parameters)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    GC.@preserve refs check(
        C.quiver_database_query_float(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(parameters),
            out_value,
            out_has_value,
        ),
    )

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

"""
    query_date_time(db::Database, sql::String, parameters::Vector = []) -> Optional{DateTime}

Execute a SQL query and return the first column of the first row as a DateTime.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_date_time(db::Database, sql::String, parameters::Vector = [])
    return string_to_date_time(query_string(db, sql, parameters))
end
```

Why: a positional default still generates both the 2-arg and 3-arg methods, so every existing caller (`test_database_query.jl`, `test_database_boolean.jl`, `test_database_csv_import.jl`, `test_database_update.jl`) resolves unchanged. `[]` is `Any[]`, which matches `::Vector`, and `marshal_params(Any[])` returns empty `Cint`/`Ptr{Cvoid}` vectors that `@ccall` converts to pointers the C side never reads (count 0). Result: 10 methods → 5 definitions (the facts verifier's corrected count).

### 10. Dart wrapper: `bindings/dart/lib/src/database.dart` (`_marshalParams`) and `bindings/dart/lib/src/database_query.dart`

**10a. `_marshalParams`** in `database.dart` (currently ~L170-201). Current head and tail:

```dart
  ({Pointer<Int> types, Pointer<Pointer<Void>> values}) _marshalParams(
    Arena arena,
    List<Object?> parameters,
  ) {
    final types = arena<Int>(parameters.length);
    final values = arena<Pointer<Void>>(parameters.length);
...
    return (types: types, values: values);
  }
```

New (the `for` loop in between is unchanged):

```dart
  /// Marshals query parameters into the C API's parallel type/value arrays.
  /// No parameters (omitted or empty) marshal to NULL pointers and a count of 0:
  /// the C API reads neither array then, so nothing is allocated.
  ({Pointer<Int> types, Pointer<Pointer<Void>> values, int count}) _marshalParams(
    Arena arena,
    List<Object?>? parameters,
  ) {
    if (parameters == null || parameters.isEmpty) {
      return (types: nullptr, values: nullptr, count: 0);
    }

    final types = arena<Int>(parameters.length);
    final values = arena<Pointer<Void>>(parameters.length);
...
    return (types: types, values: values, count: parameters.length);
  }
```

Why the early return: today only an explicit `[]` reaches `arena<Int>(0)`. After this change every parameterless query would, and a zero-byte `calloc` is implementation-defined in C (package:ffi throws `ArgumentError` if it returns NULL). Returning `nullptr` removes that dependency, and matches JS's NULL idiom.

**10b. `database_query.dart`**: in `queryString`, `queryInteger` and `queryFloat`, replace the `if (parameters == null) { ...plain... } else { ...params... }` block with one call. `queryBoolean` and `queryDateTime` are unchanged. New `queryString`:

```dart
  String? queryString(String sql, [List<Object?>? parameters]) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Pointer<Char>>();
      final outHasValue = arena<Int>();
      final nativeParams = _marshalParams(arena, parameters);

      check(
        bindings.quiver_database_query_string(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          nativeParams.types,
          nativeParams.values,
          nativeParams.count,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0 || outValue.value == nullptr) {
        return null;
      }

      final result = outValue.value.cast<Utf8>().toDartString();
      bindings.quiver_database_free_string(outValue.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }
```

New `queryInteger`:

```dart
  int? queryInteger(String sql, [List<Object?>? parameters]) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Int64>();
      final outHasValue = arena<Int>();
      final nativeParams = _marshalParams(arena, parameters);

      check(
        bindings.quiver_database_query_integer(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          nativeParams.types,
          nativeParams.values,
          nativeParams.count,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0) {
        return null;
      }

      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }
```

New `queryFloat`: identical to `queryInteger` with `arena<Double>()` and `bindings.quiver_database_query_float`, returning `double?`. Keep each method's existing `///` doc comment.

### 11. Python wrapper: `bindings/python/src/quiverdb/database.py`

`_marshal_params` (currently ~L2128-2163) is unchanged. `ffi.new("int[]", 0)` / `ffi.new("void*[]", 0)` are valid zero-length cdata arrays, and the C API does not read them at count 0.

Replace the `if parameters is not None and len(parameters) > 0: ... else: ...` block in `query_string` (~L429-450), `query_integer` (~L466-487) and `query_float` (~L504-525). Signatures and docstrings stay as they are. New `query_string`:

```python
    def query_string(self, sql: str, *, parameters: list | None = None) -> str | None:
        """Execute SQL and return the first column of the first row as str, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("char**")
        out_has = ffi.new("int*")
        parameters = parameters or []
        keepalive, c_types, c_values = _marshal_params(parameters)
        check(
            lib.quiver_database_query_string(
                self._ptr,
                sql.encode("utf-8"),
                c_types,
                c_values,
                len(parameters),
                out_value,
                out_has,
            )
        )

        if out_has[0] == 0 or out_value[0] == ffi.NULL:
            return None
        try:
            return ffi.string(out_value[0]).decode("utf-8")
        finally:
            lib.quiver_database_free_string(out_value[0])
```

New `query_integer`:

```python
    def query_integer(self, sql: str, *, parameters: list | None = None) -> int | None:
        """Execute SQL and return the first column of the first row as int, or None."""
        self._ensure_open()
        lib = get_lib()
        out_value = ffi.new("int64_t*")
        out_has = ffi.new("int*")
        parameters = parameters or []
        keepalive, c_types, c_values = _marshal_params(parameters)
        check(
            lib.quiver_database_query_integer(
                self._ptr,
                sql.encode("utf-8"),
                c_types,
                c_values,
                len(parameters),
                out_value,
                out_has,
            )
        )

        if out_has[0] == 0:
            return None
        return out_value[0]
```

New `query_float`: identical to `query_integer` with `ffi.new("double*")`, `lib.quiver_database_query_float`, return type `float | None` and its current docstring. `query_boolean` and `query_date_time` are unchanged. `keepalive` must stay bound for the call. Keep the tuple unpacking as it is today; ruff's F841 does not flag tuple unpacking.

### 12. JS wrapper: `bindings/js/src/query.ts`

**12a.** Delete the first import line `import { ptr } from "bun:ffi";`. `marshalParams` was its only user.

**12b. `marshalParams`** (currently ~L25-73). Replace its signature, add the early return, and replace its last four lines. The `for` loop is unchanged. Current head and tail:

```ts
function marshalParams(parameters: QueryParam[]): {
  types: Allocation;
  values: Allocation;
  _keepalive: Allocation[];
} {
  const n = parameters.length;
  const typesBuf = new Uint8Array(n * 4);
...
  const types: Allocation = { ptr: ptr(typesBuf), buf: typesBuf };
  const values: Allocation = { ptr: ptr(valuesBuf), buf: valuesBuf };
  keepalive.push(types, values);
  return { types, values, _keepalive: keepalive };
}
```

New:

```ts
// Marshals query parameters into the C API's parallel type/value arrays. No parameters marshal to
// NULL pointers with a count of 0: the C API reads neither array then, and Bun's ptr() returns a
// TypeError instead of a pointer for a zero-length buffer.
function marshalParams(parameters: QueryParam[] = []): {
  types: Uint8Array | null;
  values: Uint8Array | null;
  count: bigint;
  _keepalive: Allocation[];
} {
  const n = parameters.length;
  if (n === 0) return { types: null, values: null, count: 0n, _keepalive: [] };

  const typesBuf = new Uint8Array(n * 4);
...
  return { types: typesBuf, values: valuesBuf, count: BigInt(n), _keepalive: keepalive };
}
```

Why: the call sites already passed the TypedArrays (`m.types.buf`), never the `ptr()` numbers, which is the house style in `bindings/js/AGENTS.md`. The two `ptr()` wrappers were dead. The returned object keeps `typesBuf`/`valuesBuf` alive, and `_keepalive` still holds the native int/float/string allocations the value slots point at.

**12c.** `queryString`, `queryInteger`, `queryFloat`: replace the `if (parameters && parameters.length > 0) { ... } else { ... }` block with one call. `queryBoolean` is unchanged. New bodies:

```ts
Database.prototype.queryString = function (
  this: Database,
  sql: string,
  parameters?: QueryParam[],
): string | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = allocPtrOut();
  const outHasValue = new Uint8Array(4);
  const params = marshalParams(parameters);

  check(
    lib.quiver_database_query_string(
      this._handle,
      sqlBuf.buf,
      params.types,
      params.values,
      params.count,
      outValue.buf,
      outHasValue,
    ),
  );

  if (new DataView(outHasValue.buffer).getInt32(0, true) === 0) return null;
  const result = decodeStringFromBuf(outValue);
  lib.quiver_database_free_string(readPtrOut(outValue));
  return result;
};

Database.prototype.queryInteger = function (
  this: Database,
  sql: string,
  parameters?: QueryParam[],
): number | null {
  const lib = getSymbols();
  const sqlBuf = toCString(sql);
  const outValue = new Uint8Array(8);
  const outHasValue = new Uint8Array(4);
  const params = marshalParams(parameters);

  check(
    lib.quiver_database_query_integer(
      this._handle,
      sqlBuf.buf,
      params.types,
      params.values,
      params.count,
      outValue,
      outHasValue,
    ),
  );

  if (new DataView(outHasValue.buffer).getInt32(0, true) === 0) return null;
  return Number(new DataView(outValue.buffer).getBigInt64(0, true));
};
```

`queryFloat` is the same as `queryInteger` with `lib.quiver_database_query_float` and `return new DataView(outValue.buffer).getFloat64(0, true);`. The DataViews over the out buffers are still created only after the call (Bun gotcha).

### Lua and C++ core

No change. `src/lua_runner.cpp` calls `Database::query_*` directly, and the C++ methods already have one signature each.

## Tests

**C API** (`tests/test_c_api_database_query.cpp`, `tests/test_c_api_database_csv_import.cpp`, schema `tests/schemas/valid/basic.sql` / `relations.sql`, no new schemas). All in steps 3-4:
- The 12 migrated plain-form tests now prove the single entry point accepts `(nullptr, nullptr, 0)` for all three types, including the NULL `db` / NULL `sql` checks.
- `QueryParamsNullStringElement` gains `EXPECT_STREQ(quiver_get_last_error(), "Cannot query_string: parameter at index 0 has null string value");`.
- `QueryParamsUnknownTypeNamesTheCalledFunction` replaces `QueryParamsUnknownType` and pins all three names.
- `QueryParamsNullDb` is deleted (a duplicate of `QueryIntegerNullDb` after migration).
- **Fail before the fix:** with only the rename applied and `convert_params` unchanged, both `EXPECT_STREQ` assertions fail (`Actual: "Cannot query: ..."`). Against HEAD the file does not compile (the plain functions take 4 arguments), which is the ABI break itself.
- Suite count: `DatabaseCApiQuery` goes from 23 tests to 22.

**C++ core / Lua:** nothing changes, so no test changes. `quiver_tests.exe` must still pass.

**Julia** (`bindings/julia/test/test_database_query.jl`, testset `"Parameter Count Mismatch"`, currently ~L274-289). After the "Too many parameters" assertion (~L283-284), add:

```julia
        # Parameters omitted entirely (the 2-arg method passes an empty list)
        @test_throws Quiver.DatabaseException Quiver.query_string(db, "SELECT label FROM Configuration WHERE id = ?")
```

This pins that the default `parameters::Vector = []` reaches the core's count check. The existing 2-arg and 3-arg tests (~L19-242) and `test_database_boolean.jl` cover the success paths of all five names unchanged.

**Python** (`bindings/python/tests/test_database_query.py`):
- In `TestQueryParameterCount.test_parameter_count_mismatch_raises` (~L179), after the `parameters=[1, 2]` block, add:

```python
        # Parameters omitted entirely
        with pytest.raises(QuiverError):
            db.query_string("SELECT label FROM Configuration WHERE id = ?")
```

- Rename `TestQueryEmptyParams.test_query_with_empty_params_routes_to_simple` (~L116) to `test_query_with_empty_params_returns_value`. The "simple" form it names no longer exists. Body unchanged.

**Dart** (`bindings/dart/test/database_query_test.dart`, group `'Query Parameter Count'`, ~L387). After the "Too many parameters" `expect` (~L403-406), add:

```dart
        // Parameters omitted entirely
        expect(
          () => db.queryString('SELECT label FROM Configuration WHERE id = ?'),
          throwsA(isA<DatabaseException>()),
        );
```

This one exercises the new `nullptr` early return in `_marshalParams`, and so does every existing parameterless query test (`'Query String'`, `'Query Integer'`, `'Query Float'`, `'Query DateTime'` groups).

**JS** (`bindings/js/test/database-query.test.ts`, `describe("query parameter count")`, ~L131). After the "Too many parameters" `expect` (~L140-142), add:

```ts
      // Parameters omitted entirely
      expect(() => db.queryString("SELECT label FROM AllTypes WHERE some_integer = ?")).toThrow();
```

The existing `[]` case (~L136-138) now goes through the `null, null, 0n` path. Before this change it took the plain C function.

None of the binding additions fail before the change: they pin that the collapsed default path still reaches the core's count check (`Failed to execute statement: expected 1 bound parameter(s) but got 0`). The regression they guard is a wrongly wired NULL/0 call.

## Docs and changelog

**`src/c/AGENTS.md`** (CRLF in the working tree).

1. File map (currently ~L32). Old:
   ```
     database_query.cpp      # Query operations (plain and parameterized)
   ```
   New:
   ```
     database_query.cpp      # Query operations: one C function per C++ query_* method
   ```
2. Replace the whole `## Parameterized Queries` section at the end of the file (~L220-227). Old:
   ````
   ## Parameterized Queries

   `_params` variants use parallel arrays for typed parameters:
   ```c
   // param_types[i]: QUIVER_DATA_TYPE_INTEGER(0), FLOAT(1), STRING(2), NULL(4)
   // param_values[i]: pointer to int64_t, double, const char*, or NULL
   quiver_database_query_string_params(db, sql, param_types, param_values, param_count, &out, &has);
   ```
   ````
   New:
   ````
   ## Queries

   One C function per C++ `query_*` method, so the names follow the prefix rule. Each takes parallel
   arrays for its positional `?` parameters. A query without parameters passes `(NULL, NULL, 0)`,
   since the arrays are only required when `param_count > 0`. There is no separate no-parameter form,
   so every binding makes the same call whether or not it has parameters:
   ```c
   // param_types[i]: QUIVER_DATA_TYPE_INTEGER(0), FLOAT(1), STRING(2), NULL(4)
   // param_values[i]: pointer to int64_t, double, const char*, or NULL
   quiver_database_query_string(db, sql, param_types, param_values, param_count, &out, &has);
   quiver_database_query_integer(db, "SELECT COUNT(*) FROM Items", NULL, NULL, 0, &count, &has);
   ```
   `convert_params(caller, ...)` takes the C++ method name, like `unmarshal_group_columns_to_rows`,
   so its Pattern 1 errors read `Cannot query_string: unknown parameter type 999`.
   ````
   The sentence at ~L203 ("This pattern mirrors the `convert_params()` approach from `database_query.cpp`...") stays true, leave it.

**`bindings/dart/AGENTS.md`** (CRLF).

1. In the bullet "**The checked-in `bindings.dart` predates the pinned ffigen (20.1.1).**", find the list of hand edits. It currently ends with "`quiver_database_update_relation` plus its `_by_label` form." Plan 21 may have added a "Removals are hand-deleted the same way (...)" sentence after it. Add this sentence at the end of whatever is there, before "Take the generator upgrade as its own deliberate change":
   ```
     The query entry points were collapsed the same way: the three plain
     `quiver_database_query_{string,integer,float}` blocks were deleted and the parameterized
     blocks renamed onto those names.
   ```
2. The "**Query API shape**" bullet (~L105). Old:
   ```
   - **Query API shape**: `queryString`/`queryInteger`/`queryBoolean`/`queryFloat`/`queryDateTime`
     take an optional positional `List<Object?>? parameters` (no separate `*Params` methods).
   ```
   New:
   ```
   - **Query API shape**: `queryString`/`queryInteger`/`queryBoolean`/`queryFloat`/`queryDateTime`
     take an optional positional `List<Object?>? parameters` (no separate `*Params` methods). Every
     call makes the one C call per type; `_marshalParams` returns `nullptr` arrays and a count of 0
     when `parameters` is null or empty, so a parameterless query allocates nothing.
   ```

**`bindings/js/AGENTS.md`** (CRLF). Right after the bullet that starts "**A nullable scalar string argument passes literal `null`, never `""`**", add:

```
- **No query parameters pass `null, null, 0n`** (`marshalParams` in `src/query.ts`). The C API
  reads neither array at count 0, and Bun's `ptr()` returns a `TypeError` object (it does not throw)
  for a zero-length buffer, so an empty or omitted list never builds a buffer at all.
```

**No other AGENTS.md changes.** Root `AGENTS.md` already maps `query_string()` → `quiver_database_query_string()` in the cross-layer table and describes the C++ `query_*(sql, parameters = {})` surface. After this change both are true. `bindings/julia/AGENTS.md` (the `GC.@preserve` rule still holds), `bindings/python/AGENTS.md`, `src/AGENTS.md` and `tests/AGENTS.md` name neither the plain nor the `_params` symbols (verified by grep).

**No other docs.** `docs/*.md`, the READMEs (`bindings/js/README.md` documents `queryString(sql, parameters?)`, which is unchanged) and `bindings/js/src/lua-api.ts` do not mention the C symbols.

**`CHANGELOG.md`** (CRLF). Under `## [0.11.0] — unreleased` → `### Changed`, append at the end of the `### Changed` list, after the last bullet, which is currently the `export_csv()` quoting entry ending "*Adapt:* regenerate golden files and any byte-for-byte comparisons over exported CSVs.", and before `### Removed` or `### Fixed`, whichever comes first:

```
- **BREAKING — C API: one `quiver_database_query_*` function per type.** `quiver_database_query_string`,
  `quiver_database_query_integer` and `quiver_database_query_float` now take the parameter arrays
  (`param_types`, `param_values`, `param_count`) that the `quiver_database_query_*_params` forms
  took, and those three `_params` functions are gone, so each C++ `query_*` method maps to exactly
  one C function. A parameter the C API cannot convert now names the function called
  (`Cannot query_integer: unknown parameter type 999`) instead of `Cannot query: …`. The Julia,
  Dart, Python and JS query methods are unchanged.

  *Adapt:* in direct C API calls, pass `NULL, NULL, 0` after `sql` for a query without parameters,
  and drop the `_params` suffix from a parameterized call.
```

## Verification

From the repo root (`C:\Development\Quiver\quiver1`), in order:

1. `cmake --build build --config Debug`. Expected: builds with no errors.
2. `./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApiQuery.*:DatabaseCApiCSV.ImportCSV_Scalar_SelfReferenceFK_ReImport'`. Expected: 23 tests from 2 suites pass (22 in `DatabaseCApiQuery`), including `QueryParamsNullStringElement`, `QueryParamsUnknownTypeNamesTheCalledFunction` and `QueryParameterCountMismatch`. `QueryParamsNullDb` and `QueryParamsUnknownType` no longer exist.
3. `./build/bin/quiver_c_tests.exe` and `./build/bin/quiver_tests.exe`. Expected: all pass.
4. `git grep -n "query_\(string\|integer\|float\)_params" -- . ':!build'`. Expected: no output. The CHANGELOG spells the removed names `quiver_database_query_*_params`, which this pattern does not match.
5. `bindings/julia/generator/generator.bat`, then `git diff -- bindings/julia/src/c_api.jl`. Expected: only the six hunks described in step 5.
6. `bindings/julia/test/test.bat`. Expected: all pass, including `"Parameter Count Mismatch"`.
7. Dart: first delete the stale native cache so the hook rebuilds the C library: `rm -rf bindings/dart/.dart_tool/hooks_runner bindings/dart/.dart_tool/lib`. Then `cd bindings/dart && dart analyze` (expected: no new issues) and `bindings/dart/test/test.bat` (expected: all pass, including `'Query Parameter Count'`).
8. `bindings/python/tests/test.bat`. Expected: all pass, including `test_parameter_count_mismatch_raises` and `test_query_with_empty_params_returns_value`.
9. `bindings/js/test/test.bat`, then `cd bindings/js && bun run lint`. Expected: tests pass, including `query parameter count`. No new lint errors in `src/query.ts` / `src/loader.ts` (in particular no unused `ptr` import).
10. `scripts/format.bat`. Expected: it may reflow the C API test calls (clang-format) and nothing else. Re-run step 2 if it touched test files.
11. `scripts/test-all.bat`. Expected: C++, C API, Julia, Dart, JS and Python PASS. The CLI smoke step fails today because `example/example1.lua` does not exist (pre-existing, owned by plan 65). Do not chase it here.

## Acceptance criteria

- [ ] `include/quiver/c/database.h` declares exactly three query functions, `quiver_database_query_{string,integer,float}`, each with `(db, sql, param_types, param_values, param_count, out_value, out_has_value)`. No `_params` symbol remains.
- [ ] `src/c/database_query.cpp` has three bodies and `convert_params(const char* caller, ...)`. Its two messages read `Cannot <query_string|query_integer|query_float>: ...`.
- [ ] `tests/test_c_api_database_query.cpp`: 12 plain-form tests migrated to `(nullptr, nullptr, 0)`, `_params` suffixes dropped, `QueryParamsNullDb` deleted, `QueryParamsNullStringElement` pins its full message, `QueryParamsUnknownTypeNamesTheCalledFunction` pins all three. The `delete[] value;` lines are untouched.
- [ ] `tests/test_c_api_database_csv_import.cpp` calls `quiver_database_query_integer`.
- [ ] `bindings/julia/src/c_api.jl` regenerated (not hand-edited). `database_query.jl` has five query definitions with `parameters::Vector = []`, all inside `GC.@preserve refs`.
- [ ] `bindings/dart/lib/src/ffi/bindings.dart` hand-edited (ffigen not run; the enum classes are untouched). `_marshalParams` returns `nullptr`/`nullptr`/0 for null or empty. No branch left in `database_query.dart`.
- [ ] `_c_api.py` declares only the three 7-argument query functions. The Python query methods make one FFI call each.
- [ ] `loader.ts` lists only the three 7-argument query symbols. `marshalParams` returns `null, null, 0n` for no parameters. The `bun:ffi` `ptr` import is gone from `query.ts`.
- [ ] New omitted-parameters assertions in the Julia, Python, Dart and JS parameter-count tests. The Python `routes_to_simple` test is renamed.
- [ ] `src/c/AGENTS.md`, `bindings/dart/AGENTS.md` and `bindings/js/AGENTS.md` updated as specified. CHANGELOG 0.11.0 `### Changed` has the **BREAKING** bullet with an *Adapt:* line. No manifest version changed.
- [ ] Verification steps 1-10 pass, and step 11 passes except the pre-existing CLI smoke failure.

## Pitfalls

- **Do not run `scripts/generator.bat` or `bindings/dart/generator/generator.bat`.** Both run ffigen and rewrite `bindings.dart` into enum-based bindings that break hub. Run only the Julia generator.
- **Stale Dart native build.** The Dart hook caches the compiled C library under `bindings/dart/.dart_tool/`. The old DLL still exports `quiver_database_query_string`, but with 4 arguments. Called with 7, it reads `param_types` (NULL) as `out_value` and fails with `Null argument: out_value`. That is confusing, so clear `.dart_tool/hooks_runner` and `.dart_tool/lib` first (`bindings/dart/AGENTS.md`, "Stale native cache").
- **Same-name trap for every FFI layer.** The names survive and only the arity changes, so a binding still pointed at an old library does not fail at load time. It misbehaves at call time. Python and JS load from `build/bin` (their `test.bat` prepend it to PATH), and Julia from `build/bin` (no `Artifacts.toml` in the monorepo). So always rebuild (step 1) before the binding tests.
- **JS `dlopen` fails on a stale symbol.** If `loader.ts` still lists a `_params` name, `getSymbols()` throws on the first call and every JS test fails. Remove all three entries.
- **Don't pass a zero-length TypedArray to Bun.** `ptr()` of one returns a `TypeError` object silently (it does not throw), and the zero-length argument path is unspecified. Keep the `n === 0` early return in `marshalParams`.
- **Julia regen can surface other diffs.** If an earlier plan changed a header without regenerating, its hunks appear too. Keep them, since the generated file must match the headers, and note it.
- **Line endings.** `.cpp/.h/.dart/.jl/.py` are LF in the working tree (`eol=lf`). `.ts` and `.md` files are CRLF in the working tree (`text=auto` + `core.autocrlf=true`), and git normalizes them on commit. The Edit tool preserves either. Don't run sed/awk over them. No `.bat` file is edited.
- **Julia default argument.** Use `parameters::Vector = []`, not `parameters::Vector{Any} = Any[]`. Callers pass typed vectors (`Int64[...]`, `String[...]`) in `test_database_update.jl` and `test_database_csv_import.jl`, and `::Vector` accepts them.
- **Python `parameters or []`** rebinds the argument before marshalling. `len(parameters)` must come from that rebound list, not the original `None`.
- **No binding test pins the old messages.** The only pin was `msg.find("unknown parameter type")` in `QueryParamsUnknownType`, which is replaced. `git grep "Cannot query:"` finds only `src/c/database_query.cpp` today. `src/schema.cpp`'s "Cannot query columns/foreign keys/indexes" messages are unrelated (plan 62).

## Out of scope

- `delete[] value;` frees in `QueryStringReturnsValue` / `QueryStringWithParams`: plan 69.
- `bigint` query parameters in JS (`marshalParams` loop): plan 33.
- Python `_marshal_params` datetime or bool changes: rejected or left unchanged by plans 25 and 28.
- `convert_params` rejecting `QUIVER_DATA_TYPE_DATE_TIME` (3) as "unknown parameter type 3": pre-existing and untouched here. Bindings bind datetimes as strings.
- Upgrading the Dart ffigen output: its own deliberate change per `bindings/dart/AGENTS.md`.
- `src/schema.cpp` "Cannot query columns..." messages: plan 62.
- The JS README method list: plan 74. Its `queryString(sql, parameters?)` line is already correct.
