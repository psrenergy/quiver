# 12 — Delete the unused C API binary-metadata builder family and C++ add_dimension/add_time_dimension

**Batch** 2 · **Severity** m · **Breaking** yes: C callers of 7 `quiver_binary_metadata_*` symbols, and C++ callers of `BinaryMetadata::add_dimension` / `add_time_dimension`. Julia and Lua public APIs are unchanged. · **Size** M · **Layers** C++ core, C API, Julia (generated FFI + one test comment), docs/changelog

**Depends on** none. It runs after plans 08–11, which edit the same C++ files, so every edit below is anchored by function or test name, not by line. · **Overlaps with**
- **08**: reorders validation inside `BinaryMetadata::from_toml_content`. The new C++ tests below go through that function.
- **09**: moves `initial_value` into a single computation. Its note says the builder half of that problem "disappears with plan 12". If plan 09 made `add_time_dimension` call its new helper, delete `add_time_dimension` anyway and keep the helper.
- **11**: adds length checks and missing-key errors to `from_toml_content`, changes how `from_element` reaches it, and may add tests to both binary-metadata test files. Every TOML in this plan has all six required keys, explicit `time_dimensions` / `frequencies`, and equal array lengths, so it passes plan 11's checks.
- **16**: also regenerates `bindings/julia/src/c_api.jl`. The two plans are independent.
- **21**: deletes the other dead C API symbols (`quiver_clear_last_error`, element counters). It shares the CHANGELOG `### Removed` section this plan creates, and edits `src/c/CLAUDE.md` line ~49. This plan edits only the memory-management block of that file (line ~112).

## Why

The C API has two ways to build a `quiver_binary_metadata_t`:
- one-shot factories: `quiver_binary_metadata_from_toml` and `quiver_binary_metadata_from_element`;
- an incremental builder family: `quiver_binary_metadata_create` plus `_set_initial_datetime`, `_set_unit`, `_set_version`, `_set_labels`, `_add_dimension` and `_add_time_dimension`.

Nothing uses the builder family. I verified this with a repo-wide grep, and also across `C:\Development\Claw\*`. The only references are:
- the header and the implementation;
- `tests/test_c_api_binary_metadata.cpp`;
- the generated `bindings/julia/src/c_api.jl`;
- one stale Julia comment;
- three doc lines.

The subsystem's two consumers both use the factories:
- Julia `Metadata(; kwargs...)` (`bindings/julia/src/binary/metadata.jl:28-48`) builds an `Element` and calls `quiver_binary_metadata_from_element`.
- Lua `quiver.metadata{...}` (`src/lua_runner.cpp` ~L1061-1072, `build_metadata_from_lua`) calls `BinaryMetadata::from_element`.

Dart, Python and JS bind no binary API, by the root design decision.

The C++ `BinaryMetadata::add_dimension` / `add_time_dimension` are called only by these two C builders (`src/c/binary/binary_metadata.cpp` ~L168, ~L183) and by `tests/test_binary_metadata.cpp` (the `BinaryMetadataAddDimension` suite). Deleting only the C half would leave public C++ methods with no C binding, which breaks the root principle "All public C++ methods should be bound to C API". So both halves go.

The builder path is also wrong, not just unused. `add_time_dimension` stores `initial_value = 0` (`src/binary/binary_metadata.cpp` ~L584):

```cpp
    TimeProperties time_props{freq_enum, 0, parent_index};
    dimensions.push_back({name, size, std::move(time_props)});
```

Nothing on that path ever derives it: neither `set_initial_datetime` nor `validate()` does. Only `from_toml_content` does, via `compute_time_dimension_initial_values`. `first_dimensions` (`src/binary/iteration.cpp` ~L90) returns `dim.time->initial_value` as the start coordinate.

**Reproduction (current code, C API):**
1. `create`, `set_version("1")`, `set_unit("MW")`, `set_labels({"a"})`, `set_initial_datetime("2025-03-15T00:00:00")`, `add_time_dimension("month",12,"monthly")`, `add_time_dimension("day",31,"daily")`.
2. The C API reports `time_properties.initial_value == 0` for both dimensions.

   With `from_toml` / `from_element` on the same fields, the values are `1` and `15`, and every traversal of builder-made metadata starts at coordinate `{0, 0}`.

It also re-implements core logic in the C layer. `quiver_binary_metadata_set_initial_datetime` (`src/c/binary/binary_metadata.cpp` ~L116-120) copies the C++ parse and its exact message:

```cpp
        std::tm tm{};
        if (!quiver::datetime::parse_iso8601(iso8601, tm)) {
            throw std::runtime_error(std::string("Failed to parse initial_datetime: ") + iso8601);
        }
```

That is the same text as `from_toml_content` (`src/binary/binary_metadata.cpp` ~L309-313). `set_labels` writes its own C-layer message, `"Cannot set_labels: null label at index "`. Both break `src/c/CLAUDE.md`: "the C API never re-implements validation or error messages that exist in C++".

This change violates no principle. It applies two: "Delete unused code, do not deprecate", and "Simple solutions over complex abstractions" (one construction path).

## Constraints and decisions

- **Maintainer decision (binding), from the item's notes:**
  - Scope is only the binary-metadata part. The element counters and `quiver_clear_last_error` belong to plan 21.
  - The change is BREAKING, with a CHANGELOG entry under 0.11.0.
  - Port the getter and null-argument C API tests to `from_toml` handles.
  - Move the useful C++ builder-test checks onto `from_element` / `from_toml_content`.
  - Regenerate Julia `c_api.jl`. Dart does not bind binary.
- Root `CLAUDE.md`, Principles: "WIP project - breaking changes acceptable"; "Delete unused code, do not deprecate"; "All public C++ methods should be bound to C API". That last rule is why the C++ half is deleted too.
- Root `CLAUDE.md`, Design Decisions: "Binary + expression subsystems are exposed in Julia and Lua only". So there is no Dart, Python or JS work: `bindings/dart/lib/src/ffi/bindings.dart`, `bindings/python/src/quiverdb/_c_api.py` and `bindings/js/src/loader.ts` hold no `quiver_binary_metadata_*` symbol (verified by grep).
- `src/c/CLAUDE.md`: "the C API never re-implements validation or error messages that exist in C++".
- Versioning: 0.11.0 is unreleased and is already a minor bump over 0.10.9, so no manifest bump is needed. The two verifiers' "bump the 0.x minor version" step is already satisfied and is dropped.
- `bindings/julia/CLAUDE.md`: "`src/c_api.jl` GENERATED low-level FFI module (do not hand-edit; regenerate)".
- Self-Updating: fix the CLAUDE.md lines that name the builders:
  - root `CLAUDE.md` ~L717;
  - `src/CLAUDE.md` ~L697;
  - `src/c/CLAUDE.md` ~L112.

Alternatives considered and rejected:
- **Keep the C++ `add_dimension` / `add_time_dimension` and delete only the C wrappers.** This leaves public C++ methods with no C binding, against a root principle. Their only other callers are tests.
- **Keep the builders and fix them** (derive `initial_value` in `add_time_dimension` / `set_initial_datetime`). That keeps a second construction path that no one calls and that has already drifted once. See the comment at `add_time_dimension`: "Chain to the previous time dimension, matching from_toml_content/from_element".
- **The original finding's hedge, "keep the C++ half if out-of-repo code relies on it".** Dropped. Nothing in the Claw repos uses it, and the project is WIP.
- **Delete every test in `test_c_api_binary_metadata.cpp` that calls `create`.** Rejected: that loses getter, free-helper and bounds coverage. Those tests are ported to `from_toml` instead.

## Changes

Apply the steps in this order. After each C/C++ step, `git grep` for the deleted names (see Verification) catches leftovers.

### 1. `include/quiver/c/binary/binary_metadata.h`

1a. In the `// Lifecycle` block, delete the `create` declaration (currently ~L39). Current:

```c
// Lifecycle
QUIVER_C_API quiver_error_t quiver_binary_metadata_create(quiver_binary_metadata_t** out);
QUIVER_C_API quiver_error_t quiver_binary_metadata_free(quiver_binary_metadata_t* md);
```

New:

```c
// Lifecycle (a handle is built only by the two factories below)
QUIVER_C_API quiver_error_t quiver_binary_metadata_free(quiver_binary_metadata_t* md);
```

1b. Delete the whole `// Builders` block (currently ~L51-65), including its comment line and the blank line after it. Current:

```c
// Builders
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_initial_datetime(quiver_binary_metadata_t* md,
                                                                        const char* iso8601);
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_unit(quiver_binary_metadata_t* md, const char* unit);
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_version(quiver_binary_metadata_t* md, const char* version);
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_labels(quiver_binary_metadata_t* md,
                                                              const char* const* labels,
                                                              size_t count);
QUIVER_C_API quiver_error_t quiver_binary_metadata_add_dimension(quiver_binary_metadata_t* md,
                                                                 const char* name,
                                                                 int64_t size);
QUIVER_C_API quiver_error_t quiver_binary_metadata_add_time_dimension(quiver_binary_metadata_t* md,
                                                                      const char* name,
                                                                      int64_t size,
                                                                      const char* frequency);
```

New: nothing. `// Serialization` (`to_toml`) is followed directly by `// Getters`.

### 2. `src/c/binary/binary_metadata.cpp`

2a. Delete `quiver_binary_metadata_create` (currently ~L49-59), which is the whole function:

```cpp
QUIVER_C_API quiver_error_t quiver_binary_metadata_create(quiver_binary_metadata_t** out) {
    QUIVER_REQUIRE(out);

    try {
        *out = new quiver_binary_metadata{quiver::BinaryMetadata{}};
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    }
}
```

The `// Lifecycle` comment then heads only `quiver_binary_metadata_free`. Keep `free` exactly as it is.

2b. Delete the whole `// Builders` section (currently ~L109-189). It runs from the line `// Builders` through the closing `}` of `quiver_binary_metadata_add_time_dimension`, and holds these seven functions:
- `quiver_binary_metadata_set_initial_datetime`
- `quiver_binary_metadata_set_unit`
- `quiver_binary_metadata_set_version`
- `quiver_binary_metadata_set_labels`
- `quiver_binary_metadata_add_dimension`
- `quiver_binary_metadata_add_time_dimension`

The section begins:

```cpp
// Builders

QUIVER_C_API quiver_error_t quiver_binary_metadata_set_initial_datetime(quiver_binary_metadata_t* md,
                                                                        const char* iso8601) {
```

and ends:

```cpp
    try {
        md->metadata.add_time_dimension(name, size, frequency);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

After the deletion, `// Serialization` (`quiver_binary_metadata_to_toml`) is followed by `// Getters`.

2c. Includes. `#include <new>` was there only for `std::bad_alloc` in `create`. After 2b, nothing in the file uses `std::chrono` directly: `std::tm` / `parse_iso8601` / `tm_to_time_point` were only in `set_initial_datetime`. Delete both includes. Current:

```cpp
#include "quiver/c/binary/binary_metadata.h"

#include "../database_helpers.h"
#include "../internal.h"
#include "utils/datetime.h"

#include <chrono>
#include <new>
#include <string>
#include <vector>
```

New:

```cpp
#include "quiver/c/binary/binary_metadata.h"

#include "../database_helpers.h"
#include "../internal.h"
#include "utils/datetime.h"

#include <string>
#include <vector>
```

Keep `utils/datetime.h`: `quiver_binary_metadata_get_initial_datetime` still calls `quiver::datetime::format_utc`. The anonymous-namespace helpers (`to_c_frequency`, `convert_dimension_to_c`) stay.

### 3. `include/quiver/binary/binary_metadata.h` (C++ public header)

Delete the `// Setters` block (currently ~L41-43) and the blank line before it. Current (tail of `struct BinaryMetadata`):

```cpp
    // Validation
    void validate() const;
    void validate_time_dimension_metadata() const;
    void validate_time_dimension_sizes() const;

    // Setters
    void add_dimension(const std::string& name, int64_t size);
    void add_time_dimension(const std::string& name, int64_t size, const std::string& frequency);
};
```

New:

```cpp
    // Validation
    void validate() const;
    void validate_time_dimension_metadata() const;
    void validate_time_dimension_sizes() const;
};
```

If an earlier plan (08–11) added members between `// Validation` and `// Setters`, keep them. Delete only the three `// Setters` lines.

### 4. `src/binary/binary_metadata.cpp`

Delete the two definitions just before `}  // namespace quiver` at the end of the file (currently ~L571-586). Current:

```cpp
void BinaryMetadata::add_dimension(const std::string& name, int64_t size) {
    dimensions.push_back({name, size, std::nullopt});
}

void BinaryMetadata::add_time_dimension(const std::string& name, int64_t size, const std::string& frequency) {
    TimeFrequency freq_enum = frequency_from_string(frequency);
    // Chain to the previous time dimension, matching from_toml_content/from_element
    int64_t parent_index = -1;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].is_time_dimension()) {
            parent_index = static_cast<int64_t>(i);
        }
    }
    TimeProperties time_props{freq_enum, 0, parent_index};
    dimensions.push_back({name, size, std::move(time_props)});
}
```

New: nothing. The file ends with `validate_time_dimension_sizes()`'s closing brace, a blank line, and `}  // namespace quiver`.

- If plan 09 changed `add_time_dimension`'s body (for example, to call an initial-value helper), still delete the whole function, but keep that helper: `from_toml_content` / `ExpressionAggregate` use it.
- No include changes: `frequency_from_string`, `TimeProperties` and `std::nullopt` are still used by `from_toml_content`.

### 5. `bindings/julia/src/c_api.jl` (generated)

Regenerate. Do not hand-edit.

```
bindings/julia/generator/generator.bat
```

It runs `julia +1.12.5 --project=bindings/julia/generator generator.jl`. The expected diff is **only** the removal of these seven wrapper functions (currently ~L625-667):
- `quiver_binary_metadata_create`
- `quiver_binary_metadata_set_initial_datetime`
- `quiver_binary_metadata_set_unit`
- `quiver_binary_metadata_set_version`
- `quiver_binary_metadata_set_labels`
- `quiver_binary_metadata_add_dimension`
- `quiver_binary_metadata_add_time_dimension`

For example:

```julia
function quiver_binary_metadata_create(out)
    @ccall libquiver_c.quiver_binary_metadata_create(out::Ptr{Ptr{quiver_binary_metadata_t}})::quiver_error_t
end
```

See Pitfalls if other hunks appear.

No hand-written Julia source changes. `bindings/julia/src/binary/metadata.jl` calls only:
- `from_toml`, `from_element`, `to_toml`;
- the getters;
- `free`, `free_string`, `free_string_array`, `free_dimension`.

### 6. Lua, Dart, Python, JS

No change.
- **Lua:** `src/lua_runner.cpp` never calls `add_dimension` / `add_time_dimension`. `quiver.metadata` and `quiver.metadata_from_element` use `BinaryMetadata::from_element`, and `quiver.metadata_from_toml` uses `from_toml_content`.
- **Dart / Python / JS:** they declare no binary symbols (design decision).

## Tests

None of the kept or ported tests is a regression test that fails before the change. This is a deletion. The ported tests use only the factories, so they pass on the current code and after the change. What catches a mistake:
- a compile error in `quiver_c_tests` / `quiver_tests` if any deleted symbol is still referenced;
- the `git grep` in Verification.

### C API — `tests/test_c_api_binary_metadata.cpp`

Today the suite `BinaryCApiMetadata` has 34 tests. After this plan it has 21, plus whatever plan 11 added. Edit test by test, anchored by test name. Do not replace the file: plan 11 may have added tests to it.

**T0. Add one file-scope TOML constant** right after the `#include` block, before the first `// ====` banner. If a constant with identical content already exists in the file (for example, one added by plan 11), reuse that one and skip this.

```cpp
// The C API builds metadata only through its two factories (from_toml / from_element); tests that
// just need a valid handle take it from this TOML.
static const char* const VALID_TOML = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "block"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["plant_1", "plant_2"]
)";
```

Leave the identical inline literals in `GetNumberOfTimeDimensions` and `TomlRoundTrip` alone. They are out of scope, and leaving them keeps the diff small.

**T1. Delete** these tests entirely: the `TEST(...)` line through its closing `}`, plus the blank line after it.

| Test | Why it goes / where its coverage now lives |
|------|-------------------------------------------|
| `CreateAndDestroy` | `create` is gone. `DestroyNull` stays. |
| `SetAndGetUnit` | Setter test. `get_unit` is covered by `TomlRoundTrip` and `FromElement`. |
| `SetAndGetVersion` | Setter test. `get_version` is covered by `TomlRoundTrip`. |
| `SetAndGetLabels` | Setter test. `get_labels` is covered by the ported `FreeStringArrayExplicit`. |
| `AddDimensionAndGet` | Covered by the ported `MultipleDimensions`. |
| `AddTimeDimensionAndGet` | Covered by the ported `MixedDimensionsAndTimeDimensions`. |
| `NullArgs` | Only `create`/`set_unit` plus one `from_toml(nullptr, nullptr)` check, which `NullArgsErrorMessages` already makes with its message. |
| `NullArgsOnSetters` | Setters are gone. |
| `GetDimensionOutOfRangeErrorMessage` | Merged into the ported `GetDimensionOutOfRange` (T4). |
| `TomlRoundTripFromBuilders` | Builder test. `TomlRoundTrip` covers from_toml → to_toml → from_toml. |
| `EmptyLabels` | Setter semantics. Empty labels cannot come out of a factory: `validate()` rejects them, covered by C++ `BinaryMetadataValidate.EmptyLabels`. |
| `OverwriteLabels` | Setter semantics. |
| `ZeroTimeDimensions` | Merged into the ported `MultipleDimensions` (T9). |

Also delete the section banners that are left with no test under them:
- `// TOML round-trip with builders`, above `TomlRoundTripFromBuilders`;
- `// Labels -- edge cases`, above `EmptyLabels`.

Rename two banners. Each is the middle line of a three-line `// ====` block:
- `// Builders and Getters` → `// Getters`
- `// Zero time dimensions` → `// Time dimension chaining`

**T2. Replace `SetAndGetInitialDatetime`** (the whole test) with:

```cpp
TEST(BinaryCApiMetadata, GetInitialDatetime) {
    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_binary_metadata_from_toml(VALID_TOML, &md), QUIVER_OK);

    char* datetime = nullptr;
    EXPECT_EQ(quiver_binary_metadata_get_initial_datetime(md, &datetime), QUIVER_OK);
    ASSERT_NE(datetime, nullptr);
    // Chrono-based, timezone-independent formatting (src/utils/datetime.h format_utc).
    EXPECT_STREQ(datetime, "2025-01-01T00:00:00");
    quiver_binary_metadata_free_string(datetime);

    quiver_binary_metadata_free(md);
}
```

`format_utc` writes `"%04d-%02u-%02uT%02d:%02d:%02d"`, so an exact match is correct. The Julia 1960 test already relies on it.

**T3. Keep `GetNumberOfTimeDimensions` and `TomlRoundTrip` unchanged.**

**T4. Replace `GetDimensionOutOfRange`** (the whole test). Current:

```cpp
TEST(BinaryCApiMetadata, GetDimensionOutOfRange) {
    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_binary_metadata_create(&md), QUIVER_OK);

    quiver_dimension_t dim = {};
    EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 0, &dim), QUIVER_ERROR);

    quiver_binary_metadata_free(md);
}
```

New. A factory-built handle always has at least one dimension, so the out-of-range index is `count`:

```cpp
TEST(BinaryCApiMetadata, GetDimensionOutOfRange) {
    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_binary_metadata_from_toml(VALID_TOML, &md), QUIVER_OK);

    quiver_dimension_t dim = {};
    EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 2, &dim), QUIVER_ERROR);  // VALID_TOML has 2 dimensions
    EXPECT_STREQ(quiver_get_last_error(), "Cannot get_dimension: index out of range");

    quiver_binary_metadata_free(md);
}
```

**T5. Edit `NullArgsErrorMessages`.** Delete these seven assertion pairs (each is an `EXPECT_EQ` line plus the `EXPECT_STREQ` line after it):

```cpp
    EXPECT_EQ(quiver_binary_metadata_create(nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");
```
```cpp
    EXPECT_EQ(quiver_binary_metadata_set_unit(nullptr, "MW"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
```
```cpp
    EXPECT_EQ(quiver_binary_metadata_set_version(nullptr, "1"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
```
```cpp
    EXPECT_EQ(quiver_binary_metadata_set_initial_datetime(nullptr, "2025-01-01T00:00:00"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
```
```cpp
    EXPECT_EQ(quiver_binary_metadata_set_labels(nullptr, nullptr, 0), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
```
```cpp
    EXPECT_EQ(quiver_binary_metadata_add_dimension(nullptr, "x", 10), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
```
```cpp
    EXPECT_EQ(quiver_binary_metadata_add_time_dimension(nullptr, "x", 10, "monthly"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: md");
```

Keep every other pair (`from_toml`, `to_toml`, and the seven getters) exactly as it is. The test then starts with `quiver_binary_metadata_from_toml(nullptr, nullptr)` → `"Null argument: toml"`.

**T6. Edit `NullArgsOnGetterOutParams`.** Replace only its setup line:

```cpp
    ASSERT_EQ(quiver_binary_metadata_create(&md), QUIVER_OK);
```

with:

```cpp
    ASSERT_EQ(quiver_binary_metadata_from_toml(VALID_TOML, &md), QUIVER_OK);
```

All eight assertions after it stay unchanged, including `get_dimension(md, 0, nullptr)` → `"Null argument: out"`. `QUIVER_REQUIRE` checks `out` before the index.

**T7. Replace `InvalidFrequency` and `InvalidInitialDatetime`** (both whole tests) with factory-driven versions. The error now comes from the one C++ owner of each message, through `quiver_binary_metadata_from_toml`'s catch:

```cpp
TEST(BinaryCApiMetadata, InvalidFrequency) {
    const char* toml = R"(
version = "1"
dimensions = ["stage"]
dimension_sizes = [4]
time_dimensions = ["stage"]
frequencies = ["invalid_freq"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";

    quiver_binary_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("invalid_freq"), std::string::npos);
}

TEST(BinaryCApiMetadata, InvalidInitialDatetime) {
    const char* toml = R"(
version = "1"
dimensions = ["row"]
dimension_sizes = [3]
time_dimensions = []
frequencies = []
initial_datetime = "not-a-date"
unit = "MW"
labels = ["val"]
)";

    quiver_binary_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
    std::string err = quiver_get_last_error();
    EXPECT_NE(err.find("not-a-date"), std::string::npos);
}
```

Today these produce `"Unknown frequency: invalid_freq"` (`frequency_from_string`, `src/binary/time_properties.cpp`) and `"Failed to parse initial_datetime: not-a-date"` (`from_toml_content`). The assertions check only the offending value, as the old tests did, so plan 11 rewording the prefix does not break them.

**T8. Replace `FreeStringArrayExplicit`** (the whole test) with:

```cpp
TEST(BinaryCApiMetadata, FreeStringArrayExplicit) {
    const char* toml = R"(
version = "1"
dimensions = ["row"]
dimension_sizes = [3]
time_dimensions = []
frequencies = []
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["a", "b", "c"]
)";

    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_OK);

    char** out_labels = nullptr;
    size_t count = 0;
    EXPECT_EQ(quiver_binary_metadata_get_labels(md, &out_labels, &count), QUIVER_OK);
    ASSERT_EQ(count, 3u);
    EXPECT_STREQ(out_labels[0], "a");
    EXPECT_STREQ(out_labels[1], "b");
    EXPECT_STREQ(out_labels[2], "c");
    EXPECT_EQ(quiver_binary_metadata_free_string_array(out_labels, count), QUIVER_OK);

    quiver_binary_metadata_free(md);
}
```

**T9. Replace `AllTimeFrequencies`, `MultipleDimensions` and `MixedDimensionsAndTimeDimensions`** (each whole test) with:

```cpp
TEST(BinaryCApiMetadata, AllTimeFrequencies) {
    struct FreqTest {
        const char* name;
        quiver_time_frequency_t expected;
    };
    FreqTest cases[] = {
        {"yearly", QUIVER_TIME_FREQUENCY_YEARLY},
        {"monthly", QUIVER_TIME_FREQUENCY_MONTHLY},
        {"weekly", QUIVER_TIME_FREQUENCY_WEEKLY},
        {"daily", QUIVER_TIME_FREQUENCY_DAILY},
        {"hourly", QUIVER_TIME_FREQUENCY_HOURLY},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        // A lone time dimension is the outermost one, so its size is unconstrained for every frequency.
        const std::string toml = std::string(R"(
version = "1"
dimensions = ["t"]
dimension_sizes = [10]
time_dimensions = ["t"]
frequencies = [")") + tc.name + R"("]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";

        quiver_binary_metadata_t* md = nullptr;
        ASSERT_EQ(quiver_binary_metadata_from_toml(toml.c_str(), &md), QUIVER_OK);

        quiver_dimension_t dim = {};
        EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 0, &dim), QUIVER_OK);
        EXPECT_EQ(dim.is_time_dimension, 1);
        EXPECT_EQ(dim.time_properties.frequency, tc.expected);
        quiver_binary_metadata_free_dimension(&dim);
        quiver_binary_metadata_free(md);
    }
}
```

```cpp
TEST(BinaryCApiMetadata, MultipleDimensions) {
    const char* toml = R"(
version = "1"
dimensions = ["x", "y", "z"]
dimension_sizes = [10, 20, 30]
time_dimensions = []
frequencies = []
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";

    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_OK);

    size_t count = 0;
    EXPECT_EQ(quiver_binary_metadata_get_dimension_count(md, &count), QUIVER_OK);
    EXPECT_EQ(count, 3u);

    int64_t num_time = -1;
    EXPECT_EQ(quiver_binary_metadata_get_number_of_time_dimensions(md, &num_time), QUIVER_OK);
    EXPECT_EQ(num_time, 0);

    const char* expected_names[] = {"x", "y", "z"};
    int64_t expected_sizes[] = {10, 20, 30};

    for (size_t i = 0; i < 3; ++i) {
        quiver_dimension_t dim = {};
        EXPECT_EQ(quiver_binary_metadata_get_dimension(md, i, &dim), QUIVER_OK);
        EXPECT_STREQ(dim.name, expected_names[i]);
        EXPECT_EQ(dim.size, expected_sizes[i]);
        EXPECT_EQ(dim.is_time_dimension, 0);
        quiver_binary_metadata_free_dimension(&dim);
    }

    quiver_binary_metadata_free(md);
}
```

```cpp
TEST(BinaryCApiMetadata, MixedDimensionsAndTimeDimensions) {
    const char* toml = R"(
version = "1"
dimensions = ["stage", "scenario"]
dimension_sizes = [4, 5]
time_dimensions = ["stage"]
frequencies = ["monthly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";

    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_OK);

    size_t count = 0;
    EXPECT_EQ(quiver_binary_metadata_get_dimension_count(md, &count), QUIVER_OK);
    EXPECT_EQ(count, 2u);

    quiver_dimension_t dim0 = {};
    EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 0, &dim0), QUIVER_OK);
    EXPECT_STREQ(dim0.name, "stage");
    EXPECT_EQ(dim0.is_time_dimension, 1);
    EXPECT_EQ(dim0.time_properties.frequency, QUIVER_TIME_FREQUENCY_MONTHLY);
    quiver_binary_metadata_free_dimension(&dim0);

    quiver_dimension_t dim1 = {};
    EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 1, &dim1), QUIVER_OK);
    EXPECT_STREQ(dim1.name, "scenario");
    EXPECT_EQ(dim1.is_time_dimension, 0);
    quiver_binary_metadata_free_dimension(&dim1);

    quiver_binary_metadata_free(md);
}
```

`MultipleDimensions` now also carries the old `ZeroTimeDimensions` assertion (`num_time == 0`). That is why `ZeroTimeDimensions` is deleted in T1.

**T10. Replace `BuilderTimeDimensionsCounted`** (the whole test, under the renamed `// Time dimension chaining` banner). The new test pins parent chaining across a non-time dimension and, through the C struct, the derived `initial_value`: the field the builder path left at 0.

```cpp
TEST(BinaryCApiMetadata, TimeDimensionsChainToPreviousTimeDimension) {
    const char* toml = R"(
version = "1"
dimensions = ["month", "scenario", "day"]
dimension_sizes = [12, 3, 31]
time_dimensions = ["month", "day"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-03-15T00:00:00"
unit = "MW"
labels = ["val"]
)";

    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_OK);

    int64_t num_time = 0;
    EXPECT_EQ(quiver_binary_metadata_get_number_of_time_dimensions(md, &num_time), QUIVER_OK);
    EXPECT_EQ(num_time, 2);

    quiver_dimension_t month = {};
    EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 0, &month), QUIVER_OK);
    EXPECT_EQ(month.time_properties.parent_dimension_index, -1);
    EXPECT_EQ(month.time_properties.initial_value, 1);  // the outermost time dimension starts at 1
    quiver_binary_metadata_free_dimension(&month);

    quiver_dimension_t day = {};
    EXPECT_EQ(quiver_binary_metadata_get_dimension(md, 2, &day), QUIVER_OK);
    EXPECT_EQ(day.time_properties.parent_dimension_index, 0);  // skips the non-time "scenario"
    EXPECT_EQ(day.time_properties.initial_value, 15);          // derived from initial_datetime
    quiver_binary_metadata_free_dimension(&day);

    quiver_binary_metadata_free(md);
}
```

`initial_value == 15` for Daily under Monthly on March 15 holds before this plan (Julia `test_binary_metadata.jl` ~L82-83 asserts the same). It still holds after plans 08 and 09: Daily under Monthly is the day of the month in both.

**Kept unchanged:**
- `DestroyNull`, `GetNumberOfTimeDimensions`, `TomlRoundTrip`, `FromElement`;
- `InvalidToml`, `FreeStringNull`, `FreeStringArrayNull`, `FreeDimensionNull`;
- `FromElementNullArgs`, `FromElementMissingRequiredField`.

Final `BinaryCApiMetadata` list, 21 tests, plus any added by plan 11:
- `DestroyNull`, `GetInitialDatetime`, `GetDimensionOutOfRange`, `GetNumberOfTimeDimensions`;
- `TomlRoundTrip`, `FromElement`, `NullArgsErrorMessages`, `NullArgsOnGetterOutParams`;
- `InvalidToml`, `InvalidFrequency`, `InvalidInitialDatetime`;
- `FreeStringNull`, `FreeStringArrayNull`, `FreeStringArrayExplicit`, `FreeDimensionNull`;
- `AllTimeFrequencies`, `MultipleDimensions`, `MixedDimensionsAndTimeDimensions`;
- `FromElementNullArgs`, `FromElementMissingRequiredField`;
- `TimeDimensionsChainToPreviousTimeDimension`.

### C++ — `tests/test_binary_metadata.cpp`

**C1. Delete the whole `BinaryMetadataAddDimension` section:**
- its three-line banner (`// ====`, `// BinaryMetadataAddDimension`, `// ====`, currently ~L753-755);
- all seven tests under it, currently ~L757-819: `NonTimeDimension`, `TimeDimension`, `InvalidFrequencyThrows`, `MultipleAddsAccumulate`, `TimeDimensionParentIndexMinusOne`, `TimeDimensionsCountedAndChained`, `ValidationFiresOnBuilderMetadata`.

Today this is the end of the file. If a later-added section follows it (from plans 08–11), keep that section. Make sure the file still ends with exactly one newline.

Where each check's coverage now lives:
- `NonTimeDimension` → `BinaryMetadataFromTomlContent.NoTimeDimensions`.
- `TimeDimension` and `TimeDimensionParentIndexMinusOne` → `FromTomlContent.FrequenciesAssigned` and `ParentIndicesSet`.
- `MultipleAddsAccumulate` → `AllFieldsPopulated`.
- The three checks that have no factory-path equivalent move in C2–C4.

**C2. Extend `BinaryMetadataFromTomlContent.MixedTimeAndNonTime`** with the chaining check from `TimeDimensionsCountedAndChained`. Current tail of the test:

```cpp
    auto md = BinaryMetadata::from_toml_content(toml);
    EXPECT_EQ(md.dimensions.size(), 3u);
    EXPECT_TRUE(md.dimensions[0].is_time_dimension());
    EXPECT_FALSE(md.dimensions[1].is_time_dimension());
    EXPECT_TRUE(md.dimensions[2].is_time_dimension());
    EXPECT_EQ(md.number_of_time_dimensions(), 2);
}
```

New tail:

```cpp
    auto md = BinaryMetadata::from_toml_content(toml);
    EXPECT_EQ(md.dimensions.size(), 3u);
    EXPECT_TRUE(md.dimensions[0].is_time_dimension());
    EXPECT_FALSE(md.dimensions[1].is_time_dimension());
    EXPECT_TRUE(md.dimensions[2].is_time_dimension());
    EXPECT_EQ(md.number_of_time_dimensions(), 2);
    // A time dimension chains to the previous *time* dimension, skipping the non-time "scenario"
    EXPECT_EQ(md.dimensions[0].time->parent_dimension_index, -1);
    EXPECT_EQ(md.dimensions[2].time->parent_dimension_index, 0);
}
```

This holds on the current code, since `from_toml_content` sets `previous_time_dim_index = i`, a dimension index.

**C3–C4. Add three tests** to the `BinaryMetadataFromTomlContent` section, directly after `ErrorTimeDimensionsOutOfOrder`:

```cpp
TEST(BinaryMetadataFromTomlContent, ErrorUnknownFrequency) {
    std::string toml = R"(
version = "1"
dimensions = ["stage"]
dimension_sizes = [4]
time_dimensions = ["stage"]
frequencies = ["invalid_freq"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    EXPECT_THROW(BinaryMetadata::from_toml_content(toml), std::invalid_argument);
}

TEST(BinaryMetadataFromTomlContent, ErrorInvalidInitialDatetime) {
    std::string toml = R"(
version = "1"
dimensions = ["row"]
dimension_sizes = [3]
time_dimensions = []
frequencies = []
initial_datetime = "not-a-date"
unit = "MW"
labels = ["val"]
)";
    EXPECT_THROW(BinaryMetadata::from_toml_content(toml), std::runtime_error);
}

TEST(BinaryMetadataFromTomlContent, ErrorDuplicateFrequencies) {
    std::string toml = R"(
version = "1"
dimensions = ["a", "b"]
dimension_sizes = [12, 12]
time_dimensions = ["a", "b"]
frequencies = ["monthly", "monthly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    EXPECT_THROW(BinaryMetadata::from_toml_content(toml), std::runtime_error);
}
```

Where each exception comes from today:
- `ErrorUnknownFrequency`: `frequency_from_string` throws `std::invalid_argument("Unknown frequency: invalid_freq")`. This is the type the deleted `InvalidFrequencyThrows` asserted.
- `ErrorInvalidInitialDatetime`: `from_toml_content` throws `std::runtime_error("Failed to parse initial_datetime: not-a-date")`.
- `ErrorDuplicateFrequencies`: `validate_time_dimension_metadata` (called from `validate()`, at the end of `from_toml_content`) throws `std::runtime_error("Time dimension frequencies must be unique. Duplicate: monthly")`. That is what `ValidationFiresOnBuilderMetadata` pinned for the builder path.

All three pass before and after this plan. They carry coverage that existed only on the deleted builder path, or only at the C layer.

### Lua, Dart, Python, JS

No tests change. There is no behaviour to test in these layers: Lua never reached the builders, and the others have no binary binding.

### Julia — `bindings/julia/test/test_binary_metadata.jl`

The tests themselves do not change. Fix the stale comment in `@testset "Pre-epoch initial datetime (1960) round-trips"` (currently ~L460-463). Current:

```julia
        # Regression guard: pre-1970 datetimes go through the C API's
        # quiver_binary_metadata_set_initial_datetime, which on Windows used to corrupt them via
        # _mkgmtime (1960-01-01 -> 1969-12-31T23:59:59). The C++ fix routes through the chrono
        # calendar; assert the exact string survives the FFI round-trip.
```

New. This wording does not name `from_toml_content`, because plan 11 may stop routing `from_element` through it:

```julia
        # Regression guard: `Metadata(; ...)` builds through quiver_binary_metadata_from_element,
        # whose initial_datetime parse (shared with from_toml) used to corrupt pre-1970 datetimes
        # on Windows via _mkgmtime (1960-01-01 -> 1969-12-31T23:59:59). The C++ fix routes through
        # the chrono calendar; assert the exact string survives the FFI round-trip.
```

## Docs and changelog

All four `.md` files below are **CRLF in the working tree**. Edit them with the Edit tool, not `sed`.

**Root `CLAUDE.md`**: the binary cross-layer table (currently ~L717). Julia's keyword constructor and Lua's `quiver.metadata{...}` both assemble an Element and call `from_element`. Old row:

```
| Metadata builder | `BinaryMetadata{}` | `quiver_binary_metadata_create()` | `Metadata(; kwargs...)` | `quiver.metadata{kwargs}` |
```

New row:

```
| Metadata from keywords | `BinaryMetadata::from_element()` | `quiver_binary_metadata_from_element()` | `Metadata(; kwargs...)` | `quiver.metadata{kwargs}` |
```

Leave the next two rows ("Metadata from TOML", "Metadata from Element") unchanged.

**`src/CLAUDE.md`**: `## Binary Subsystem`, the `BinaryMetadata` bullet list (currently ~L695-697).

- Delete the line:
  ```
    - Builders: `add_dimension()`, `add_time_dimension()` (chains `parent_dimension_index` to the previous time dimension)
  ```
- In the Factories line, replace:
  ```
    - Factories: `from_toml_content()`, `from_element()`
  ```
  with:
  ```
    - Factories: `from_toml_content()`, `from_element()` — the only way to build one; they derive every time dimension's `parent_dimension_index` (the previous time dimension) and `initial_value` (from `initial_datetime`)
  ```

If plan 09 or 11 already reworded the Factories line, append only the clause from "— the only way to build one" onward to whatever is there.

**`src/c/CLAUDE.md`**: `## Memory Management`, the code block (currently ~L111-112). Old:

```
// Binary metadata lifecycle
quiver_binary_metadata_create/free
```

New:

```
// Binary metadata lifecycle (built only by the from_toml / from_element factories)
quiver_binary_metadata_from_toml/from_element/free
```

Do not touch the `## Return Codes` exception list at ~L49. It belongs to plan 21.

**`CHANGELOG.md`**: under `## [0.11.0] — unreleased`, add a new `### Removed` section after the last `### Changed` entry and before `### Fixed`. That is Keep a Changelog order: Added, Changed, Removed, Fixed. If an earlier plan already created `### Removed` under 0.11.0, append this bullet to it instead.

```markdown
### Removed

- **BREAKING — the C API's incremental binary-metadata builders, and the C++
  `BinaryMetadata::add_dimension` / `add_time_dimension` behind them.** `quiver_binary_metadata_create`,
  `quiver_binary_metadata_set_initial_datetime`, `quiver_binary_metadata_set_unit`,
  `quiver_binary_metadata_set_version`, `quiver_binary_metadata_set_labels`,
  `quiver_binary_metadata_add_dimension` and `quiver_binary_metadata_add_time_dimension` are gone.
  No binding called them — Julia's `Metadata(; kwargs...)` and Lua's `quiver.metadata{...}` already
  build through `from_element` — and they were the one construction path that never derived a time
  dimension's `initial_value` from `initial_datetime`: it stayed 0, so a traversal of builder-made
  metadata started at coordinate 0. Julia and Lua code is unaffected; only the generated low-level
  `Quiver.C` wrappers for these seven symbols disappear.

  *Adapt:* build the metadata in one call — in C with `quiver_binary_metadata_from_toml` (a TOML
  string with `version`, `dimensions`, `dimension_sizes`, `time_dimensions`, `frequencies`,
  `initial_datetime`, `unit` and `labels`) or `quiver_binary_metadata_from_element` (an element
  carrying the same keys); in C++ with `BinaryMetadata::from_toml_content` or
  `BinaryMetadata::from_element`.
```

No other docs change. `docs/*.md`, the READMEs and `bindings/js/src/lua-api.ts` do not mention these symbols (verified by grep). `tests/CLAUDE.md` names no test from this file.

## Verification

Run from the repo root, in order.

1. Build:
   ```
   cmake --build build --config Debug
   ```
   It must compile with no reference to a deleted symbol.
2. Targeted C++ tests:
   ```
   ./build/bin/quiver_tests.exe --gtest_filter='BinaryMetadata*'
   ```
   - All pass.
   - `BinaryMetadataAddDimension.*` no longer exists.
   - `BinaryMetadataFromTomlContent.ErrorUnknownFrequency`, `.ErrorInvalidInitialDatetime`, `.ErrorDuplicateFrequencies` and `.MixedTimeAndNonTime` pass.
   - The count is today's 82 − 7 + 3 = 78, plus any tests plans 08–11 added.
3. Targeted C API tests:
   ```
   ./build/bin/quiver_c_tests.exe --gtest_filter='BinaryCApiMetadata.*'
   ```
   All pass. There are 21 tests, plus any plan 11 added, including `GetInitialDatetime`, `GetDimensionOutOfRange`, `InvalidFrequency`, `InvalidInitialDatetime`, `AllTimeFrequencies`, `MultipleDimensions`, `MixedDimensionsAndTimeDimensions` and `TimeDimensionsChainToPreviousTimeDimension`.
4. Full native suites:
   ```
   ./build/bin/quiver_tests.exe
   ./build/bin/quiver_c_tests.exe
   ```
   All pass. `test_lua_binary.cpp` / `test_lua_expression.cpp` / `test_c_api_expression.cpp` / `test_c_api_binary_file.cpp` never used the builders.
5. Regenerate Julia FFI:
   ```
   bindings/julia/generator/generator.bat
   git diff --stat -- bindings/julia/src/c_api.jl
   git diff -- bindings/julia/src/c_api.jl
   ```
   The diff must be only the deletion of the 7 wrapper functions, about 28 lines removed.
6. Leftover check, which must print nothing:
   ```
   git grep -nE "quiver_binary_metadata_(create|set_|add_)|add_(time_)?dimension" -- . ':!CHANGELOG.md'
   ```
7. Julia suite:
   ```
   bindings/julia/test/test.bat
   ```
   All pass, including `test_binary_metadata.jl`, `test_binary_file.jl`, `test_csv_converter.jl` and `test_expression.jl`.
8. Format:
   ```
   scripts/format.bat
   ```
   Then `git status`: only the files listed in Acceptance criteria are modified, and no `.bat` file shows as changed.
9. Everything:
   ```
   scripts/test-all.bat
   ```
   All seven steps pass. Dart, JS and Python are run for completeness only; they have nothing binary.

## Acceptance criteria

- [ ] `include/quiver/c/binary/binary_metadata.h` declares none of the 7 builder functions. `// Lifecycle` holds only `quiver_binary_metadata_free`.
- [ ] `src/c/binary/binary_metadata.cpp` defines none of them. `<new>` and `<chrono>` are no longer included.
- [ ] `include/quiver/binary/binary_metadata.h` has no `// Setters` block. `src/binary/binary_metadata.cpp` defines no `add_dimension` / `add_time_dimension`.
- [ ] `bindings/julia/src/c_api.jl` was regenerated, and the only diff is the removal of the 7 wrappers.
- [ ] `tests/test_c_api_binary_metadata.cpp`:
  - has `VALID_TOML` (or reuses an identical existing constant);
  - the 13 tests in T1 are deleted;
  - T2, T4 and T6–T10 are ported to `from_toml`;
  - `NullArgsErrorMessages` has no builder lines;
  - `BinaryCApiMetadata.*` passes.
- [ ] `tests/test_binary_metadata.cpp`:
  - the `BinaryMetadataAddDimension` section is gone;
  - `MixedTimeAndNonTime` asserts parent indices;
  - the 3 new `BinaryMetadataFromTomlContent.Error*` tests pass.
- [ ] The Julia 1960 test comment names `quiver_binary_metadata_from_element`, not `set_initial_datetime`.
- [ ] The root `CLAUDE.md` row, the `src/CLAUDE.md` Builders/Factories lines and the `src/c/CLAUDE.md` lifecycle block are updated. CRLF is preserved.
- [ ] `CHANGELOG.md` 0.11.0 has the `### Removed` **BREAKING** entry with an *Adapt:* line.
- [ ] The `git grep` in Verification step 6 prints nothing. `scripts/test-all.bat` passes.

## Pitfalls

- **Earlier plans move lines.** Plans 08, 09, 10 and 11 edit `src/binary/binary_metadata.cpp`, `include/quiver/binary/binary_metadata.h` and both binary-metadata test files before this one runs.
  - Find every edit by function or test name. Never replace a whole file.
  - Plan 11 in particular may have added tests to `tests/test_c_api_binary_metadata.cpp` and `tests/test_binary_metadata.cpp`. Keep them. If one of them calls a deleted builder (it should not), port it to `from_toml` the same way as T6.
- **Plan 09 and `add_time_dimension`.** If plan 09 changed `add_time_dimension` or `TimeProperties{..., 0, ...}` on the builder path, the function still goes entirely. Do not delete any new helper plan 09 introduced for `from_toml_content` / `ExpressionAggregate`.
- **Exception types in the new C++ tests.** `ErrorUnknownFrequency` asserts `std::invalid_argument`, because `frequency_from_string` throws that. If an earlier plan rewrapped frequency parsing inside `from_toml_content` into a `std::runtime_error`, assert the type the code now throws. Check with `--gtest_filter=BinaryMetadataFromTomlContent.ErrorUnknownFrequency`.
  - `ErrorDuplicateFrequencies` relies on `from_toml_content` running `validate()`. It does today, at the end. After plan 08 it runs before initial values are computed. Either way a `std::runtime_error` is raised.
  - If the test sees a `std::logic_error` from the initial-value computation instead, an earlier plan broke plan 08's "validate before computing" ordering. Fix the order there, not the test.
- **Julia regeneration diff.** The generator rewrites the whole `c_api.jl` from the current headers. If `git diff` shows hunks beyond the 7 removals, an earlier plan changed a C header without regenerating.
  - Those hunks are still correct, because they reflect the headers. Keep them, and mention them in the commit message.
  - Do not hand-delete the 7 functions as a shortcut if the generator cannot run. The generator needs `julia +1.12.5` through juliaup. Install that channel instead.
  - Stale `@ccall` stubs would not fail the Julia tests: `@ccall` resolves lazily, and no test calls them. That is why Verification step 6 greps for them.
- **Line endings.** `CLAUDE.md`, `src/CLAUDE.md`, `src/c/CLAUDE.md` and `CHANGELOG.md` are CRLF in the working tree. `.cpp`, `.h` and `.jl` are LF (`.gitattributes`). Use the Edit tool. Touch no `.bat` file.
- **Stale native libraries elsewhere.** `bindings/dart/.dart_tool/...` and `bindings/python/.venv/.../libquiver_c.dll` still export the old symbols until those caches rebuild. That is harmless: neither binding declares them.
- **`AllTimeFrequencies` string building.** Start the concatenation with `std::string(...)`. `const char* + const char*` does not compile. Pass `toml.c_str()` to `quiver_binary_metadata_from_toml`.
- **The out-of-range index in `GetDimensionOutOfRange`** is `2`, the dimension count of `VALID_TOML`. A factory-built handle can never have zero dimensions (`validate()` rejects it), so the old "index 0 on an empty handle" case no longer exists.

## Out of scope

- The stored `TimeProperties::initial_value`, `set_initial_value`, and recomputing it after `aggregate()`: plan 09.
- Length checks and missing-key / wrong-type errors in `from_toml_content`, and dropping `from_element`'s TOML round trip: plan 11.
- `quiver_clear_last_error`, the element counters `quiver_element_{has_scalars,has_arrays,scalar_count,array_count}`, and C++ `Element::has_scalars/has_arrays`, together with their CLAUDE.md line at `src/c/CLAUDE.md` ~L49: plan 21.
- Deduplicating the identical TOML literals in `GetNumberOfTimeDimensions` / `TomlRoundTrip`. Not needed for this change.
- The pre-pattern messages in `BinaryMetadata::validate()`, such as `"Number of labels must be positive, got 0"`. They are the documented exception in the root `CLAUDE.md`.
- The getters, `to_toml` and the free helpers of the binary-metadata C API. They are unchanged.
