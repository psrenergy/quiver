# 11 — BinaryMetadata factory: length checks, missing-key errors, no TOML round trip in from_element

**Batch** 2 · **Severity** medium · **Breaking** no. Inputs that used to crash, read out of bounds or load wrong now throw, and some error text changes · **Size** M · **Layers** C++ core (`src/binary/binary_metadata.cpp`); new tests in C++, C API, Lua and Julia (no C API, FFI or binding source changes)
**Depends on** none as a hard dependency. Plans 08 and 09 land first and edit the same function body; see Overlaps · **Overlaps with** 08, 09 (the tail of `from_toml_content`, which this plan moves), 12 (a Julia comment that this plan makes stale), 47/48 (Lua `quiver.metadata` error text and decoder)

---

## Why

`BinaryMetadata` has one construction body, at the end of `from_toml_content`. Every factory goes through it:
- `from_element` serializes its fields to TOML text, then calls `from_toml_content`.
- `from_toml_file` calls it too, on every `open_file(path, 'r')` sidecar read, on `ExpressionFile`, and on `CSVConverter`.
- The C API `quiver_binary_metadata_from_toml` / `_from_element`.
- Julia `Metadata(; ...)` / `from_element` / `from_toml_content`.
- Lua `quiver.metadata{}` / `quiver.metadata_from_toml` / `quiver.metadata_from_element`.

That body has four defects.

**1. It indexes parallel arrays with no length check.** `src/binary/binary_metadata.cpp` (currently ~L316-331):

```cpp
    int64_t time_dim_index = 0;
    int64_t previous_time_dim_index = -1;
    metadata.dimensions.clear();
    for (size_t i = 0; i < dimensions.size(); ++i) {
        bool is_time =
            std::find(time_dimensions.begin(), time_dimensions.end(), dimensions[i]) != time_dimensions.end();
        if (is_time) {
            TimeFrequency freq = frequency_from_string(frequencies[time_dim_index]);
            ...
            metadata.dimensions.push_back({dimensions[i], dimension_sizes[i], std::move(time_props)});
```

Nothing compares `dimension_sizes.size()` with `dimensions.size()`, or `frequencies.size()` with `time_dimensions.size()`. `validate()` runs only after this loop. Reproductions, each with a valid `initial_datetime` so the loop is reached:
- Julia `Quiver.Binary.Metadata(; initial_datetime="2025-01-01T00:00:00", unit="MW", labels=["v"], dimensions=["stage","block"], dimension_sizes=Int64[12])` reads `dimension_sizes[1]` from a 1-element vector.
- Julia `Metadata(; ..., dimensions=["stage"], dimension_sizes=Int64[12], time_dimensions=["stage"])`. `frequencies` defaults to `String[]` (`bindings/julia/src/binary/metadata.jl` ~L36), so this reads `frequencies[0]` from an empty vector.
- Lua `quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW', labels={'v'}, dimensions={'a','b'} }` does the same. `lua_opt_int64_vector` (`src/lua_runner.cpp` ~L1055) returns `{}` for an absent key, so even omitting `dimension_sizes` reads past the end. src/AGENTS.md says "a script is untrusted input".
- A hand-edited `.toml` sidecar with `time_dimensions = ["stage"]` and no `frequencies`, opened with `open_file(path, 'r')`, does the same.

In a Debug (MSVC) build each of these aborts the process with the CRT's "vector subscript out of range". In Release it is undefined behaviour: a heap read past the end.

**2. The loop reads `frequencies` at a running counter, not at the matched position.** Take `dimensions=["a","a"], dimension_sizes=[12,12], time_dimensions=["a"], frequencies=["monthly"]`. The subset and order checks pass, and so would the two new length checks. Then both `"a"` entries match, `time_dim_index` reaches 1, and the code reads `frequencies[1]`. `validate()` would reject the duplicate name, but it runs too late.

**3. TOML array entries of the wrong type are skipped silently, and missing keys are unwrapped bare.** In `from_toml_content` (~L234-282), each array is read like this:

```cpp
    if (auto* arr = tbl["dimensions"].as_array()) {
        for (auto& elem : *arr) {
            if (auto val = elem.value<std::string>()) {
                dimensions.push_back(*val);
            }
        }
    }
```

TOML 1.0 (toml++ v3.4) allows mixed arrays, so the defects are:
- `dimensions = ["row", 2]` loads as the one dimension `["row"]`, and every later size lines up with the wrong dimension.
- `time_dimensions = "stage"` (not an array) loads as "no time dimensions".
- `tbl["unit"].value<std::string>().value()` (~L270, ~L271, ~L282) throws `std::bad_optional_access` ("bad optional access") on a missing or non-string `version`, `unit` or `initial_datetime`, and the message names no key.

**4. `from_element` round-trips through TOML text.** At ~L191-217 it copies its already-typed vectors into a `toml::table`, prints it, and re-parses it with `from_toml_content`. The only purpose is to reuse the body. As a side effect, its errors say `Error building metadata from toml: ...` even though the caller never passed any TOML. `metadata.dimensions.clear()` (~L318) runs on a freshly default-constructed struct.

Principles violated: input validation at an FFI and untrusted-script boundary (the "When NOT to be lazy" case), the root AGENTS.md error patterns (`Error building metadata from toml:` is an ad-hoc format), and "simple over roundabout" (the from_element round trip).

## Constraints and decisions

- **Maintainer decision (binding):** "Shared body as an anonymous-namespace free function in binary_metadata.cpp taking the fields plus the operation name (no header change)." So:
  - `include/quiver/binary/binary_metadata.h` does not change.
  - The body becomes `build_metadata(operation, dimensions, dimension_sizes, time_dimensions, frequencies, initial_datetime, unit, labels, version)` in an anonymous namespace in `src/binary/binary_metadata.cpp`.
  - `from_element` calls it directly, which deletes the TOML round trip.
  - `from_toml_content` calls it after reading the TOML.
- **Root AGENTS.md, "C++ Error Message Patterns":** Pattern 1 is `"Cannot {operation}: {reason}"`, and "Validators thread the calling operation's name through so the `{operation}` is the public method the user called". The operation names are therefore `from_element` and `from_toml_content`, the C++ and Julia names of the two public factories. `from_toml_file` delegates to `from_toml_content` and reports that name.
- **Root AGENTS.md, same section:** "Known exception: the binary/expression subsystem's metadata validation throws descriptive messages (e.g. `"Number of labels must be positive, got 0"`) that predate the pattern; new code should use the three patterns." Every new message here is Pattern 1. `validate()`'s messages stay as they are.
- **Root AGENTS.md, Principles:** "Error Messages: All error messages are defined in the C++/C API layer". Julia and Lua surface these messages unchanged, so no binding code changes.
- **Root AGENTS.md, Design Decisions:** "Binary + expression subsystems are exposed in Julia and Lua only". There are no Dart, Python or JS tests or changes.
- **src/AGENTS.md, Lua section:** "a script is untrusted input". This is why the length checks belong in the C++ core, where every entry point passes.
- **Existing optionality is kept:**
  - `from_element` treats `time_dimensions` and `frequencies` as optional (`get_string_array_opt`) and the other six fields as required. That stays unchanged.
  - For TOML, an absent array keeps reading as empty, as it does today. A missing required array is then reported by the checks that run anyway: `dimension_sizes count (0) does not match dimensions count (2)` for a missing `dimension_sizes`, and `validate()`'s `Number of labels must be positive, got 0` for a missing `labels`.
  - An absent *string* key had no working default (it threw "bad optional access"), so it now throws naming the key.
- **The two verifiers disagreed.** The policy verifier wanted to keep the from_element TOML round trip and fix only `from_toml_content`. The maintainer note overrides that. The facts verifier's corrected proposal (anonymous-namespace function, matched-position frequency index, per-key typed reads) is what this plan implements.

Rejected alternatives:
- A private static `from_fields(...)` member (the original finding). It changes the public header, and the maintainer ruled that out.
- Rejecting duplicate dimension names before the loop to close defect 2. That duplicates `validate()`'s uniqueness check. Indexing `frequencies` by the matched position is a one-line change and makes the loop correct for any input.
- A `required` flag on the TOML array reader, so that a missing `labels` says `missing key 'labels'`. It adds a boolean parameter for an outcome the count and `validate()` checks already give, and it changes today's absent-array behaviour.
- Switching `value<T>()` to `value_exact<T>()`. That would newly reject `dimension_sizes = [4.0]` and `[true]`, which toml++'s permissive `value<int64_t>()` accepts today. That strictness change is separate from this fix.
- Inlining the eight reads into the `build_metadata(...)` call arguments. C++ leaves argument evaluation order unspecified, so which missing key is reported first would vary by compiler. Keep the named locals in today's order.

## Changes

Only one source file changes: `src/binary/binary_metadata.cpp`. Tests and docs follow in their own sections.

**Before you start:** run `git log --oneline -5 -- src/binary/binary_metadata.cpp` and read `BinaryMetadata::from_toml_content` in full. Plans 08 and 09 run before this one and edit its tail, from the `// Create and populate BinaryMetadata` comment down to `return metadata;`:
- 08 validates time-dimension metadata before the initial values are computed.
- 09 moves the initial-value computation into one `BinaryMetadata` function.

The code below is written against HEAD `58dfe7a`. **Wherever it differs from your checkout in that tail, carry the checkout's tail over verbatim.** Then apply only the edits marked (c) and (d) in step 2. Everything before `// Create and populate BinaryMetadata` (the key reads and the two subset/order checks) is untouched by 08 and 09.

### Step 1: add `<type_traits>`

In `src/binary/binary_metadata.cpp`, current includes (~L9-14):

```cpp
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <toml++/toml.hpp>
```

Add `#include <type_traits>` after `<stdexcept>`. Keep `<sstream>`, because `to_toml()` still uses `std::ostringstream`.

### Step 2: add an anonymous namespace inside `namespace quiver` with the three helpers

Current code (~L103-106):

```cpp
namespace quiver {

BinaryMetadata::BinaryMetadata() = default;
BinaryMetadata::~BinaryMetadata() = default;
```

New code. Insert the block between `namespace quiver {` and `BinaryMetadata::BinaryMetadata() = default;`. It sits inside `namespace quiver` so that the moved body keeps its unqualified `BinaryMetadata`, `TimeFrequency`, `TimeProperties` and `frequency_from_string`. It can still call the file-top `compute_time_dimension_initial_values` without qualification, exactly as `from_toml_content` does today.

```cpp
namespace quiver {

namespace {

// Reads a required TOML string, naming `key` when it is absent or not a string.
std::string read_toml_string(const toml::table& tbl, const std::string& key) {
    const toml::node* node = tbl.get(key);
    if (!node) {
        throw std::runtime_error("Cannot from_toml_content: missing key '" + key + "'");
    }
    auto value = node->value<std::string>();
    if (!value) {
        throw std::runtime_error("Cannot from_toml_content: key '" + key + "' must be a string");
    }
    return *value;
}

// Reads a TOML array of T. An absent key reads as empty: build_metadata's count checks and
// validate() then reject a missing required array. A non-array value or an entry of another type
// throws instead of being skipped, which used to shift every later dimension onto the wrong size.
template <typename T>
std::vector<T> read_toml_array(const toml::table& tbl, const std::string& key) {
    const toml::node* node = tbl.get(key);
    if (!node) {
        return {};
    }
    const toml::array* arr = node->as_array();
    if (!arr) {
        throw std::runtime_error("Cannot from_toml_content: key '" + key + "' must be an array");
    }
    std::vector<T> result;
    for (const toml::node& entry : *arr) {
        auto value = entry.value<T>();
        if (!value) {
            constexpr const char* kind = std::is_same_v<T, std::string> ? "strings" : "integers";
            throw std::runtime_error("Cannot from_toml_content: array '" + key + "' must contain " + kind);
        }
        result.push_back(*value);
    }
    return result;
}

// The one body behind from_element and from_toml_content, which differ only in how they read the
// eight fields. `operation` is the public factory the caller used, for the Pattern 1 messages.
BinaryMetadata build_metadata(const std::string& operation,
                              const std::vector<std::string>& dimensions,
                              const std::vector<int64_t>& dimension_sizes,
                              const std::vector<std::string>& time_dimensions,
                              const std::vector<std::string>& frequencies,
                              const std::string& initial_datetime_str,
                              const std::string& unit,
                              const std::vector<std::string>& labels,
                              const std::string& version) {
    // The dimension loop reads dimension_sizes and frequencies in parallel with dimensions and
    // time_dimensions, so their lengths must agree before anything is indexed.
    if (dimension_sizes.size() != dimensions.size()) {
        throw std::runtime_error("Cannot " + operation + ": dimension_sizes count (" +
                                 std::to_string(dimension_sizes.size()) + ") does not match dimensions count (" +
                                 std::to_string(dimensions.size()) + ")");
    }
    if (frequencies.size() != time_dimensions.size()) {
        throw std::runtime_error("Cannot " + operation + ": frequencies count (" + std::to_string(frequencies.size()) +
                                 ") does not match time_dimensions count (" + std::to_string(time_dimensions.size()) +
                                 ")");
    }

    // Validate time_dimensions are a subset of dimensions
    for (const auto& td : time_dimensions) {
        if (std::find(dimensions.begin(), dimensions.end(), td) == dimensions.end()) {
            throw std::runtime_error("Cannot " + operation + ": time dimension '" + td + "' is not in dimensions");
        }
    }

    // Validate time_dimensions are in the same order as dimensions
    size_t last_pos = 0;
    for (const auto& td : time_dimensions) {
        auto it = std::find(dimensions.begin() + last_pos, dimensions.end(), td);
        if (it == dimensions.end()) {
            throw std::runtime_error("Cannot " + operation +
                                     ": time dimensions must appear in the same order as dimensions");
        }
        last_pos = static_cast<size_t>(std::distance(dimensions.begin(), it)) + 1;
    }

    // ---- From here to `return metadata;`: the tail of from_toml_content as it stands in your checkout.
    // ---- Shown as at HEAD 58dfe7a, with edits (c) and (d) applied.

    // Create and populate BinaryMetadata
    BinaryMetadata metadata;
    metadata.unit = unit;
    metadata.labels = labels;
    metadata.version = version;

    std::tm tm{};
    if (!quiver::datetime::parse_iso8601(initial_datetime_str, tm)) {
        throw std::runtime_error("Failed to parse initial_datetime: " + initial_datetime_str);
    }
    metadata.initial_datetime = quiver::datetime::tm_to_time_point(tm);

    // Add dimensions to metadata
    int64_t previous_time_dim_index = -1;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        auto time_it = std::find(time_dimensions.begin(), time_dimensions.end(), dimensions[i]);
        if (time_it != time_dimensions.end()) {
            // The frequency at the matched position, not a running count: a repeated dimension name
            // matches twice, and a count would read past `frequencies` before validate() rejects it.
            auto time_pos = static_cast<size_t>(std::distance(time_dimensions.begin(), time_it));
            TimeFrequency freq = frequency_from_string(frequencies[time_pos]);
            TimeProperties time_props{freq, 0, previous_time_dim_index};
            metadata.dimensions.push_back({dimensions[i], dimension_sizes[i], std::move(time_props)});
            previous_time_dim_index = i;
        } else {
            metadata.dimensions.push_back({dimensions[i], dimension_sizes[i], std::nullopt});
        }
    }

    // Compute and set initial values for time dimensions
    std::vector<int64_t> initial_values =
        compute_time_dimension_initial_values(metadata.dimensions, metadata.initial_datetime);
    int64_t time_dim_index = 0;
    for (auto& dim : metadata.dimensions) {
        if (dim.is_time_dimension()) {
            dim.time->set_initial_value(initial_values[time_dim_index]);
            time_dim_index++;
        }
    }

    metadata.validate();
    return metadata;
}

}  // namespace

BinaryMetadata::BinaryMetadata() = default;
BinaryMetadata::~BinaryMetadata() = default;
```

Delete the two `// ----` marker lines after pasting. They are instructions, not code.

The edits inside the moved code, relative to today's `from_toml_content`:
- **(a)** Add the two count checks at the top. They are new.
- **(b)** Change the two `"Error building metadata from toml: ..."` messages to `"Cannot " + operation + ": ..."`, which is Pattern 1 and names the real factory.
- **(c)** Rewrite the dimension loop:
  - Replace `bool is_time = std::find(...) != time_dimensions.end();` and `frequencies[time_dim_index]` with the matched position (`time_it` / `time_pos`).
  - Delete the loop's `time_dim_index++;` and the `int64_t time_dim_index = 0;` declared before the loop.
  - Delete the dead `metadata.dimensions.clear();`.
  - Keep `previous_time_dim_index = i;` exactly as it is.
- **(d)** Today's `time_dim_index = 0;` reset (~L332) becomes a declaration, `int64_t time_dim_index = 0;`, placed right before the initial-value assignment loop. **If plan 09 replaced that assignment loop with a single function call**, no counter is left, so do not add one. Keep 09's call as it is.
- Keep `"Failed to parse initial_datetime: "` unchanged. It is Pattern 3 and out of scope.

### Step 3: make `from_element` call `build_metadata` directly

Current code (~L182-218, the tail of `BinaryMetadata::from_element`; the four lambdas above it stay unchanged):

```cpp
    std::vector<std::string> dimensions = get_string_array("dimensions");
    std::vector<int64_t> dimension_sizes = get_int_array("dimension_sizes");
    std::vector<std::string> time_dimensions = get_string_array_opt("time_dimensions");
    std::vector<std::string> frequencies = get_string_array_opt("frequencies");
    std::string initial_datetime_str = get_string("initial_datetime");
    std::string unit = get_string("unit");
    std::vector<std::string> labels = get_string_array("labels");
    std::string version = get_string("version");

    // Build TOML and delegate
    toml::array dim_arr, size_arr, time_dim_arr, freq_arr, label_arr;
    ...
    std::ostringstream oss;
    oss << tbl;
    return from_toml_content(oss.str());
}
```

New code. Keep the eight locals and delete everything from `// Build TOML and delegate` to the end:

```cpp
    std::vector<std::string> dimensions = get_string_array("dimensions");
    std::vector<int64_t> dimension_sizes = get_int_array("dimension_sizes");
    std::vector<std::string> time_dimensions = get_string_array_opt("time_dimensions");
    std::vector<std::string> frequencies = get_string_array_opt("frequencies");
    std::string initial_datetime_str = get_string("initial_datetime");
    std::string unit = get_string("unit");
    std::vector<std::string> labels = get_string_array("labels");
    std::string version = get_string("version");

    return build_metadata("from_element", dimensions, dimension_sizes, time_dimensions, frequencies,
                          initial_datetime_str, unit, labels, version);
}
```

### Step 4: replace the body of `from_toml_content`

Current code: the whole body of `BinaryMetadata::from_toml_content` (~L230-346), which starts with

```cpp
BinaryMetadata BinaryMetadata::from_toml_content(const std::string& content) {
    // Parse toml content
    toml::table tbl = toml::parse(content);

    std::vector<std::string> dimensions;
    if (auto* arr = tbl["dimensions"].as_array()) {
```

and ends with `metadata.validate();\n    return metadata;\n}`. Step 2 has already moved its tail into `build_metadata`, so you can delete the tail here.

New code:

```cpp
BinaryMetadata BinaryMetadata::from_toml_content(const std::string& content) {
    toml::table tbl = toml::parse(content);

    std::vector<std::string> dimensions = read_toml_array<std::string>(tbl, "dimensions");
    std::vector<int64_t> dimension_sizes = read_toml_array<int64_t>(tbl, "dimension_sizes");
    std::vector<std::string> time_dimensions = read_toml_array<std::string>(tbl, "time_dimensions");
    std::vector<std::string> frequencies = read_toml_array<std::string>(tbl, "frequencies");
    std::string initial_datetime_str = read_toml_string(tbl, "initial_datetime");
    std::string unit = read_toml_string(tbl, "unit");
    std::vector<std::string> labels = read_toml_array<std::string>(tbl, "labels");
    std::string version = read_toml_string(tbl, "version");

    return build_metadata("from_toml_content", dimensions, dimension_sizes, time_dimensions, frequencies,
                          initial_datetime_str, unit, labels, version);
}
```

`BinaryMetadata::from_toml_file` (~L220-228) stays unchanged. It still calls `from_toml_content`.

### Step 5: other files

- **Header:** `include/quiver/binary/binary_metadata.h` does not change (maintainer decision).
- **C API** (`src/c/binary/binary_metadata.cpp`): no change. `quiver_binary_metadata_from_toml` / `_from_element` already catch `std::exception` and forward `e.what()`.
- **FFI declarations:** no C header changes, so there is no Julia `c_api.jl` regeneration. Python `_c_api.py`, JS `loader.ts` and Dart `bindings.dart` do not bind the binary subsystem.
- **Julia** (`bindings/julia/src/binary/metadata.jl`): no change. `Metadata(; ...)` → `from_element` → C API.
- **Lua** (`src/lua_runner.cpp`): no change. `build_metadata_from_lua` → `BinaryMetadata::from_element`, and `metadata_from_toml` → `from_toml_content`.
- **Dart / Python / JS:** not applicable (binary subsystem not exposed, root Design Decision).

## Tests

Every new test below fails before the fix. In a Debug build, the count-mismatch and duplicate cases **abort the test process** with the CRT's "vector subscript out of range" rather than failing cleanly. To see the failure, run only the targeted `--gtest_filter`, never the full suite.

### C++ core: `tests/test_binary_metadata.cpp`

1. **Includes.** Add `#include <sstream>` and `#include <string>` after `#include <quiver/element.h>` (~L6).

2. **Helpers.** Add these after `make_valid_element()` (~L38):

```cpp
// make_valid_toml() with the line assigning `key` removed.
static std::string valid_toml_without(const std::string& key) {
    std::istringstream in(make_valid_toml());
    std::string out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind(key + " =", 0) != 0) {
            out += line + "\n";
        }
    }
    return out;
}

// Requires `fn` to throw std::runtime_error whose message contains `substring`.
template <typename Fn>
static void expect_runtime_error(Fn fn, const std::string& substring) {
    try {
        fn();
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(substring), std::string::npos) << e.what();
        return;
    }
    ADD_FAILURE() << "expected std::runtime_error containing: " << substring;
}
```

In `make_valid_toml()`, each line starts with the key followed by ` =`. `"dimensions ="` is therefore not a prefix of `"dimension_sizes = ..."` or `"time_dimensions = ..."`.

3. **Tighten two existing tests.** They pin the new Pattern 1 text, which is also the before/after proof for edit (b).
   - `BinaryMetadataFromTomlContent.ErrorTimeDimensionNotInDimensions` (~L137-149). Old: `EXPECT_THROW(BinaryMetadata::from_toml_content(toml), std::runtime_error);`. New:
     ```cpp
     expect_runtime_error([&] { BinaryMetadata::from_toml_content(toml); },
                          "Cannot from_toml_content: time dimension 'nonexistent' is not in dimensions");
     ```
   - `BinaryMetadataFromTomlContent.ErrorTimeDimensionsOutOfOrder` (~L151-163). Old: same `EXPECT_THROW` line. New:
     ```cpp
     expect_runtime_error([&] { BinaryMetadata::from_toml_content(toml); },
                          "Cannot from_toml_content: time dimensions must appear in the same order as dimensions");
     ```

4. **New `BinaryMetadataFromTomlContent` tests.** Add these after `NoTimeDimensions` (~L180):

```cpp
TEST(BinaryMetadataFromTomlContent, DimensionSizesCountMismatchThrows) {
    std::string toml = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [12]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(toml); },
                         "Cannot from_toml_content: dimension_sizes count (1) does not match dimensions count (2)");
}

TEST(BinaryMetadataFromTomlContent, FrequenciesCountMismatchThrows) {
    std::string toml = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "block"]
frequencies = ["monthly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(toml); },
                         "Cannot from_toml_content: frequencies count (1) does not match time_dimensions count (2)");
}

// Both counts agree, but a repeated dimension name matches the one time dimension twice. Indexing
// frequencies by a running count read frequencies[1]; now validate() reports the duplicate.
TEST(BinaryMetadataFromTomlContent, RepeatedTimeDimensionNameStaysInBounds) {
    std::string toml = R"(
version = "1"
dimensions = ["a", "a"]
dimension_sizes = [12, 12]
time_dimensions = ["a"]
frequencies = ["monthly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(toml); }, "must be unique");
}

TEST(BinaryMetadataFromTomlContent, WrongTypedArrayEntryThrows) {
    std::string strings = R"(
version = "1"
dimensions = ["row", 2]
dimension_sizes = [3, 2]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(strings); },
                         "Cannot from_toml_content: array 'dimensions' must contain strings");

    std::string integers = R"(
version = "1"
dimensions = ["row", "col"]
dimension_sizes = [3, "2"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(integers); },
                         "Cannot from_toml_content: array 'dimension_sizes' must contain integers");
}

TEST(BinaryMetadataFromTomlContent, NonArrayValueThrows) {
    std::string toml = R"(
version = "1"
dimensions = ["stage"]
dimension_sizes = [12]
time_dimensions = "stage"
frequencies = ["monthly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(toml); },
                         "Cannot from_toml_content: key 'time_dimensions' must be an array");
}

TEST(BinaryMetadataFromTomlContent, MissingStringKeyNamesTheKey) {
    for (const std::string key : {"version", "initial_datetime", "unit"}) {
        SCOPED_TRACE(key);
        expect_runtime_error([&] { BinaryMetadata::from_toml_content(valid_toml_without(key)); },
                             "Cannot from_toml_content: missing key '" + key + "'");
    }
}

TEST(BinaryMetadataFromTomlContent, NonStringKeyNamesTheKey) {
    std::string toml = valid_toml_without("version") + "version = 1\n";
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(toml); },
                         "Cannot from_toml_content: key 'version' must be a string");
}

// An absent array reads as empty: the optional time keys load, and a missing required array is
// reported by the count check or validate().
TEST(BinaryMetadataFromTomlContent, AbsentArrayReadsAsEmpty) {
    std::string no_time_keys = R"(
version = "1"
dimensions = ["row", "col"]
dimension_sizes = [3, 2]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    auto md = BinaryMetadata::from_toml_content(no_time_keys);
    EXPECT_EQ(md.number_of_time_dimensions(), 0);

    expect_runtime_error([&] { BinaryMetadata::from_toml_content(valid_toml_without("dimension_sizes")); },
                         "Cannot from_toml_content: dimension_sizes count (0) does not match dimensions count (2)");
    expect_runtime_error([&] { BinaryMetadata::from_toml_content(valid_toml_without("labels")); },
                         "Number of labels must be positive, got 0");
}
```

Notes:
- `NonStringKeyNamesTheKey` appends the line at the end. TOML allows keys in any order, and `make_valid_toml()` has no table headers, so the key stays top-level.
- In `RepeatedTimeDimensionNameStaysInBounds`, `"must be unique"` matches both `Dimension names must be unique, duplicate: 'a'` (today's `validate()` order) and `Time dimension frequencies must be unique. Duplicate: monthly`. The second is what fires first if plan 08 moved time-dimension validation earlier. The test's job is "no out-of-bounds read, a clean `std::runtime_error`". If a different `std::runtime_error` fires first in your checkout, keep the lambda and change only the substring.

5. **New `BinaryMetadataFromElement` tests.** Add these after `DimensionSizesMustBeIntegers` (~L347):

```cpp
TEST(BinaryMetadataFromElement, DimensionSizesCountMismatchThrows) {
    expect_runtime_error(
        [] {
            BinaryMetadata::from_element(Element()
                                             .set("version", "1")
                                             .set("initial_datetime", "2025-01-01T00:00:00")
                                             .set("unit", "MW")
                                             .set("dimensions", {"stage", "block"})
                                             .set("dimension_sizes", {12})
                                             .set("labels", {"val"}));
        },
        "Cannot from_element: dimension_sizes count (1) does not match dimensions count (2)");
}

TEST(BinaryMetadataFromElement, FrequenciesCountMismatchThrows) {
    expect_runtime_error(
        [] {
            BinaryMetadata::from_element(Element()
                                             .set("version", "1")
                                             .set("initial_datetime", "2025-01-01T00:00:00")
                                             .set("unit", "MW")
                                             .set("dimensions", {"stage"})
                                             .set("dimension_sizes", {12})
                                             .set("time_dimensions", {"stage"})
                                             .set("labels", {"val"}));
        },
        "Cannot from_element: frequencies count (0) does not match time_dimensions count (1)");
}

// from_element used to round-trip through TOML, so its errors said "Error building metadata from toml".
TEST(BinaryMetadataFromElement, ErrorsNameFromElement) {
    expect_runtime_error(
        [] {
            BinaryMetadata::from_element(Element()
                                             .set("version", "1")
                                             .set("initial_datetime", "2025-01-01T00:00:00")
                                             .set("unit", "MW")
                                             .set("dimensions", {"stage"})
                                             .set("dimension_sizes", {12})
                                             .set("time_dimensions", {"month"})
                                             .set("frequencies", {"monthly"})
                                             .set("labels", {"val"}));
        },
        "Cannot from_element: time dimension 'month' is not in dimensions");
}
```

`.set("dimension_sizes", {12})` resolves to the `std::initializer_list<int64_t>` overload, the same way the existing `{3}` calls in this file do. No existing test in this file needs any change besides the two in item 3. `RoundTripEquivalenceWithToml` (~L234) keeps checking that the two factories agree, and now it also covers the direct path.

### C API: `tests/test_c_api_binary_metadata.cpp`

Add after `FromElementMissingRequiredField` (~L637). These use only `from_element` / `from_toml`, not the builders that plan 12 deletes.

```cpp
TEST(BinaryCApiMetadata, FromElementDimensionSizesCountMismatch) {
    quiver_element_t* el = nullptr;
    ASSERT_EQ(quiver_element_create(&el), QUIVER_OK);
    quiver_element_set_string(el, "version", "1");
    quiver_element_set_string(el, "initial_datetime", "2025-01-01T00:00:00");
    quiver_element_set_string(el, "unit", "MW");
    const char* dims[] = {"row", "col"};
    quiver_element_set_array_string(el, "dimensions", dims, 2, nullptr);
    int64_t sizes[] = {3};
    quiver_element_set_array_integer(el, "dimension_sizes", sizes, 1, nullptr);
    const char* labels[] = {"val"};
    quiver_element_set_array_string(el, "labels", labels, 1, nullptr);

    quiver_binary_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_binary_metadata_from_element(el, &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
    EXPECT_STREQ(quiver_get_last_error(),
                 "Cannot from_element: dimension_sizes count (1) does not match dimensions count (2)");

    quiver_element_destroy(el);
}

TEST(BinaryCApiMetadata, FromTomlFrequenciesCountMismatch) {
    const char* toml = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "block"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    quiver_binary_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
    EXPECT_STREQ(quiver_get_last_error(),
                 "Cannot from_toml_content: frequencies count (0) does not match time_dimensions count (2)");
}

TEST(BinaryCApiMetadata, FromTomlMissingKeyNamesTheKey) {
    const char* toml = R"(
version = "1"
dimensions = ["row"]
dimension_sizes = [3]
initial_datetime = "2025-01-01T00:00:00"
labels = ["val"]
)";
    quiver_binary_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot from_toml_content: missing key 'unit'");
}

TEST(BinaryCApiMetadata, FromTomlWrongTypedEntry) {
    const char* toml = R"(
version = "1"
dimensions = ["row", "col"]
dimension_sizes = [3, "2"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    quiver_binary_metadata_t* md = nullptr;
    EXPECT_EQ(quiver_binary_metadata_from_toml(toml, &md), QUIVER_ERROR);
    EXPECT_EQ(md, nullptr);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot from_toml_content: array 'dimension_sizes' must contain integers");
}
```

Before the fix:
- The first two tests abort in Debug.
- The third fails with `bad optional access`.
- The fourth aborts: the dropped `"2"` leaves `dimension_sizes` one entry short, and the loop reads `dimension_sizes[1]`.

No existing C API test changes. `FromElementMissingRequiredField` still sees `Cannot from_element`, and `InvalidToml` still sees toml++'s parse error.

### Lua: `tests/test_lua_binary.cpp`

Add after `MetadataFromToml` (~L197). The substrings leave out the `Cannot <op>:` prefix on purpose: plans 47 and 48 may later change how Lua names these operations, and these tests should survive that.

```cpp
TEST_F(LuaBinaryTest, MetadataCountMismatchThrows) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    // A script is untrusted input; each of these used to index past dimension_sizes / frequencies.
    expect_lua_error(lua,
                     "quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW', labels={'v'},"
                     " dimensions={'stage', 'block'}, dimension_sizes={12} }\n",
                     "dimension_sizes count (1) does not match dimensions count (2)");
    expect_lua_error(lua,
                     "quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW', labels={'v'},"
                     " dimensions={'stage', 'block'} }\n",
                     "dimension_sizes count (0) does not match dimensions count (2)");
    expect_lua_error(lua,
                     "quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW', labels={'v'},"
                     " dimensions={'stage'}, dimension_sizes={12}, time_dimensions={'stage'} }\n",
                     "frequencies count (0) does not match time_dimensions count (1)");
}

TEST_F(LuaBinaryTest, MetadataFromTomlRejectsWrongTypedEntry) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua, R"lua(
        quiver.metadata_from_toml([[
version = "1"
dimensions = ["row", 2]
dimension_sizes = [3, 2]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
]])
    )lua",
                     "array 'dimensions' must contain strings");
}
```

`expect_lua_error` comes from `tests/test_lua_runner.h`, which this file already includes. The raw-string delimiter `lua` keeps the embedded `)` of the Lua call from ever being read as the end of the literal. Before the fix:
- `MetadataCountMismatchThrows` aborts in Debug.
- `MetadataFromTomlRejectsWrongTypedEntry` fails with "expected script to throw". The script loads a 1-dimension metadata.

### Julia: `bindings/julia/test/test_binary_metadata.jl`

This file is already part of `runtests.jl`, so nothing needs registering.

1. Add after `@testset "from_toml_content error time dimensions out of order"` (~L138-150):

```julia
    @testset "from_toml_content wrong-typed array entry" begin
        toml = """
            version = "1"
            dimensions = ["row", 2]
            dimension_sizes = [3, 2]
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        err = @test_throws Quiver.DatabaseException Quiver.Binary.from_toml_content(toml)
        @test err.value.msg == "Cannot from_toml_content: array 'dimensions' must contain strings"
    end

    @testset "from_toml_content missing unit names the key" begin
        toml = """
            version = "1"
            dimensions = ["row"]
            dimension_sizes = [3]
            initial_datetime = "2025-01-01T00:00:00"
            labels = ["val"]
            """
        err = @test_throws Quiver.DatabaseException Quiver.Binary.from_toml_content(toml)
        @test err.value.msg == "Cannot from_toml_content: missing key 'unit'"
    end
```

2. Add after `@testset "from_element invalid version propagates validation"` (~L393-402):

```julia
    @testset "Metadata dimension_sizes count mismatch" begin
        err = @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            labels = ["val"],
            dimensions = ["stage", "block"],
            dimension_sizes = Int64[12],
        )
        @test err.value.msg == "Cannot from_element: dimension_sizes count (1) does not match dimensions count (2)"
    end

    @testset "Metadata time dimensions without frequencies" begin
        # frequencies defaults to String[]; this used to read frequencies[0] from an empty vector
        err = @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            labels = ["val"],
            dimensions = ["stage"],
            dimension_sizes = Int64[12],
            time_dimensions = ["stage"],
        )
        @test err.value.msg == "Cannot from_element: frequencies count (0) does not match time_dimensions count (1)"
    end
```

`err.value.msg` follows the pattern already used in `test_binary_file.jl` ~L110-118 (`exception = @test_throws ...; exception.value.msg`). `DatabaseException.msg` is `quiver_get_last_error()` unchanged (`bindings/julia/src/exceptions.jl`).

### Dart / Python / JS

None. The binary subsystem is not exposed there (root Design Decision).

## Docs and changelog

**`src/AGENTS.md`, UI-metadata paragraph** (~L168-170). Old:
> explicitly
> not `src/binary/binary_metadata.cpp`'s posture, which throws on a parse error or a bare
> `.value()` unwrap with no test for either.

New:
> explicitly
> not `src/binary/binary_metadata.cpp`'s posture, which throws on a parse error and on a missing
> or wrong-typed key.

**`src/AGENTS.md`, Binary Subsystem list** (~L695). Old:
> `  - Factories: `from_toml_content()`, `from_element()``

New (keep the two-space list indent):
> `  - Factories: `from_toml_content()` (and `from_toml_file()`, which reads the sidecar and calls it), `from_element()`. Both hand their eight fields to one anonymous-namespace `build_metadata(operation, ...)` in `binary_metadata.cpp`. It rejects a `dimension_sizes`/`dimensions` or `frequencies`/`time_dimensions` count mismatch before indexing either, takes each time dimension's frequency from its matched position in `time_dimensions`, and names the calling factory in its Pattern 1 errors. `from_element` does not go through TOML text. In a TOML document, an absent array reads as empty, an absent or non-string `version`/`unit`/`initial_datetime` throws naming the key, and a wrong-typed array entry throws instead of being skipped.`

No other AGENTS.md changes:
- Root `AGENTS.md`'s "Known exception" sentence about binary validation messages is still true.
- The cross-layer binary table rows do not change.
- `src/c/AGENTS.md` and `bindings/julia/AGENTS.md` have no C API or binding change to record.
- `tests/AGENTS.md` lists files, not test names.

Also unchanged: `bindings/js/src/lua-api.ts`, whose `quiver.metadata` example stays valid, and `docs/*.md`, which do not mention binary metadata.

**`CHANGELOG.md`.** Append this as the last bullet of `### Fixed` under `## [0.12.0] — unreleased`, after whatever earlier plans added. Wrap at about 100 columns like the neighbouring entries:

```markdown
- **Binary metadata factories reject mismatched or malformed fields instead of reading out of
  bounds.** `from_element` and `from_toml_content` (C API `quiver_binary_metadata_from_element` /
  `_from_toml`, Julia `Metadata(; ...)` / `from_element` / `from_toml_content`, Lua
  `quiver.metadata{}` / `quiver.metadata_from_toml` / `quiver.metadata_from_element`, and every
  `.toml` sidecar read by `open_file(path, 'r')`) indexed `dimension_sizes` and `frequencies`
  without checking their lengths. So `dimensions = {"a", "b"}, dimension_sizes = {3}`, or
  `time_dimensions` without `frequencies`, read past the end of an array: undefined behaviour,
  and an abort in a Debug build. They now throw `Cannot <op>: dimension_sizes count (1) does not
  match dimensions count (2)` or `Cannot <op>: frequencies count (0) does not match
  time_dimensions count (1)`. A TOML array entry of the wrong type (`dimensions = ["a", 2]`) used
  to be dropped silently, which shifted every later dimension onto the wrong size. It is now
  `Cannot from_toml_content: array 'dimensions' must contain strings`, and a non-array value is
  `key '<k>' must be an array`. A missing or non-string `version`, `unit` or `initial_datetime`
  used to report `bad optional access`. It now names the key: `missing key 'unit'` or `key 'unit'
  must be a string`. The two time-dimension errors are now Pattern 1 and name the factory that was
  called (`Cannot from_element: time dimension 'x' is not in dimensions`). Before, they read
  `Error building metadata from toml: ...`, even from `from_element`.
```

It is not BREAKING. Every input that now throws used to crash, load incorrectly, or throw an unnamed error. The only valid-input change is message text, which the project does not treat as breaking (compare plan 47).

## Verification

Run from the repo root in PowerShell. In Git Bash, use `./build/bin/...` and `cmd //c scripts\\format.bat` for the `.bat` files.

1. Build:
   `cmake --build build --config Debug`
   Expected: it compiles cleanly. MSVC `/W4` may warn C4267/C4244 on `previous_time_dim_index = i;`. That line is unchanged from today, so do not "fix" it here.
2. Targeted C++ core and Lua tests:
   `.\build\bin\quiver_tests.exe --gtest_filter="BinaryMetadata*:LuaBinaryTest.*"`
   These must pass: `BinaryMetadataFromTomlContent.{ErrorTimeDimensionNotInDimensions, ErrorTimeDimensionsOutOfOrder, DimensionSizesCountMismatchThrows, FrequenciesCountMismatchThrows, RepeatedTimeDimensionNameStaysInBounds, WrongTypedArrayEntryThrows, NonArrayValueThrows, MissingStringKeyNamesTheKey, NonStringKeyNamesTheKey, AbsentArrayReadsAsEmpty}`, `BinaryMetadataFromElement.{DimensionSizesCountMismatchThrows, FrequenciesCountMismatchThrows, ErrorsNameFromElement}`, `LuaBinaryTest.{MetadataCountMismatchThrows, MetadataFromTomlRejectsWrongTypedEntry}`, and every pre-existing test in those suites.
3. Targeted C API tests:
   `.\build\bin\quiver_c_tests.exe --gtest_filter="BinaryCApiMetadata.*"`
   These must pass: `FromElementDimensionSizesCountMismatch`, `FromTomlFrequenciesCountMismatch`, `FromTomlMissingKeyNamesTheKey`, `FromTomlWrongTypedEntry`, and all existing ones.
4. The full native suites catch regressions in the ~100 `from_element` call sites (binary file, CSV converter, expression, iteration fixtures):
   `.\build\bin\quiver_tests.exe` then `.\build\bin\quiver_c_tests.exe`. Expected: all pass.
5. Julia:
   `.\bindings\julia\test\test.bat`
   Expected: all pass, including the four new testsets in `test_binary_metadata.jl`. No `c_api.jl` regeneration: no C header changed.
6. Format:
   `.\scripts\format.bat`
   Then `git status` / `git diff --stat`. Only these may change:
   - `src/binary/binary_metadata.cpp`
   - `tests/test_binary_metadata.cpp`
   - `tests/test_c_api_binary_metadata.cpp`
   - `tests/test_lua_binary.cpp`
   - `bindings/julia/test/test_binary_metadata.jl`
   - `src/AGENTS.md`
   - `CHANGELOG.md`

   No `.bat` file should appear.
7. Everything:
   `.\scripts\test-all.bat`
   Expected: all six suites and the CLI smoke test pass. Dart, Python and JS are unaffected, but they run here too.

## Acceptance criteria

- [x] `include/quiver/binary/binary_metadata.h` is byte-identical to before.
- [x] `src/binary/binary_metadata.cpp` has an anonymous namespace inside `namespace quiver` holding `read_toml_string`, `read_toml_array<T>` and `build_metadata(operation, dimensions, dimension_sizes, time_dimensions, frequencies, initial_datetime_str, unit, labels, version)`.
- [x] `build_metadata` checks both counts before any indexing and throws the two Pattern 1 messages quoted above.
- [x] The dimension loop reads `frequencies` at the matched position in `time_dimensions`. `metadata.dimensions.clear()` and the loop's running `time_dim_index` are gone.
- [x] `from_element` builds no `toml::table` and calls `build_metadata("from_element", ...)`.
- [x] `from_toml_content` reads every key through `read_toml_array` / `read_toml_string`. No bare `.value()` unwrap and no skip-on-wrong-type loop is left.
- [x] No `"Error building metadata from toml"` string is left anywhere in `src/`: `git grep "Error building metadata"` returns nothing. *(Checked as `git grep "Error building metadata" -- src include`, which is empty: repo-wide, CHANGELOG.md, this plan and a test comment quote the old text. See Implementation notes.)*
- [x] Whatever plans 08 and 09 put in the tail of `from_toml_content` survives unchanged inside `build_metadata`.
- [x] The new C++, C API, Lua and Julia tests exist and pass, and the two tightened C++ tests pin the new message text.
- [x] The `src/AGENTS.md` edits (two places) and the `CHANGELOG.md` `### Fixed` entry are in.
- [x] `scripts/test-all.bat` is green. *(All six suites pass; the CLI smoke step no longer exists. See Implementation notes.)*

## Pitfalls

- **The tests abort instead of failing before the fix.** In Debug with MSVC, an out-of-range `std::vector::operator[]` triggers the CRT's "vector subscript out of range" and the whole test executable exits. If you want to see a new test fail first, run it alone with `--gtest_filter`.
- **Plans 08 and 09 edited the code you are moving.** Do not paste the HEAD tail from this plan over theirs. Move the checkout's tail verbatim, then apply edits (c) and (d). If 09 left no initial-value assignment loop, `time_dim_index` disappears entirely.
- **Placement of `build_metadata`.** It must be defined before `from_element` in the file. Its anonymous namespace sits inside `namespace quiver` so the moved body compiles unqualified. Do not put it in the file-top anonymous namespace, which is outside `quiver`: every `BinaryMetadata`, `TimeFrequency`, `TimeProperties` and `frequency_from_string` would then need a `quiver::` prefix.
- **Argument evaluation order.** Keep the eight named locals in both factories. Inlining the reads into the call would make "which missing key is reported first" compiler-dependent.
- **`toml::table::get(key)` returns `nullptr` for an absent key.** Use it instead of `tbl[key]`, whose `node_view` hides the difference between absent and wrong type.
- **`value<int64_t>()` is permissive.** It still accepts `true` and `4.0` as integers, deliberately (see Rejected alternatives). Do not switch to `value_exact` in this change.
- **`from_element` no longer passes strings through toml++.** The old round trip escaped malformed UTF-8 bytes, so a label with invalid UTF-8 came back transcoded. Now it is stored byte-for-byte. `to_toml()` still escapes when the sidecar is written, so file round trips are unchanged. No test depends on the old behaviour.
- **The Lua tests pin only the reason, not the `Cannot <op>:` prefix.** Plans 47 and 48 may later change how Lua names `quiver.metadata` errors. The C++, C API and Julia tests pin the full text.
- **The Julia test comment at `test_binary_metadata.jl` ~L460-462** ("go through the C API's `quiver_binary_metadata_set_initial_datetime`") is already wrong, and plan 12 owns that fix. Do not touch it here. Note for plan 12: after this plan, the path is `Metadata` → `from_element` → `build_metadata`. It is **not** `from_element` → `from_toml_content`.
- **`scripts/format.bat` runs JuliaFormatter**, which may rewrap the new Julia testsets. That is fine; commit its output. Do not open or save any `.bat` file with a Unix tool (they are CRLF).

## Out of scope

- Deleting `add_dimension` / `add_time_dimension` and the C API builder family, and fixing the stale Julia comment at `test_binary_metadata.jl` ~L460-462: **plan 12**.
- Where and how `initial_value` is computed, and validating time metadata before computing it: **plans 08 and 09**.
- Rejecting unknown keys or wrong-typed values in the Lua `quiver.metadata{}` table (`build_metadata_from_lua`): **plan 48**. Lua converter errors naming the Lua-visible method: **plan 47**.
- Making `Failed to parse initial_datetime: <s>` name the factory, and making `from_toml_file` errors name the sidecar path: not planned, and deliberately left alone here.
- Rewriting `validate()`'s pre-pattern messages: covered by root AGENTS.md's documented "Known exception".
- Stricter TOML typing (`value_exact`): not planned.

## Implementation notes

Implemented on `rs/plan11` at HEAD `946022d` (plan 10's merge). Plans 06 to 10 landed on master while this plan was being verified, so 08 and 09 had already reshaped the tail that Step 2 moves. `compute_time_dimension_initial_values` and the initial-value assignment loop are gone, and the tail ends `metadata.validate(); metadata.derive_initial_values(); return metadata;`. As Step 2 instructs, that tail was carried into `build_metadata` verbatim, together with 09's `// initial_value: derive_initial_values()` comment. Only edit (c) applied. Edit (d) had nothing left to change, because no counter remains. A read-only verification pass, by me and 3 adversarial agents, matched every other excerpt, symbol, signature and test anchor before any edit. No C API, FFI, binding or Lua source changed, so no generator was run.

**Red/green.** Before the fix, each new or tightened gtest was run alone with `--gtest_filter=<one>`:
- **Clean FAIL (9):**
  - `ErrorTimeDimensionNotInDimensions`, `ErrorTimeDimensionsOutOfOrder` and `ErrorsNameFromElement`: the message was `Error building metadata from toml: ...`.
  - `NonArrayValueThrows`: no throw.
  - `MissingStringKeyNamesTheKey`, `NonStringKeyNamesTheKey` and C API `FromTomlMissingKeyNamesTheKey`: `std::bad_optional_access`, which MSVC spells `Bad optional access`.
  - Lua `MetadataFromTomlRejectsWrongTypedEntry`: `expected script to throw`.
- **ABORT (10):** `vector(1939) : Assertion failed: vector subscript out of range`, with no gtest summary line.
  - `DimensionSizesCountMismatchThrows` and `FrequenciesCountMismatchThrows`, in both C++ suites.
  - `RepeatedTimeDimensionNameStaysInBounds`.
  - `WrongTypedArrayEntryThrows`: its first half had already failed with "does not throw".
  - `AbsentArrayReadsAsEmpty`.
  - C API `FromElementDimensionSizesCountMismatch`, `FromTomlFrequenciesCountMismatch` and `FromTomlWrongTypedEntry`.
  - Lua `MetadataCountMismatchThrows`.
- **Julia:** `test.bat test_binary_metadata.jl` failed at the first new testset (`from_toml_content wrong-typed array entry`: No exception thrown), and failfast stopped the run there. The two `Metadata` count-mismatch testsets were deliberately not run before the fix. Their out-of-bounds read happens inside the Debug `libquiver.dll` hosted by Julia, and without gtest nothing redirects the CRT assertion dialog, so the run would hang. The C++ and C API runs on the same `from_element` path are their red evidence.

After the fix:
- All 19 gtests pass.
- `BinaryMetadata*:LuaBinaryTest.*`: 117/117.
- `BinaryCApiMetadata.*`: 38/38.
- `quiver_tests`: 1349/1349.
- `quiver_c_tests`: 573/573.
- Julia: 1482/1482 (Binary Metadata 161/161).
- `scripts/test-all.bat`: green, exit 0. All six suites PASS: C++ 1349, C API 573, Julia 1482, Dart 426, JS 212, Python 309. There is no CLI smoke step any more: `01e78d7` (on master before plan 06) removed it from `test-all.bat`, so the pre-existing step 7 failure (plan 65) no longer shows up. Root `AGENTS.md` still says `test-all.bat` runs "the six suites below plus a `quiver_cli` smoke test", and `tests/AGENTS.md` still lists a step 7. That is out of scope here; it is for plan 65/82 or whoever owns `01e78d7`.

**Deviations:**
1. **gmock instead of the helper.** The tests use `EXPECT_THAT(fn, ThrowsMessage<std::runtime_error>(HasSubstr(...)))` (gmock is already linked into `quiver_tests`) instead of the plan's hand-written `expect_runtime_error`. It is one helper fewer, and it is also more correct for the red run. The helper caught only `runtime_error`, so the pre-fix `bad_optional_access` would have escaped and ended the test body. `ThrowsMessage` reports it as a mismatch, and the `SCOPED_TRACE` loop keeps going. The C API tests keep `EXPECT_STREQ`, because `quiver_c_tests` does not link gmock.
2. **`RepeatedTimeDimensionNameStaysInBounds` pins the full message**, `Dimension names must be unique, duplicate: 'a'`. The plan's note says plan 08 would make the frequency-uniqueness message fire first, and that is wrong: 08 moved `validate()` as a whole, and `validate()` checks name uniqueness before it calls `validate_time_dimension_metadata()`.
3. **CHANGELOG.** The entry is the last bullet of `### Fixed` under `## [0.12.4] — unreleased`. The plan said `[0.12.0]`, but the manifests are at 0.12.4 (#315), so no manifest bump either. Two changes to the plan's text:
   - It says "a bare `bad_optional_access`", because MSVC spells the message `Bad optional access`.
   - It adds one sentence: the count checks also reject **surplus** entries (more sizes than dimensions, or frequencies beyond `time_dimensions`), which used to be ignored silently. The plan's claim that "every input that now throws used to crash, load incorrectly, or throw an unnamed error" misses this one case. No caller in the repo produces it, so the change stays non-BREAKING.
4. **Docs beyond the plan's two `src/AGENTS.md` edits.**
   - The UI-metadata sentence says that an absent array still reads as empty. The plan's "missing or wrong-typed key" would have implied that an absent array throws.
   - Three more phrases named `from_toml_content` as the caller of `validate()` / `derive_initial_values()`. That stopped being complete once `from_element` no longer round-trips through it. The phrases are in the "Initial values" bullet and the "Time Coordinates" paragraph of `src/AGENTS.md` (both added by 08/09) and in the root `AGENTS.md` design decision on the stored `initial_value`. All three now name `build_metadata`.
   - For the same reason, the Julia section header `# Time dimension size validation (via from_toml_content)` in `test_binary_metadata.jl` became `(via Metadata / from_element)`.
   - The post-implementation review workflow found these: 3 lenses, each finding adversarially verified, 4 confirmed and 4 refuted.
5. **The acceptance grep is scoped:** `git grep "Error building metadata" -- src include` is empty. Repo-wide it can never be empty, because CHANGELOG.md, this plan and the `ErrorsNameFromElement` comment all quote the old text.
6. **`scripts/format.bat` first failed at its last step** with `bun: command not found: biome`, because `bindings/js/node_modules` had never been installed. `bun install`, whose output is gitignored, fixed it, and the re-run passed. Biome then rewrote the CRLF working-tree copies of 42 JS files to LF with no content change (`git diff --ignore-cr-at-eol` was empty). Those files were restored with `git checkout -- bindings/js`. No `.bat` file was touched.

**For later plans:**
- **Plan 12:**
  - The Julia builder path is now `Metadata` → `from_element` → `build_metadata`. It is **not** `from_element` → `from_toml_content`.
  - The comment on `derive_initial_values()` in `include/quiver/binary/binary_metadata.h` ("every producer of metadata (from_toml_content, ExpressionAggregate) calls this") is now incomplete in the same way. It was left alone here because this plan keeps that header byte-identical. Plan 12 edits the header anyway.
- **Plans 47/48:** the two Lua tests pin only the reason text, not the `Cannot <op>:` prefix.
- **Future-format sidecars:** a sidecar with `version != "1"` and different key shapes now fails on its structure (`key 'labels' must be an array`) before `validate()`'s version check runs. No v2 format exists. If forward compatibility ever matters, check `version` first in `from_toml_content`.
- **UTF-8:** `from_element` now stores strings byte-for-byte, where the old TOML round trip rewrote invalid UTF-8 bytes as `\u00XX`. `to_toml()` still escapes on write, so file round trips are unchanged.
