# Phase 1: C++ Core - Pattern Map

**Mapped:** 2026-05-06
**Files analyzed:** 11 (7 new, 4 edited)
**Analogs found:** 11 / 11 (every new file has at least a partial in-tree analog; one capability — operator overloads on a value type — has no in-tree analog and is flagged separately)

## File Classification

| New / Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---------------------|------|-----------|----------------|---------------|
| `include/quiver/expr/expression.h` | value-type header (Rule of Zero) | request-response (in-process) | `include/quiver/element.h` | role-match (no operator-overload analog) |
| `include/quiver/expr/node.h` | virtual hierarchy header | engine-internal | (no in-tree analog — first virtual public hierarchy) | role-only (use generic export pattern) |
| `include/quiver/binary/iteration.h` | free-function header in `quiver::binary` | pure (metadata-only) | `include/quiver/binary/time_constants.h` + `include/quiver/binary/time_properties.h` | role-match (sibling header, same namespace) |
| `src/expr/expression.cpp` | engine impl + free-function operators | streaming (row-by-row file I/O) | `src/binary/csv_converter.cpp` (file-to-file streamer); `src/element.cpp` (value-type impl) | partial (engine logic must be new; copy I/O loop shape from csv_converter) |
| `src/expr/nodes.cpp` | virtual hierarchy impl + validation | request-response | `src/binary/binary_metadata.cpp` (validation pattern); `src/database_create.cpp` (Pattern-1 throws) | role-match |
| `src/binary/iteration.cpp` | free-function impl (refactor lift) | pure | `src/binary/binary_file.cpp:245-348` (current `next_dimensions` + `dimension_sizes_at_values`) | exact (lift verbatim into free functions) |
| `tests/test_expression.cpp` | GoogleTest fixture | unit | `tests/test_binary_file.cpp` (lines 1-200) | exact |
| `include/quiver/binary/binary_file.h` (EDIT) | extend public API | request-response | existing `read`/`write` declarations on lines 37-38 | exact |
| `src/binary/binary_file.cpp` (EDIT) | implement new overloads | streaming | existing `read`/`write` impls on lines 106-142, `calculate_file_position` on lines 144-158 | exact |
| `src/CMakeLists.txt` (EDIT) | build wiring | n/a | existing `set(QUIVER_SOURCES …)` block (lines 1-28) | exact |
| `tests/CMakeLists.txt` (EDIT) | test registration | n/a | existing `add_executable(quiver_tests …)` block (lines 3-25) | exact |

## Pattern Assignments

---

### `include/quiver/expr/expression.h` (value-type header)

**Analog:** `include/quiver/element.h`

**Header guard + include layout** (`include/quiver/element.h` lines 1-11):
```cpp
// include/quiver/element.h
#ifndef QUIVER_ELEMENT_H
#define QUIVER_ELEMENT_H

#include "export.h"
#include "value.h"

#include <map>
#include <string>
#include <vector>

namespace quiver {
```

**Class declaration with `QUIVER_API`, default ctor, copy/move (Rule of Zero implicit)** (`include/quiver/element.h` lines 13-46):
```cpp
// include/quiver/element.h
class QUIVER_API Element {
public:
    Element() = default;

    // Scalars
    Element& set(const std::string& name, const char* value);
    Element& set(const std::string& name, int64_t value);
    // ... fluent setters that return Element& ...

    // Accessors
    const std::map<std::string, Value>& scalars() const;
    const std::map<std::string, std::vector<Value>>& arrays() const;

    bool has_scalars() const;

    void clear();

    std::string to_string() const;

private:
    std::map<std::string, Value> scalars_;
    std::map<std::string, std::vector<Value>> arrays_;
};
```

**Closing namespace + guard** (`include/quiver/element.h` lines 48-50):
```cpp
}  // namespace quiver

#endif  // QUIVER_ELEMENT_H
```

**Replication notes:**
- Copy header guard format (`QUIVER_<NAME>_H`).
- Use `class QUIVER_API Expression { … };` — `Element` already shows the export macro placement.
- Public members use `lower_case`; private members get trailing `_` (here `node_`).
- Rule of Zero — do NOT declare `~Expression()`, `Expression(const Expression&)`, etc. The compiler-generated members are correct because the only member is `std::shared_ptr<Node>` (already copyable + movable).
- File ends with `}  // namespace quiver` then `#endif  // …` on its own line — exact format.

**Divergence notes:**
- `Expression` lives in nested namespace `quiver::expr` (not bare `quiver`). Open with `namespace quiver::expr {` or nested `namespace quiver { namespace expr { … } }` — the project uses concatenated form (see `include/quiver/binary/time_constants.h:4` `namespace quiver::time {`). Match that style.
- `Expression(const BinaryFile&)` is **non-explicit** by design (CORE-01: implicit conversion). This is the only place in the public API where an implicit conversion is intentional. Do NOT mark it `explicit` — diverges from the otherwise-uniform `explicit` pattern (e.g., `Database(const std::string&, …)` is explicit, see `database.h:20`).
- Free-function operator overloads (`operator+(const Expression&, const Expression&)` and the scalar variants) live in this header at namespace scope below the class. **No in-tree analog** — sol2 metamethods in `src/lua_runner.cpp` are Lua-side, not free-function operators on a C++ value type. The planner should write these from scratch following the locked signatures in RESEARCH.md §"Operator overloads".
- Include `quiver/binary/binary_file.h` at the top (needed for the `BinaryFile&` ctor parameter); also forward-declare `Node` if you want to avoid pulling in `node.h` from this header.

---

### `include/quiver/expr/node.h` (virtual hierarchy header)

**Analog:** No exact in-tree analog. The closest patterns are:
- `include/quiver/binary/binary_file.h` (a single class with `QUIVER_API` and `~BinaryFile()` declared) — for the export and destructor declaration style.
- `include/quiver/binary/dimension.h` + `include/quiver/binary/time_properties.h` — for the simpler-struct pattern in the same `include/quiver/binary/` subdirectory; `expr/node.h` lives in a sibling subdirectory so the same layout fits.

**Export + destructor pattern (`include/quiver/binary/binary_file.h` lines 17-30):**
```cpp
// include/quiver/binary/binary_file.h
class QUIVER_API BinaryFile {
public:
    explicit BinaryFile(const std::string& file_path,
                        const BinaryMetadata& metadata,
                        std::unique_ptr<std::iostream> io);
    ~BinaryFile();

    // Non-copyable
    BinaryFile(const BinaryFile&) = delete;
    BinaryFile& operator=(const BinaryFile&) = delete;

    // Movable
    BinaryFile(BinaryFile&& other) noexcept;
    BinaryFile& operator=(BinaryFile&& other) noexcept;
```

**Replication notes:**
- Apply `QUIVER_API` to **every class** in the hierarchy: `Node`, `FileNode`, `ScalarNode`, `BinaryOpNode`. (Risk register item: missing `QUIVER_API` on a virtual base causes "missing vtable" link errors on Windows shared builds.)
- `Node` has `virtual ~Node() = default;` declared inline (only thing in the body; no separate impl needed). This matches the project's "Rule of Zero where possible" preference even on virtual bases — only the destructor needs the `virtual` keyword to make the type polymorphic.
- Pure-virtual signatures use `= 0`: `virtual BinaryMetadata metadata() const = 0;` and `virtual void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const = 0;`. Both are `const` because nodes are logically immutable.
- The subclasses keep their own state (path, value, etc.) and use trailing `_` for private members (`path_`, `meta_`, `file_`, `value_`, `op_`, `lhs_`, `rhs_`, `broadcast_meta_`, `lhs_buf_`, `rhs_buf_`).
- Use `mutable` on cache members that need to be modified inside `const` methods (`mutable std::unique_ptr<BinaryFile> file_;`, `mutable std::vector<double> lhs_buf_;`, etc.). This is a documented pattern in C++20; no codebase analog uses it yet, but it's idiomatic and required by the design (RESEARCH.md §"Common Pitfalls" Pitfall 4).

**Divergence notes:**
- This is the **first public virtual hierarchy** in the codebase. Existing classes are all either Pimpl (`Database`, `BinaryFile`, `LuaRunner`, `CSVConverter`) or plain value types (`Element`, `Row`, `Result`, `Migration`, `BinaryMetadata`, `Dimension`, `TimeProperties`). The decision to make `Node` public follows DESIGN.md's sketch and CONTEXT.md's "Claude's Discretion" default; flag this in the planning ADR comment.
- `enum class BinaryOp { Add, Subtract, Multiply, Divide };` declared at namespace scope above `BinaryOpNode`. Project uses `enum class` (scoped) per `.clang-tidy` `readability-identifier-naming` rule that requires `CamelCase` enum constants (see `TimeFrequency::Yearly` in `time_properties.h:15`).
- Forward-declare `BinaryFile` in this header (only the `FileNode(const BinaryFile&)` constructor uses it, and a forward declaration suffices because the constructor body lives in `nodes.cpp`). This avoids a `binary_file.h` include in the public Node header and keeps compile times reasonable.

---

### `include/quiver/binary/iteration.h` (free-function header)

**Analog:** `include/quiver/binary/time_constants.h` (sibling header in same subsystem and namespace style)

**Header structure (`include/quiver/binary/time_constants.h` lines 1-37):**
```cpp
// include/quiver/binary/time_constants.h
#ifndef QUIVER_TIME_CONSTANTS_H
#define QUIVER_TIME_CONSTANTS_H

namespace quiver::time {
// Hours
constexpr int MAX_HOURS_IN_DAY = 24;
// ...
}  // namespace quiver::time

#endif  // QUIVER_TIME_CONSTANTS_H
```

**Free-function declaration with `QUIVER_API` analog (`include/quiver/binary/time_properties.h` lines 1-19):**
```cpp
// include/quiver/binary/time_properties.h
#ifndef QUIVER_TIME_PROPERTIES_H
#define QUIVER_TIME_PROPERTIES_H

#include "../export.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace quiver {

struct Dimension;

enum class TimeFrequency { Yearly, Monthly, Weekly, Daily, Hourly };

QUIVER_API std::string frequency_to_string(TimeFrequency frequency);
QUIVER_API TimeFrequency frequency_from_string(const std::string& str);
```

**Replication notes:**
- Use `namespace quiver::binary { … }` concatenated form — matches `namespace quiver::time` in `time_constants.h:4`.
- Decorate every free function with `QUIVER_API` — exactly the pattern of `frequency_to_string` / `frequency_from_string` in `time_properties.h:17-18`.
- Header-guard format: `QUIVER_BINARY_ITERATION_H` (or `QUIVER_ITERATION_H` — both consistent with codebase).
- Include `binary_metadata.h` (the parameter type) and `<optional>`, `<vector>`, `<cstdint>` — minimal STL set, no transitive pulls.
- Forward-declaration of `BinaryMetadata` is **not** an option here because the free functions take `const BinaryMetadata&` — full type required for `add_dimension`-style callers, and the pattern in `time_properties.h:13` shows even a forward declaration of `Dimension` is allowed when only a pointer/reference is needed; here we need the full type.

**Divergence notes:**
- `next_dimensions` returns `std::optional<std::vector<int64_t>>` (Claude's-discretion default 4 in RESEARCH.md). No exact analog in the codebase for an `optional<vector>` return, but `std::optional` returns are used throughout `Database` (e.g., `std::optional<int64_t> read_scalar_integer_by_id(...)` in `database.h:53`). Idiomatic.
- Header lives in `include/quiver/binary/`, not the new `include/quiver/expr/` directory — the iterator helpers are part of the binary subsystem (work on `BinaryMetadata`), not the expression subsystem.

---

### `src/expr/expression.cpp` (engine impl + operators)

**Analogs:**
- `src/binary/csv_converter.cpp` (file-to-file streaming with `next_dimensions`-driven row loop)
- `src/element.cpp` (value-type impl style)

**Streaming row-loop pattern from `src/binary/csv_converter.cpp` lines 92-134** (this is the loop shape the save engine mirrors):
```cpp
// src/binary/csv_converter.cpp lines 92-134
void CSVConverter::bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions) {
    // Open the binary file in read mode
    BinaryFile bin_reader = BinaryFile::open_file(file_path, 'r');
    const auto& metadata = bin_reader.get_metadata();

    // Open the CSV file in write mode
    auto csv_io = std::make_unique<std::fstream>(file_path + std::string(CSV_EXTENSION), std::ios::out);
    CSVConverter csv_writer(file_path, metadata, std::move(csv_io), aggregate_time_dimensions);
    csv_writer.write_header();

    // Get initial dimension values
    const auto& dimensions = metadata.dimensions;
    std::vector<int64_t> initial_dimensions;
    for (const auto& dim : dimensions) {
        if (dim.is_time_dimension()) {
            initial_dimensions.push_back(dim.time->initial_value);
        } else {
            initial_dimensions.push_back(1);
        }
    }

    // Calculate maximum number of lines
    int64_t max_lines = 1;
    for (const auto& dim : dimensions) {
        max_lines *= dim.size;
    }

    // Iterate files and write to CSV
    std::vector<int64_t> current_dimensions = initial_dimensions;
    for (int64_t i = 0; i < max_lines; ++i) {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dimensions[j];
        }

        std::vector<double> data = bin_reader.read(dims, true);
        csv_writer.get_io() << csv_writer.build_line(data, current_dimensions);

        current_dimensions = csv_writer.next_dimensions(current_dimensions);
        if (current_dimensions == initial_dimensions) {
            break;
        }
    }
}
```

**Pattern-1 error throw from `src/database_create.cpp` lines 9-12** (the canonical "Cannot {operation}: {reason}" form for D-11 self-save check):
```cpp
// src/database_create.cpp lines 9-12
const auto& scalars = element.scalars();
if (scalars.empty()) {
    throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
}
```

**Implementation file include layout from `src/element.cpp` lines 1-5** (impl-file include style for a value type):
```cpp
// src/element.cpp
#include "quiver/element.h"

#include <sstream>

namespace quiver {
```

**Replication notes:**
- `#include "quiver/expr/expression.h"` first (own header), then a blank line, then STL/project headers in alphabetical groups (clang-format `IncludeBlocks: Regroup` handles ordering automatically).
- Open/close `namespace quiver::expr { … }` exactly once. Closing comment must be `}  // namespace quiver::expr` per `clang-format` `FixNamespaceComments: true`.
- The save-engine row loop replaces the `unordered_map<string, int64_t>` slow path with the fast `vector<int64_t>` path (`writer.write(row, dims);` calls the new D-14 overload). The shape mirrors `csv_converter.cpp:120-134` but with three changes:
  1. Use `quiver::binary::first_dimensions(meta)` instead of the inline `initial_dimensions` builder.
  2. Use `quiver::binary::next_dimensions(meta, dims)` returning `std::optional` — break when `nullopt`.
  3. Maintain a single `std::vector<double> row;` outside the loop (CORE-06).
- The D-11 self-save check uses Pattern 1: `throw std::runtime_error("Cannot save: output path collides with input file '" + in + "'");`. Use `std::filesystem::weakly_canonical` (already used in `src/binary/binary_file.cpp:56`).
- Free-function operator overloads at namespace scope at the end of the file. Each constructs a `std::shared_ptr<BinaryOpNode>` and wraps it in `Expression`.

**Divergence notes:**
- The save engine is **new logic**, not a pure copy. `csv_converter.cpp` reads from a single binary file; the save engine reads from an arbitrary tree of `Node`s via `node_->compute_row(dims, row)`.
- `csv_converter.cpp` uses a `for (i < max_lines)` upper bound; the engine should use a `do { … } while (next_dim has value)` loop because `next_dimensions` returns `optional` in the new API. Don't fall into the `max_lines` counting pattern — `optional`-loop is cleaner.
- Operator overloads have no analog — write from scratch following the locked signatures in RESEARCH.md §"AST: Node virtual hierarchy + Expression value type" (twelve overloads: three for each of `+ - * /`).

---

### `src/expr/nodes.cpp` (virtual hierarchy impl + validation)

**Analogs:**
- `src/binary/binary_metadata.cpp` lines 367-457 — multi-step `validate()` with sequential checks and Pattern-1 throws.
- `src/database_create.cpp` lines 9-12 — concise Pattern-1 precondition check.

**Multi-step validation pattern from `src/binary/binary_metadata.cpp` lines 367-419** (model for `BinaryOpNode` constructor's chained validation):
```cpp
// src/binary/binary_metadata.cpp
void BinaryMetadata::validate() const {
    // Version check
    if (version != QUIVER_FILE_VERSION) {
        throw std::runtime_error("Incompatible file version: expected " + std::string(QUIVER_FILE_VERSION) + ", got " +
                                 version);
    }

    // Dimension count
    if (dimensions.empty()) {
        throw std::runtime_error("Number of dimensions must be positive, got 0");
    }

    // Label count
    if (labels.empty()) {
        throw std::runtime_error("Number of labels must be positive, got 0");
    }

    // Time dimension count must not exceed total dimensions
    if (number_of_time_dimensions < 0 || number_of_time_dimensions > static_cast<int64_t>(dimensions.size())) {
        throw std::runtime_error("Number of time dimensions must be non-negative and <= number of dimensions. "
                                 "Got number_of_time_dimensions=" +
                                 std::to_string(number_of_time_dimensions) +
                                 ", number_of_dimensions=" + std::to_string(dimensions.size()));
    }

    // Dimension sizes must be positive
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].size <= 0) {
            throw std::runtime_error("Dimension size at index " + std::to_string(i) + " must be positive, got " +
                                     std::to_string(dimensions[i].size));
        }
    }

    // ... more checks ...

    validate_time_dimension_metadata();
}
```

**Setter pattern for value-type metadata from `src/binary/binary_metadata.cpp` lines 558-566** (model for building `broadcast_meta_` in the BinaryOpNode constructor):
```cpp
// src/binary/binary_metadata.cpp lines 558-566
void BinaryMetadata::add_dimension(const std::string& name, int64_t size) {
    dimensions.push_back({name, size, std::nullopt});
}

void BinaryMetadata::add_time_dimension(const std::string& name, int64_t size, const std::string& frequency) {
    TimeFrequency freq_enum = frequency_from_string(frequency);
    TimeProperties time_props{freq_enum, 0, -1};
    dimensions.push_back({name, size, std::move(time_props)});
}
```

**Replication notes:**
- `BinaryOpNode` ctor follows the `validate()` pattern: ordered checks (cheap first: unit, then shape, then time props, then initial_datetime, then labels), each with a Pattern-1 throw using the exact wording locked by D-06 / D-07 / D-09.
- Build `broadcast_meta_` with direct member assignment + `dimensions.push_back({…})` — same as `add_dimension` does. `BinaryMetadata` is a Rule-of-Zero struct with public members so this works directly.
- After building, call `broadcast_meta_.validate()` to catch any internal-consistency violations as defense in depth (Pitfall 6 in RESEARCH.md).
- For `FileNode::compute_row` lazy file open:
  ```cpp
  if (!file_) {
      file_ = std::make_unique<BinaryFile>(BinaryFile::open_file(path_, 'r'));
  }
  file_->read_into(own_dims_buf_, out);  // D-13 fast path
  ```
- Use `std::variant<std::shared_ptr<Node>, double>` is **NOT** the chosen design — the AST uses concrete subclasses (`FileNode` / `ScalarNode` / `BinaryOpNode`) with virtual dispatch. Stay with the polymorphic design.
- All throws use Pattern 1 (`"Cannot apply binary operation: ..."`). Concrete strings come from D-06 / D-07 / D-09; planner has discretion on minor wording. Use the literal strings from CONTEXT.md verbatim where they appear in quotes.

**Divergence notes:**
- `Node::compute_row` is `const` but mutates buffer members — this requires `mutable` on `lhs_buf_`, `rhs_buf_`, `lhs_dims_buf_`, `rhs_dims_buf_`, and `file_`. **No in-tree analog** uses `mutable` members; the planner should treat this as a deliberate choice and add a code comment explaining the logical-immutability rationale ("buffers are reused for performance; the node's logical output is unchanged across calls").
- The validation logic in `BinaryOpNode` ctor is more complex than `BinaryMetadata::validate()` (six chained checks across two operands); the planner may consider extracting helpers like `validate_units`, `compute_broadcast_dims`, `validate_time_props`, `compute_broadcast_labels` as private static-or-anonymous-namespace functions in `nodes.cpp`. The risk register item ("BinaryOpNode ctor validation logic accumulates complexity") lists this explicitly.

---

### `src/binary/iteration.cpp` (free-function impl — refactor lift)

**Analog:** `src/binary/binary_file.cpp` lines 245-348 (the existing `BinaryFile::next_dimensions` and `dimension_sizes_at_values` — these are the **literal source** for the lifted free functions).

**Existing protected member to lift (`src/binary/binary_file.cpp` lines 245-276):**
```cpp
// src/binary/binary_file.cpp lines 245-276
std::vector<int64_t> BinaryFile::next_dimensions(const std::vector<int64_t>& current_dimensions) {
    const auto& dimensions = impl_->metadata.dimensions;
    const auto& current_sizes = dimension_sizes_at_values(current_dimensions);

    std::vector<int64_t> next = current_dimensions;

    for (int i = static_cast<int>(next.size()) - 1; i >= 0; --i) {
        if (next[i] < current_sizes[i]) {
            next[i] += 1;
            break;
        } else {
            next[i] = 1;
        }
    }

    // Adjust time dimensions which were reset to 1 before their parent dimension is incremented.
    // Ex: [month, scenario, day] when initial date is 2025-01-02
    // [1, 1, 31] -> [1, 2, 1] is incorrect, should be [1, 2, 2]
    for (size_t i = 0; i < next.size(); ++i) {
        const auto& dim = dimensions[i];
        if (!dim.is_time_dimension())
            continue;
        int64_t initial_value = dim.time->initial_value;
        int64_t parent_idx = dim.time->parent_dimension_index;  // -1 = no parent
        if (next[i] < initial_value && parent_idx != -1 &&
            next[parent_idx] == dimensions[parent_idx].time->initial_value) {
            next[i] = initial_value;
        }
    }

    return next;
}
```

**`dimension_sizes_at_values` to lift (`src/binary/binary_file.cpp` lines 278-348):**
```cpp
// src/binary/binary_file.cpp lines 278-348 (excerpt — full body is the calendar-aware time-dim logic)
std::vector<int64_t> BinaryFile::dimension_sizes_at_values(const std::vector<int64_t>& dimension_values) const {
    using namespace quiver::time;
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;

    std::vector<int64_t> sizes;
    sizes.reserve(dimensions.size());
    for (const auto& dim : dimensions) {
        sizes.push_back(dim.size);
    }

    auto datetime = metadata.initial_datetime;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (!dimensions[i].is_time_dimension())
            continue;
        datetime = dimensions[i].time->add_offset_from_int(datetime, dimension_values[i]);
    }
    // ... calendar arithmetic for variable-size time dims (lines 295-345) ...
    return sizes;
}
```

**Replication notes:**
- Lift the bodies **verbatim** into pure functions in the `quiver::binary` namespace. The functions take `const BinaryMetadata&` instead of accessing `impl_->metadata` (the only required substitution).
- Move `dimension_sizes_at_values` into the same `iteration.cpp` as a static helper in an anonymous namespace (it's a private impl detail of `next_dimensions`); only expose `first_dimensions` and `next_dimensions` in the public header.
- `first_dimensions` is essentially the inline initial-dimensions builder from `csv_converter.cpp:104-111` lifted into a function:
  ```cpp
  std::vector<int64_t> first_dimensions(const BinaryMetadata& meta) {
      std::vector<int64_t> result;
      result.reserve(meta.dimensions.size());
      for (const auto& dim : meta.dimensions) {
          result.push_back(dim.is_time_dimension() ? dim.time->initial_value : 1);
      }
      return result;
  }
  ```
- `next_dimensions` returns `std::optional<std::vector<int64_t>>`: after running the existing increment + carry logic, compare to `first_dimensions(meta)` — if equal, return `std::nullopt` (means we wrapped past the end); else return the new vector.
- Existing `BinaryFile::next_dimensions` and `BinaryFile::dimension_sizes_at_values` (protected members) **must remain**: `csv_converter.cpp:85` and `csv_converter.cpp:130` call `csv_writer.next_dimensions(current_dimensions)` (where `csv_writer` is a `CSVConverter` inheriting from `BinaryFile`). Two options for the planner:
  - (a) Keep the protected member intact; the lifted free function becomes a parallel implementation (some duplication; DRY violated).
  - (b) Make the protected member a thin delegate: `return *quiver::binary::next_dimensions(impl_->metadata, current_dimensions);` (handles end-of-iteration: csv_converter currently signals end via comparison to `initial_dimensions`; if the free function returns `nullopt`, the delegate could fall back to returning `current_dimensions` — but this would change behavior. Safer: have the delegate return `first_dimensions(meta)` when the free function returns `nullopt`, preserving the existing csv_converter loop semantics).
  - **Default recommendation:** option (b) with the fallback — single source of truth for the time-dim reset adjustment, csv_converter behavior unchanged. Plan must include a regression test that runs `csv_converter` round-trip after the refactor.

**Divergence notes:**
- The protected member `BinaryFile::next_dimensions` mutates internal state nowhere (it's a const-correct const-friendly read); the free function has the same property. No state drift.
- Be careful with the `using namespace quiver::time;` directive at line 279 of `binary_file.cpp` — when lifting, prefer fully qualifying (`quiver::time::MAX_HOURS_IN_DAY`) inside the new file or at least keep the using inside an anonymous namespace, to avoid polluting the broader `quiver::binary` namespace.

---

### `tests/test_expression.cpp` (GoogleTest fixture)

**Analog:** `tests/test_binary_file.cpp` lines 1-200 (programmatic-fixture pattern, `BinaryTempFileFixture`).

**Test-file include layout (`tests/test_binary_file.cpp` lines 1-11):**
```cpp
// tests/test_binary_file.cpp lines 1-11
#include <chrono>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <quiver/binary/binary_file.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/element.h>

using namespace quiver;
namespace fs = std::filesystem;
```

**Fixture pattern (`tests/test_binary_file.cpp` lines 13-52):**
```cpp
// tests/test_binary_file.cpp
// ============================================================================
// Fixture
// ============================================================================

class BinaryTempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_binary_test").string(); }

    void TearDown() override {
        for (auto ext : {".qvr", ".toml", ".csv"}) {
            auto full = path + ext;
            if (fs::exists(full))
                fs::remove(full);
        }
    }

    std::string path;

    static BinaryMetadata make_simple_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row", "col"})
                                                .set("dimension_sizes", {3, 2})
                                                .set("labels", {"val1", "val2"}));
    }

    static BinaryMetadata make_time_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"stage", "block"})
                                                .set("dimension_sizes", {4, 31})
                                                .set("time_dimensions", {"stage", "block"})
                                                .set("frequencies", {"monthly", "daily"})
                                                .set("labels", {"plant_1", "plant_2"}));
    }
};
```

**Test case naming + `EXPECT_THROW` pattern (`tests/test_binary_file.cpp` lines 244-262):**
```cpp
// tests/test_binary_file.cpp lines 244-262
TEST_F(BinaryTempFileFixture, ReadUnwrittenPositionThrowsWhenNullsNotAllowed) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_THROW(reader.read({{"row", 1}, {"col", 1}}, false), std::runtime_error);
}

TEST_F(BinaryTempFileFixture, ReadUnwrittenPositionReturnsNaNWhenNullsAllowed) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    auto reader = BinaryFile::open_file(path, 'r');
    auto values = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isnan(values[1]));
}
```

**Replication notes:**
- Copy `BinaryTempFileFixture` structure exactly: rename to `ExpressionFixture`, add `path_a`, `path_b`, `path_out` instead of single `path`, expand `TearDown` to clean all three.
- Copy `make_simple_metadata` + `make_time_metadata` helpers verbatim; possibly add a third helper `make_metadata_with_extra_dim()` for the `UnionDimsAcrossOperands` test.
- Add a `write_qvr(path, meta, fill_fn)` helper that opens a writer, iterates via `quiver::binary::first_dimensions` / `next_dimensions`, and writes computed values — this is the "programmatic fixture" recommended by Claude's-discretion default 5.
- Test names use `CamelCase` (`IdentityFile`, `AddTwoFilesAndScale`, `ScalarBroadcastAddRight`). Section dividers use 76 `=`-character lines (see `tests/test_binary_file.cpp:13-15` and many subsequent dividers).
- Use `EXPECT_THROW(expr, std::runtime_error)` for all error-path tests (D-06 / D-07 / D-09 / D-11 throws are all `runtime_error`). Use `EXPECT_DOUBLE_EQ` for value comparison, `EXPECT_NEAR` if floating-point tolerance is needed (none anticipated for the integer-value test cases).
- For the `LargeGridCompletes` test (CORE-06 timing smoke), use a generous budget (5 seconds for 100×100×5) per RESEARCH.md risk-register guidance about CI flakiness, OR remove the timing assertion entirely and rely on code review of `Expression::save()`.

**Divergence notes:**
- Test count is much higher than `test_binary_file.cpp` (~28 cases vs ~50, but many short). This is fine — `quiver_tests.exe` already has hundreds of cases.
- The fixture cleans up three paths instead of one; expand `TearDown` accordingly:
  ```cpp
  void TearDown() override {
      for (const auto& p : {path_a, path_b, path_out}) {
          for (auto ext : {".qvr", ".toml"}) {
              auto full = p + ext;
              if (fs::exists(full)) fs::remove(full);
          }
      }
  }
  ```
- `path_c` should be added if any test uses three input files (e.g., `Chained` uses `((a + b) - c) / 2.0`).

---

### `include/quiver/binary/binary_file.h` (EDIT — add 3 overloads)

**Analog:** existing `read` and `write` declarations on lines 37-38 of the same file.

**Current declarations (`include/quiver/binary/binary_file.h` lines 36-39):**
```cpp
// include/quiver/binary/binary_file.h lines 36-39
    // Data handling
    std::vector<double> read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls = false);
    void write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);
```

**Replication notes:**
- Add three new public methods adjacent to the existing `read`/`write`, immediately after them (still under `// Data handling` comment):
  ```cpp
  // Fast overloads (D-13/D-14)
  std::vector<double> read(const std::vector<int64_t>& dims, bool allow_nulls = false);
  void read_into(const std::vector<int64_t>& dims, std::vector<double>& out, bool allow_nulls = false);
  void write(const std::vector<double>& data, const std::vector<int64_t>& dims);
  ```
- Argument order: `(dims, allow_nulls)` for `read`, `(dims, out, allow_nulls)` for `read_into`, `(data, dims)` for `write` — matches the existing slow paths' argument order.
- No `const` on `read` / `read_into` (matches existing `read` — both mutate file position via `go_to_position`).
- `write` is also non-const (matches existing).

**Divergence notes:**
- Existing slow `read`/`write` use `unordered_map<std::string, int64_t>`; new fast variants use `std::vector<int64_t>` (positional, dimension-declaration-order). Both coexist (per CONTEXT.md "Coexistence vs deprecation": leave the slow ones alone — csv_converter still depends on them).
- Watch out for overload resolution ambiguity at call sites that use brace-init (Pitfall 8): `bf.read({1, 2})` is unambiguous because `unordered_map<string, int64_t>` cannot be built from `{1, 2}` (needs string keys). But CSV converter uses `bf.read(dims, true)` where `dims` is an `unordered_map` variable — fine.

---

### `src/binary/binary_file.cpp` (EDIT — implement 3 overloads + helpers)

**Analog:** existing `read`, `write`, `calculate_file_position`, `validate_dimension_values` impls in the same file.

**Existing slow `read` impl to mirror (`src/binary/binary_file.cpp` lines 106-131):**
```cpp
// src/binary/binary_file.cpp lines 106-131
std::vector<double> BinaryFile::read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls) {
    validate_dimension_values(dims);

    go_to_position(calculate_file_position(dims), 'r');

    std::vector<double> data(impl_->metadata.labels.size());
    auto bytes = static_cast<int64_t>(data.size() * sizeof(double));
    impl_->io->read(reinterpret_cast<char*>(data.data()), bytes);
    impl_->current_position += bytes;

    if (!allow_nulls) {
        for (size_t i = 0; i < data.size(); ++i) {
            if (std::isnan(data[i])) {
                std::string dim_str;
                for (const auto& [name, value] : dims) {
                    if (!dim_str.empty())
                        dim_str += ", ";
                    dim_str += name + "=" + std::to_string(value);
                }
                throw std::runtime_error("Cannot read: data at {" + dim_str + "} contains null values");
            }
        }
    }

    return data;
}
```

**Existing `calculate_file_position` (slow, with hash lookup) to parallel (`src/binary/binary_file.cpp` lines 144-158):**
```cpp
// src/binary/binary_file.cpp lines 144-158
int64_t BinaryFile::calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const {
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;
    int64_t position = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        int64_t stride = 1;
        for (size_t j = i + 1; j < dimensions.size(); ++j) {
            stride *= dimensions[j].size;
        }
        position += (dims.at(dimensions[i].name) - 1) * stride;
    }
    position *= static_cast<int64_t>(metadata.labels.size());
    position *= static_cast<int64_t>(sizeof(double));
    return position;
}
```

**Existing `validate_dimension_values` to parallel (`src/binary/binary_file.cpp` lines 184-236):**
```cpp
// src/binary/binary_file.cpp lines 184-236 (excerpt)
void BinaryFile::validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims) {
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;

    // Check count
    if (dims.size() != dimensions.size()) {
        throw std::invalid_argument("Expected " + std::to_string(dimensions.size()) + " dimensions, got " +
                                    std::to_string(dims.size()));
    }

    // Check all dimension names exist and values are in bounds
    for (size_t i = 0; i < dimensions.size(); ++i) {
        const auto& dim = dimensions[i];
        auto it = dims.find(dim.name);
        if (it == dims.end()) {
            throw std::invalid_argument("Missing required dimension: '" + dim.name + "'");
        }
        int64_t value = it->second;
        if (value < 1 || value > dim.size) {
            throw std::invalid_argument("Dimension '" + dim.name + "' value " + std::to_string(value) +
                                        " is out of bounds [1, " + std::to_string(dim.size) + "]");
        }
    }
    // ... time-dim consistency checks (lines 208-235) ...
}
```

**Replication notes:**
- Add a private helper `int64_t calculate_file_position_indexed(const std::vector<int64_t>& dims) const` adjacent to the existing one. Body uses pre-computed strides (RESEARCH.md approach 1.1: cache `std::vector<int64_t> strides_;` in `Impl`). Or compute strides inline on every call (simpler, same big-O):
  ```cpp
  int64_t BinaryFile::calculate_file_position_indexed(const std::vector<int64_t>& dims) const {
      const auto& metadata = impl_->metadata;
      const auto& dimensions = metadata.dimensions;
      int64_t position = 0;
      for (size_t i = 0; i < dimensions.size(); ++i) {
          int64_t stride = 1;
          for (size_t j = i + 1; j < dimensions.size(); ++j) stride *= dimensions[j].size;
          position += (dims[i] - 1) * stride;
      }
      position *= static_cast<int64_t>(metadata.labels.size());
      position *= static_cast<int64_t>(sizeof(double));
      return position;
  }
  ```
- Add a private helper `void validate_dimension_values_indexed(const std::vector<int64_t>& dims)` that mirrors the existing string-keyed validator but iterates in declaration order. The time-dim consistency check (lines 208-235) is identical in shape — replace `dims.at(dim.name)` with `dims[i]`.
- New `read(vector<int64_t>, bool)` — call `validate_dimension_values_indexed`, then `go_to_position(calculate_file_position_indexed(dims), 'r')`, then read into a freshly-allocated `vector<double>(metadata.labels.size())`, then null-check.
- New `read_into(vector<int64_t>, vector<double>&, bool)` — **skip** validation (D-13). Resize `out` only if `out.size() != labels.size()`. Read into `out.data()`. Null-check.
- New `write(vector<double>, vector<int64_t>)` — call `validate_dimension_values_indexed`, `validate_data_length`, then write.
- The null-error message format: `"Cannot read: data at {row=1, col=2} contains null values"` (Pattern 1) — for the indexed variant, build `dim_str` by iterating dimensions in declaration order: `dim_str += dimensions[i].name + "=" + std::to_string(dims[i]);`.

**Divergence notes:**
- Pre-computing strides in `Impl` (vector member) is an optimization for repeated calls; computing on every call is fine for Phase 1 because the engine's hot path goes through `calculate_file_position_indexed` once per row, and the inner loop already saves ~40% via avoiding the hash. Either choice is acceptable; planner picks based on simplicity.
- The new `validate_dimension_values_indexed` should be added **next to** the existing `validate_dimension_values` (lines 184-236), so the two parallel methods are adjacent — improves discoverability per the project's "human-centric" principle.

---

### `src/CMakeLists.txt` (EDIT)

**Analog:** the existing `set(QUIVER_SOURCES …)` block (lines 1-28).

**Existing block (`src/CMakeLists.txt` lines 1-28):**
```cmake
# src/CMakeLists.txt lines 1-28
# Core library sources
set(QUIVER_SOURCES
    database.cpp
    database_create.cpp
    database_read.cpp
    database_update.cpp
    database_delete.cpp
    database_metadata.cpp
    database_time_series.cpp
    database_query.cpp
    database_csv_export.cpp
    database_csv_import.cpp
    database_describe.cpp
    element.cpp
    lua_runner.cpp
    migration.cpp
    migrations.cpp
    result.cpp
    row.cpp
    schema.cpp
    schema_validator.cpp
    type_validator.cpp
    binary/binary_file.cpp
    binary/csv_converter.cpp
    binary/binary_metadata.cpp
    binary/dimension.cpp
    binary/time_properties.cpp
)
```

**Replication notes:**
- Append three lines (preserve alphabetical-ish grouping where it exists; `binary/iteration.cpp` goes with the other `binary/` entries; new `expr/` entries form a new logical group at the end):
  ```cmake
  binary/iteration.cpp
  expr/expression.cpp
  expr/nodes.cpp
  ```
- Path style is relative (no leading `./`), forward-slash separator (matches existing style; works on both Windows MSVC and Unix Make/Ninja).
- No further changes — `quiver` target wiring (`add_library(quiver SHARED …)`, `target_compile_definitions`, `target_link_libraries`) all stay the same. New sources inherit the same compile flags and link dependencies.

**Divergence notes:**
- None. This is a purely additive edit, no risk of breaking existing build.

---

### `tests/CMakeLists.txt` (EDIT)

**Analog:** the existing `add_executable(quiver_tests …)` block (lines 3-25).

**Existing block (`tests/CMakeLists.txt` lines 3-25):**
```cmake
# tests/CMakeLists.txt lines 3-25
add_executable(quiver_tests
    test_binary_file.cpp
    test_csv_converter.cpp
    test_binary_metadata.cpp
    test_binary_time_properties.cpp
    test_database_create.cpp
    test_database_csv_export.cpp
    test_database_csv_import.cpp
    test_database_delete.cpp
    test_database_errors.cpp
    test_database_lifecycle.cpp
    test_database_query.cpp
    test_database_read.cpp
    test_database_time_series.cpp
    test_database_transaction.cpp
    test_database_update.cpp
    test_element.cpp
    test_issues.cpp
    test_lua_runner.cpp
    test_migrations.cpp
    test_row_result.cpp
    test_schema_validator.cpp
)
```

**Replication notes:**
- Append a single line: `test_expression.cpp`. Place near the other `test_binary_*` entries (alphabetical) or at the end (also acceptable — existing list is not strictly alphabetical).
- No other changes — `target_link_libraries(quiver_tests …)`, `gtest_discover_tests`, and the Windows DLL copy step all already cover any new test file.

**Divergence notes:**
- None. Purely additive.

---

## Shared Patterns

### Authentication / Authorization

**Not applicable.** Quiver is an embedded library with no auth surface. ASVS V2/V3/V4 are N/A per RESEARCH.md §"Security Domain".

### Error Handling (Pattern 1: Cannot {operation}: {reason})

**Source:** `src/binary/binary_metadata.cpp` lines 367-419 (multi-step validation), `src/database_create.cpp` lines 9-12 (concise precondition check).

**Apply to:** `src/expr/expression.cpp` (D-11 self-save check), `src/expr/nodes.cpp` (all `BinaryOpNode` ctor checks for D-04 / D-06 / D-07 / D-09).

**Concrete excerpt from `src/database_create.cpp` lines 9-12:**
```cpp
// src/database_create.cpp
const auto& scalars = element.scalars();
if (scalars.empty()) {
    throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
}
```

**Replication notes:**
- All new throws in Phase 1 use Pattern 1 (`"Cannot {operation}: {reason}"`).
- Use `std::runtime_error` (not `std::invalid_argument` or custom) — matches D-06 / D-07 / D-09 / D-11 specs and existing codebase usage in `binary_metadata.cpp:367+`.
- Error strings should pinpoint the failing element where helpful: e.g., `"Cannot apply binary operation: dimension 'X' has incompatible sizes 3 vs 4 (broadcasting requires n×n, 1×n, or n×1)"`.

### Validation (eager, multi-step)

**Source:** `src/binary/binary_metadata.cpp` lines 367-419 (`BinaryMetadata::validate()`).

**Apply to:** `src/expr/nodes.cpp` (`BinaryOpNode` constructor — six-step chained validation D-04/D-06/D-07/D-08/D-09 plus internal-consistency `broadcast_meta_.validate()`).

**Pattern excerpt already shown above** in the `src/expr/nodes.cpp` section.

**Replication notes:**
- Order checks cheapest-first: unit (single string compare), then shape (dim name lookup), then time props (vector traversal), then initial_datetime (single compare), then labels (vector compare).
- Call `broadcast_meta_.validate()` last as defense-in-depth.

### Logging

**Source:** `src/database_create.cpp` line 6 (`impl_->logger->debug("Creating element in collection: {}", collection);`).

**Apply to:** **Not applicable for Phase 1.** The expression subsystem has no per-database logger context (no `Database` instance). RESEARCH.md §"Logging" confirms: "All log output comes from the C++ core" but the existing pattern is per-`Database`-instance. `Expression::save()` does not have a logger to call into. Phase 1 omits logging.

**Replication notes:** Skip logging in Phase 1. If future phases want to log expression operations, the natural place is when an expression is constructed or saved against a `Database` — which is not a Phase 1 concern.

### Ownership & Resource Management (Rule of Zero / Pimpl distinction)

**Source:**
- Rule of Zero: `include/quiver/element.h` (no copy/move/dtor declared; compiler-generated)
- Pimpl: `include/quiver/binary/binary_file.h` lines 22-30 + `src/binary/binary_file.cpp` lines 20-44 (struct Impl in cpp)

**Apply to:**
- Rule of Zero: `Expression`, `Node`, `FileNode`, `ScalarNode`, `BinaryOpNode` — none of these hide private dependencies.
- No new Pimpl in Phase 1. `BinaryFile` keeps its existing Pimpl (`src/binary/binary_file.cpp:20-44`).

**Excerpt from `BinaryFile::Impl` for context (`src/binary/binary_file.cpp` lines 20-44):**
```cpp
// src/binary/binary_file.cpp lines 20-44
struct BinaryFile::Impl {
    std::unique_ptr<std::iostream> io;
    std::string file_path;
    std::string registered_path;  // non-empty only for writers; used to unregister on destruction
    BinaryMetadata metadata;
    int64_t current_position = -1;  // tracked file position to skip redundant seeks

    Impl(std::unique_ptr<std::iostream> io, std::string file_path, BinaryMetadata metadata)
        : io(std::move(io)), file_path(std::move(file_path)), metadata(std::move(metadata)) {}

    Impl(Impl&&) noexcept = default;
    Impl& operator=(Impl&&) noexcept = default;

    ~Impl() {
        if (!registered_path.empty()) {
            write_registry.erase(registered_path);
        }
        if (io) {
            io->flush();
        }
    }
};
```

**Replication notes:**
- For `Expression`: just declare `std::shared_ptr<Node> node_;` as a private member — no Pimpl. Compiler generates copy/move/dtor.
- For `Node` virtual base: declare `virtual ~Node() = default;` to make the type polymorphic-safe; no other special members needed.
- For `FileNode`: holds a `mutable std::unique_ptr<BinaryFile> file_;` — RAII closes the file on destruction. The unique_ptr handles move-only semantics correctly.

### Naming (verb_[category_]type pattern, lower_case methods)

**Source:** project-wide — see CLAUDE.md §"Naming Convention" and `database.h` (verb-prefixed methods like `create_element`, `read_scalar_integers`).

**Apply to:**
- `Expression::save(path)` — verb_object form; consistent with `Database::describe`, `Database::export_csv`.
- `Expression::metadata()` — getter, no `get_` prefix (matches `BinaryFile::get_metadata` actually has `get_` — minor inconsistency in codebase; planner can match either, prefer `metadata()` for symmetry with `BinaryMetadata` everywhere else; flag the inconsistency in plan).
- Free functions: `quiver::binary::first_dimensions`, `quiver::binary::next_dimensions` — verb form, plural (returns multiple values).

**Divergence notes:**
- `BinaryFile::get_metadata` and `BinaryFile::get_file_path` use `get_` prefix; `BinaryMetadata::add_dimension` does not. Project is loosely consistent. Planner picks `Expression::metadata()` (no prefix) to match the value-type convention; document the choice in the plan.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| Operator overloads on a value type (in `expression.h` and `expression.cpp`) | n/a | n/a | First time the codebase exposes `operator+`/`-`/`*`/`/` as free functions on a public C++ class. sol2 metamethods in `src/lua_runner.cpp` are the closest semantic neighbor but bind to Lua, not C++. The planner writes these from scratch following the locked signatures in RESEARCH.md §"AST: Node virtual hierarchy + Expression value type". |
| Public virtual hierarchy with `mutable` cache members | n/a | n/a | `Database`, `BinaryFile`, `LuaRunner`, `CSVConverter` use Pimpl (no virtual public surface). `Element`, `BinaryMetadata`, `Dimension`, `TimeProperties` are plain value types (no virtual). `Node`/`FileNode`/`ScalarNode`/`BinaryOpNode` is the first such hierarchy. Planner follows the design notes in RESEARCH.md §"AST: Node virtual hierarchy" — no in-tree pattern to copy. |
| `std::optional<std::vector<int64_t>>` return type for iteration | n/a | n/a | Closest analog: `Database::read_scalar_integer_by_id` returns `std::optional<int64_t>` (`database.h:53`). The combination `optional<vector>` is not used anywhere in the codebase, but it is idiomatic C++20 and consistent with the pattern. Planner accepts it as the natural choice per RESEARCH.md Claude's-discretion default 4. |

## Metadata

**Analog search scope:**
- `include/quiver/` (all public headers — exhaustive)
- `include/quiver/binary/` (subsystem headers — exhaustive)
- `src/` (top-level impls — `database.cpp`, `database_create.cpp`, `element.cpp`)
- `src/binary/` (binary subsystem impls — `binary_file.cpp`, `binary_metadata.cpp`, `csv_converter.cpp`)
- `tests/` (`test_binary_file.cpp` for fixture pattern; spot-check of test_binary_metadata.cpp / test_csv_converter.cpp for divider conventions)
- `src/CMakeLists.txt`, `tests/CMakeLists.txt` (build wiring)

**Files scanned:** ~14 source/header files, 2 CMakeLists files, 1 test file (truncated read of `test_binary_file.cpp` — first 280 lines).

**Pattern extraction date:** 2026-05-06

**Confidence:** HIGH — every analog is verified against in-repo code with concrete line numbers; 8 of 11 files have an exact in-tree analog; 3 capabilities (operator overloads, public virtual hierarchy, `optional<vector>` return) have no in-tree analog and are flagged for the planner to write from scratch following locked design specs.

