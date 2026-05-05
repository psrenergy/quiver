# Phase 1: Storage and CRTP Scaffold - Pattern Map

**Mapped:** 2026-05-05
**Files analyzed:** 6 (5 NEW + 1 MOD)
**Analogs found:** 5 / 6 (one NEW with no in-tree precedent — flagged)

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `include/quiver/tensor/expression.h` | header — public C++ value-type / template | static dispatch (CRTP) | `include/quiver/binary/dimension.h` | role-match (header-only value type; CRTP itself is novel, no exact analog) |
| `include/quiver/tensor/shape.h` | header — public C++ aliases + free fns | transform | `include/quiver/binary/time_properties.h` | role-match (free fns + struct/alias in same header) |
| `include/quiver/tensor/tensor.h` | header — public C++ value type (templated) | container | `include/quiver/binary/binary_metadata.h` + `database.h` (move-noexcept) | role-match + move-pattern blend |
| `tests/test_tensor_storage.cpp` | test — GoogleTest | request-response (tests) | `tests/test_binary_metadata.cpp` | exact (same fixture-less TEST() pattern, identical includes/dividers) |
| `cmake/CheckTensorIncludeContainment.cmake` | CMake script — CI containment check | batch (file-list grep) | **no analog** — net new mechanism for project | NO ANALOG (flagged below) |
| `tests/CMakeLists.txt` | CMake — test executable + add_test registration | build | `tests/CMakeLists.txt` (existing `add_executable` block) | exact (modify-in-place) |

---

## Pattern Assignments

### `include/quiver/tensor/expression.h` (header — CRTP base + concept)

**Analog:** `include/quiver/binary/dimension.h` (header-only value-type precedent — depth, guards, namespace style)

**Include guard pattern** (`include/quiver/binary/dimension.h:1-3,22-24`):
```cpp
#ifndef QUIVER_DIMENSION_H
#define QUIVER_DIMENSION_H

#include "../export.h"
#include "time_properties.h"
// ...
}  // namespace quiver

#endif  // QUIVER_DIMENSION_H
```

**Namespace + type declaration** (`include/quiver/binary/dimension.h:12-20`):
```cpp
namespace quiver {

struct Dimension {
    std::string name;
    int64_t size;
    std::optional<TimeProperties> time;

    bool is_time_dimension() const { return time.has_value(); }
};

}  // namespace quiver
```

**What to copy:**
- File-level structure: `#ifndef QUIVER_TENSOR_EXPRESSION_H` guards (mirror the `QUIVER_<NAME>_H` convention used by `dimension.h`, `time_properties.h`, `binary_metadata.h`).
- Include `"../export.h"` placement (relative path inside `include/quiver/tensor/` → `../export.h`).
- Trailing namespace comment style: `}  // namespace quiver::tensor` (note: nested into `quiver::tensor`, **not** root `quiver`).
- 4-space indent, attached braces, `clang-format` will regroup includes (see CONVENTIONS.md "Import / Include Organization").

**What to change vs analog:**
- Namespace is `quiver::tensor` (subsystem nesting), not `quiver` directly. Dimension/TimeProperties live in root `quiver::` because they predated the binary subsystem boundary; new tensor work uses the proper subsystem namespace per CONTEXT.md `<code_context>` "Established Patterns" + research/ARCHITECTURE.md.
- `Expression<Derived>` is a **template class**, not a struct — no analog in-tree. Use C++20 concept idiom (`is_base_of_template_v` helper) per RESEARCH.md "State of the Art" guidance.
- **Do NOT** apply `QUIVER_API` macro to template classes/structs — header-only templates do not need DLL export annotations (RESEARCH.md Source verification: `include/quiver/export.h`). Use `QUIVER_API` only on the non-template helpers that compile to symbols (none in expression.h).

**No exact in-tree analog for CRTP itself** — the codebase has no existing CRTP base. Source patterns are external (Eigen `MatrixBase<Derived>`, xtensor `xexpression<E>` — cited in CONTEXT.md `<canonical_refs>`).

---

### `include/quiver/tensor/shape.h` (header — alias + free functions)

**Analog:** `include/quiver/binary/time_properties.h` (header with both free functions and a value type, same depth)

**Free-function declaration pattern** (`include/quiver/binary/time_properties.h:15-18`):
```cpp
enum class TimeFrequency { Yearly, Monthly, Weekly, Daily, Hourly };

QUIVER_API std::string frequency_to_string(TimeFrequency frequency);
QUIVER_API TimeFrequency frequency_from_string(const std::string& str);
```

**Header includes pattern** (`include/quiver/binary/time_properties.h:1-11`):
```cpp
#ifndef QUIVER_TIME_PROPERTIES_H
#define QUIVER_TIME_PROPERTIES_H

#include "../export.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace quiver {
```

**What to copy:**
- Free-function-in-header pattern for `compute_strides`, `total_size`, `ravel_index`. Per `time_properties.h:17-18` precedent, free functions can sit beside types in the same header.
- Include block ordering: `"../export.h"` first, blank line, then standard library headers (clang-format `IncludeBlocks: Regroup` enforces this).
- Header-name guard convention: `QUIVER_TENSOR_SHAPE_H`.

**What to change vs analog:**
- Free functions in `shape.h` are **template-friendly inline / `constexpr`**, not declarations needing a `.cpp`. Tag with `inline` (or `constexpr` where applicable) so they remain header-only. **Do NOT** annotate with `QUIVER_API` — header-only inline functions do not get exported. Contrast with `time_properties.h:17-18` where `frequency_to_string` is a non-inline free function and **does** get `QUIVER_API` because its definition lives in `src/binary/time_properties.cpp`.
- `shape_t` is a `using shape_t = std::vector<std::size_t>;` alias (per CONTEXT.md `<specifics>`), not a `struct`. Place under `quiver::tensor` namespace.
- No `.cpp` companion needed (CONTEXT.md `<code_context>`: "No `.cpp` companions needed for value types unless non-template helpers accumulate"). Per CONTEXT.md D-15..D-18 and the locked decision that all three free functions are inline header-only utilities.

---

### `include/quiver/tensor/tensor.h` (header — `Tensor<T>` value type)

**Analog (structure):** `include/quiver/binary/binary_metadata.h` (value type with multiple constructors and validation methods)
**Analog (move semantics):** `include/quiver/database.h:23-29` (`noexcept` move pattern Tensor<T> mirrors)

**Move-semantics pattern** (`include/quiver/database.h:23-29`):
```cpp
// Non-copyable
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;

// Movable
Database(Database&& other) noexcept;
Database& operator=(Database&& other) noexcept;
```

**Value-type constructor surface pattern** (`include/quiver/binary/binary_metadata.h:15-32`):
```cpp
struct QUIVER_API BinaryMetadata {
    std::vector<Dimension> dimensions;
    std::chrono::system_clock::time_point initial_datetime;
    std::string unit;
    std::vector<std::string> labels;
    std::string version;
    //
    int64_t number_of_time_dimensions = 0;

    BinaryMetadata();
    ~BinaryMetadata();

    // Create metadata from Element
    static BinaryMetadata from_element(const Element& element);

    // TOML Serialization
    static BinaryMetadata from_toml(const std::string& toml_content);
    std::string to_toml() const;
```

**Three-pattern error format** (CLAUDE.md "Error Handling" section, lines for `database_create.cpp:11`, `database_csv_import.cpp:38`, `database_impl.h:34`):
```cpp
// Pattern 1 — Precondition failure: "Cannot {operation}: {reason}"
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
throw std::runtime_error(std::string("Cannot ") + operation + ": no schema loaded");
throw std::runtime_error("Cannot import_csv: could not open file: " + path);

// Pattern 2 — Not found: "{Entity} not found: {identifier}"
throw std::runtime_error("Scalar attribute not found: 'value' in collection 'Items'");

// Pattern 3 — Operation failure: "Failed to {operation}: {reason}"
throw std::runtime_error("Failed to open database: " + sqlite3_errmsg(db));
```

**What to copy:**
- **Move semantics:** Tensor<T> needs `noexcept` move ctor + move assign mirroring `database.h:28-29`. Difference: `Database` is non-copyable (deletes copy ctor), but Tensor<T> **must remain copyable** (it is a value type, not a resource handle — research/ARCHITECTURE.md Anti-Pattern #5/#6 + CONTEXT.md "Plain value type with Rule of Zero"). Use `= default` for both copy and move with `noexcept` on move:
  ```cpp
  Tensor(const Tensor&) = default;
  Tensor& operator=(const Tensor&) = default;
  Tensor(Tensor&&) noexcept = default;
  Tensor& operator=(Tensor&&) noexcept = default;
  ~Tensor() = default;
  ```
- **Multi-constructor surface** mirroring `binary_metadata.h` (default + factories): three constructors per CONTEXT.md D-01 (`Tensor(shape_t)`, `Tensor(shape_t, T fill)`, `Tensor(shape_t, std::initializer_list<T>)`).
- **Three-pattern error format** for the four locked Phase 1 throws (CONTEXT.md `<specifics>`):
  - `Cannot construct tensor: initializer_list size {got} does not match shape size {expected}` (from `Tensor(shape_t, std::initializer_list<T>)`)
  - `Cannot access tensor: rank mismatch (expected {rank} indices, got {n})` (from `at(...)`)
  - `Cannot access tensor: index {i,j,k} out of bounds for shape {a,b,c}` (from `at(...)`)
  - `Cannot item: tensor has {N} elements (expected 1)` (from `.item()`)
  All four are Pattern 1 (`Cannot {op}: {reason}`).

**What to change vs analog:**
- Tensor<T> is a **template class**, not a non-template struct. Cannot use `QUIVER_API` on the template itself (header-only — no symbol to export). The struct stays declared in the header with all member function bodies inline (or in-class).
- **No `~Tensor()` user declaration** needed (Rule of Zero — compiler-generated). Contrast with `binary_metadata.h:24-25` which declares both `BinaryMetadata()` and `~BinaryMetadata()` to keep the destructor's vector-of-Dimension instantiation out of the header. Tensor's `std::vector<T>` member uses already-complete types, so the destructor stays inline.
- **CRTP base inheritance:** `class Tensor : public Expression<Tensor<T>>`. No analog in-tree; pattern from external sources (Eigen, xtensor — see CONTEXT.md `<canonical_refs>`).
- **`std::size_t`-indexed shape**, not `int64_t`. Tensor uses `shape_t = std::vector<std::size_t>` per CONTEXT.md `<specifics>`. Contrast with `Dimension::size` which is `int64_t` (database/binary subsystems use signed integer for SQL fidelity).

---

### `tests/test_tensor_storage.cpp` (GoogleTest unit test)

**Analog:** `tests/test_binary_metadata.cpp` (no fixture, fixture-less `TEST(Suite, Case)` style, identical include layout)

**Include block + namespace pattern** (`tests/test_binary_metadata.cpp:1-9`):
```cpp
#include <chrono>
#include <gtest/gtest.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/binary/dimension.h>
#include <quiver/binary/time_properties.h>
#include <quiver/element.h>

using namespace quiver;
using namespace std::chrono;
```

**Section-divider + TEST macro pattern** (`tests/test_binary_metadata.cpp:11-13, 40-46`):
```cpp
// ============================================================================
// Helper
// ============================================================================

static std::string make_valid_toml() {
    return R"(...)";
}

// ============================================================================
// BinaryMetadataFromToml
// ============================================================================

TEST(BinaryMetadataFromToml, AllFieldsPopulated) {
    auto md = BinaryMetadata::from_toml(make_valid_toml());
    EXPECT_EQ(md.version, "1");
    // ...
}
```

**Alternative fixture pattern** (`tests/test_database_lifecycle.cpp:13-21`) — only if Tensor<T> tests need shared setup (they likely don't, since constructors take all state):
```cpp
class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_test.db").string(); }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
};

TEST_F(TempFileFixture, OpenFileOnDisk) { ... }
```

**Throw assertion pattern** (`tests/test_database_lifecycle.cpp:89-94`):
```cpp
TEST_F(TempFileFixture, FromSchemaFileNotFound) {
    EXPECT_THROW(
        quiver::Database::from_schema(
            ":memory:", "nonexistent/path/schema.sql", {.read_only = false, .console_level = quiver::LogLevel::Off}),
        std::runtime_error);
}
```

**What to copy:**
- **No banner / header comment.** File starts directly with `#include` lines (CONVENTIONS.md "Comments": "No `@file` / banner headers"). `test_binary_metadata.cpp:1` is a bare `#include <chrono>` — same shape applies.
- **Include order:** standard library header → `<gtest/gtest.h>` → project public headers `<quiver/tensor/...>`. clang-format regroups; the result must compile with both orderings.
- **`using namespace quiver::tensor;`** at file scope (mirrors `using namespace quiver;` on `test_binary_metadata.cpp:8` — inside test files, full-qualification is dropped to keep assertions readable).
- **Section dividers:** 76-char `// ===…===` lines (CONVENTIONS.md "Comments"). Phase 1 sections per requirement coverage:
  - `// ElementTypes` (STG-01 — float/double/int32/int64)
  - `// TensorConstruction` (STG-01..02 — shape/size/rank getters)
  - `// TensorIndexing` (STG-03 — `op()`, `at()`)
  - `// TensorIteration` (STG-05 — `begin/end`)
  - `// ZeroDTensor` (D-11..D-13 — empty shape)
  - `// TensorMove` (REQ EXP-01 prerequisite — `noexcept` move)
  - `// IsTensorExprConcept` (EXP-02 — concept satisfaction)
- **Fixture-less `TEST(Suite, Case)`** for the bulk of cases (mirrors `test_binary_metadata.cpp`). Only introduce a fixture if a single test needs shared state — Tensor tests do not (constructors take every input directly).
- **`EXPECT_THROW` for the three Pattern-1 error sites** in `at()` / `.item()` / initializer-list constructor. Pattern from `test_database_lifecycle.cpp:89-94` — wrap the throwing expression, assert `std::runtime_error`.
- **Static asserts** for compile-time properties (per CONTEXT.md "Goal-Backward Verification Hooks" items 9-10):
  ```cpp
  static_assert(std::is_nothrow_move_constructible_v<quiver::tensor::Tensor<double>>);
  static_assert(quiver::tensor::IsTensorExpr<quiver::tensor::Tensor<double>>);
  static_assert(!quiver::tensor::IsTensorExpr<int>);
  static_assert(std::is_base_of_v<quiver::tensor::Expression<quiver::tensor::Tensor<double>>,
                                  quiver::tensor::Tensor<double>>);
  ```

**What to change vs analog:**
- No filesystem fixture (CONVENTIONS.md `tests/test_database_lifecycle.cpp:13-21` `TempFileFixture`) is needed — Tensor tests are pure-memory. Use `TEST(...)` not `TEST_F(...)`.
- Do **not** include `test_utils.h` (`tests/test_utils.h` provides `path_from`, `VALID_SCHEMA`, `quiet_options` — all are SQL/CFFI helpers irrelevant to header-only tensor tests). Tensor tests have zero filesystem touch.

---

### `cmake/CheckTensorIncludeContainment.cmake` (CI containment script)

**Analog:** **NO IN-TREE PRECEDENT** — flagged.

**Existing CMake-script context:** The project's `cmake/` directory contains `CompilerOptions.cmake`, `Dependencies.cmake`, `Platform.cmake`. All three are **modules included by `CMakeLists.txt`** (`include(CompilerOptions)`, etc., on `CMakeLists.txt:29-31`), not standalone scripts run via `cmake -P`. None of them invoke `add_test(NAME ... COMMAND ${CMAKE_COMMAND} -P ...)`.

**`grep` for any `add_test(NAME ...)` invocation in the project returns zero hits** (verified via `Grep "add_test"` against `**/CMakeLists.txt` — only `gtest_discover_tests` is used, in `tests/CMakeLists.txt:44-46, 88-90`).

**Closest precedent (in spirit, not in form):** the top-level `CMakeLists.txt:79-93` `file(GLOB_RECURSE ALL_SOURCE_FILES ...)` pattern, which enumerates project headers/sources for the `format` and `tidy` custom targets. The BLD-04 script will use the same `file(GLOB_RECURSE)` primitive but for a different purpose (regex-grep header content, fail on match).

**Reference excerpt** (`CMakeLists.txt:79-93`) for `file(GLOB_RECURSE)` syntax precedent only:
```cmake
# Source files for format and tidy targets
file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/include/quiver/*.h
    ${CMAKE_SOURCE_DIR}/include/quiver/c/*.h
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/binary/*.cpp
    ${CMAKE_SOURCE_DIR}/src/binary/*.h
    # ...
)
```

**What to author (no analog to copy from):**
1. Standalone CMake script (intended to be invoked with `cmake -P`), not an `include()`-d module. No `add_library`/`add_executable` directives — the script is purely procedural.
2. Use `file(GLOB_RECURSE)` to enumerate `include/quiver/*.h`, `include/quiver/c/*.h`, `include/quiver/binary/*.h`, `include/quiver/c/binary/*.h` — explicitly **excluding** `include/quiver/tensor/*.h` per CONTEXT.md D-17.
3. For each enumerated file, use `file(STRINGS ${path} matches REGEX "#[ \\t]*include[ \\t]*[<\"]quiver/tensor/")` to extract any line containing a tensor include.
4. If `matches` is non-empty for any file, accumulate all offending `${file}:${line}` pairs and emit `message(FATAL_ERROR "BLD-04 violation: ...")` listing them.
5. On clean tree, exit silently (CTest registers the command's exit-zero as PASS).

**Prior art (external):** This is a standard "CI containment grep" pattern (similar to LLVM's header-include guard checker), but no in-tree code uses the `cmake -P` script-as-test idiom. **Flagged as net-new pattern for the project** — planner should treat it as a discrete plan task with its own verification (the manual negative-path test in CONTEXT.md §"Done" Definition #4).

---

### `tests/CMakeLists.txt` (MOD — append test source + register `add_test`)

**Analog:** itself (modify-in-place — same file).

**Existing `add_executable` block to append to** (`tests/CMakeLists.txt:3-25`):
```cmake
add_executable(quiver_tests
    test_binary_file.cpp
    test_csv_converter.cpp
    test_binary_metadata.cpp
    test_binary_time_properties.cpp
    test_database_create.cpp
    # ... 16 more entries ...
    test_schema_validator.cpp
)
```

**Existing target_link_libraries pattern** (`tests/CMakeLists.txt:27-33`):
```cmake
target_link_libraries(quiver_tests
    PRIVATE
        quiver
        quiver_compiler_options
        GTest::gtest_main
        GTest::gmock
)
```

**Existing test discovery pattern** (`tests/CMakeLists.txt:44-46`):
```cmake
gtest_discover_tests(quiver_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:quiver_tests>
)
```

**`enable_testing()` already in scope** (`CMakeLists.txt:42-46`):
```cmake
if(QUIVER_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
```

**What to copy:**
- **Insert `test_tensor_storage.cpp`** alphabetically into the `add_executable(quiver_tests ...)` source list. The list is alphabetically sorted (verified by reading `tests/CMakeLists.txt:3-25`: `test_binary_*`, `test_csv_*`, `test_database_*`, `test_element`, `test_issues`, `test_lua_runner`, `test_migrations`, `test_row_result`, `test_schema_validator`). `test_tensor_storage.cpp` slots **between `test_schema_validator.cpp` and the closing `)`** as the new last entry alphabetically.
- **No new `target_link_libraries` call needed** — the new `.cpp` rolls into the existing `quiver_tests` target, which already links `quiver`, `quiver_compiler_options`, `GTest::gtest_main`, `GTest::gmock`. Tensor tests will pick up these dependencies transitively. (`quiver_compiler_options` provides the `/W4 /permissive-` MSVC flags that catch CRTP/concept misuse — see `cmake/CompilerOptions.cmake:5-10`.)
- **No new `gtest_discover_tests` call** — the existing one (`tests/CMakeLists.txt:44-46`) already covers all `TEST` and `TEST_F` cases inside any source file in `quiver_tests`. New `Tensor*` test names will appear automatically in `ctest` output.

**What to change vs analog:**
- **Add a new `add_test(NAME tensor_no_leakage ...)`** invocation. This is **net-new for the project** — no existing `add_test(NAME ... COMMAND ${CMAKE_COMMAND} -P ...)` pattern to copy. Per CONTEXT.md D-18:
  ```cmake
  add_test(NAME tensor_no_leakage
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/CheckTensorIncludeContainment.cmake)
  ```
- **Placement:** add the `add_test(...)` block **after** the existing `gtest_discover_tests(quiver_tests ...)` call (`tests/CMakeLists.txt:44-46`) and **before** the `if(QUIVER_BUILD_C_API)` block (`tests/CMakeLists.txt:48`). This keeps quiver-core test registration grouped and leaves the `quiver_c_tests` block undisturbed.
- **Do NOT** modify the `add_executable(quiver_c_tests ...)` block (`tests/CMakeLists.txt:50-67`) — Phase 1 ships zero C API surface (Pitfall #13 cross-phase discipline rule from RESEARCH.md).
- **Do NOT** modify the `quiver_benchmark` (`tests/CMakeLists.txt:94-96`) or `quiver_sandbox` (`tests/CMakeLists.txt:99-108`) blocks.

---

## Shared Patterns

### Three-Pattern Error Format

**Source:** `CLAUDE.md` "Error Handling" section + `.planning/codebase/CONVENTIONS.md` lines 124-156
**Apply to:** `include/quiver/tensor/tensor.h` (every `throw std::runtime_error(...)` in Phase 1)

```cpp
// Pattern 1 — Precondition: "Cannot {operation}: {reason}"
throw std::runtime_error("Cannot construct tensor: initializer_list size " + std::to_string(got)
                         + " does not match shape size " + std::to_string(expected));

throw std::runtime_error("Cannot access tensor: rank mismatch (expected " + std::to_string(rank)
                         + " indices, got " + std::to_string(n) + ")");

throw std::runtime_error("Cannot item: tensor has " + std::to_string(N) + " elements (expected 1)");
```

Phase 1 uses **only Pattern 1** (`Cannot {op}: {reason}`). Patterns 2 (`{Entity} not found:`) and 3 (`Failed to {op}:`) are not exercised this phase per CONTEXT.md `<code_context>`.

### `noexcept` Move Discipline

**Source:** `include/quiver/database.h:23-29` (resource type), `include/quiver/binary/binary_file.h:24-30` (resource type)
**Apply to:** `include/quiver/tensor/tensor.h` `Tensor<T>` (value type, but `noexcept` move still required to avoid Anti-Pattern #6 "silent copy-on-grow when stored in `std::vector<Tensor<T>>`" per CONTEXT.md `<code_context>` "Established Patterns")

```cpp
// Database is non-copyable, movable (resource handle):
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&& other) noexcept;
Database& operator=(Database&& other) noexcept;

// Tensor<T> is COPYABLE + movable (value type):
Tensor(const Tensor&) = default;
Tensor& operator=(const Tensor&) = default;
Tensor(Tensor&&) noexcept = default;
Tensor& operator=(Tensor&&) noexcept = default;
~Tensor() = default;
```

The `noexcept` annotation on move ops is the critical bit. Without it, `std::vector<Tensor<T>>::push_back` falls back to copy on grow (Anti-Pattern #6 in research). Verified via `static_assert(std::is_nothrow_move_constructible_v<Tensor<double>>)` in `tests/test_tensor_storage.cpp` (CONTEXT.md "Goal-Backward Verification Hooks" item 9).

### Header Include Guard + Namespace Convention

**Source:** `include/quiver/binary/dimension.h:1-3,22-24`, `include/quiver/binary/time_properties.h:1-3,34-36`, `include/quiver/binary/binary_metadata.h:1-3,44-46`
**Apply to:** all three new tensor headers.

```cpp
// expression.h
#ifndef QUIVER_TENSOR_EXPRESSION_H
#define QUIVER_TENSOR_EXPRESSION_H

#include "../export.h"
// ... template/concept declarations ...

namespace quiver::tensor {
// ...
}  // namespace quiver::tensor

#endif  // QUIVER_TENSOR_EXPRESSION_H
```

Convention details:
- Header guard: `QUIVER_TENSOR_<FILENAME>_H` (`QUIVER_TENSOR_EXPRESSION_H`, `QUIVER_TENSOR_SHAPE_H`, `QUIVER_TENSOR_TENSOR_H`).
- Namespace: `namespace quiver::tensor` (C++17 nested namespace syntax — used in `tests/test_utils.h:8` via `namespace quiver::test`, so the codebase already accepts this style).
- Trailing `}  // namespace quiver::tensor` comment per `clang-format` `FixNamespaceComments: true`.

### Test File Convention

**Source:** `tests/test_binary_metadata.cpp:1-13`, `tests/test_database_lifecycle.cpp:1-11`
**Apply to:** `tests/test_tensor_storage.cpp`

```cpp
#include <gtest/gtest.h>
#include <quiver/tensor/expression.h>
#include <quiver/tensor/shape.h>
#include <quiver/tensor/tensor.h>

#include <type_traits>
#include <vector>

using namespace quiver::tensor;

// ============================================================================
// ElementTypes
// ============================================================================

TEST(ElementTypes, AllFourTypesConstruct) {
    Tensor<float> tf({2, 3}, 1.0f);
    Tensor<double> td({2, 3}, 1.0);
    Tensor<int32_t> ti({2, 3}, int32_t{1});
    Tensor<int64_t> tl({2, 3}, int64_t{1});
    EXPECT_EQ(tf.size(), 6u);
    // ...
}
```

- No banner / file header comment.
- `using namespace quiver::tensor;` at file scope (drops the long prefix from assertions).
- 76-char `// ====` section dividers between test groups.
- `TEST(...)` (no fixture) for stateless tests; `TEST_F(...)` only if shared setup is needed (Phase 1 has none).

---

## No Analog Found

| File | Role | Reason |
|------|------|--------|
| `cmake/CheckTensorIncludeContainment.cmake` | CMake script invoked via `cmake -P` from CTest | The project's three existing `cmake/*.cmake` files (`CompilerOptions`, `Dependencies`, `Platform`) are all `include()`-d modules, **never** standalone scripts. The codebase has zero `add_test(NAME ... COMMAND ${CMAKE_COMMAND} -P ...)` invocations. The `file(GLOB_RECURSE)` primitive used at top-level `CMakeLists.txt:79-93` is the closest syntactic precedent, but it serves `find_program`-driven `format`/`tidy` custom targets, not CTest scripts. **Treat as net-new pattern for Quiver.** |

Additionally:
- **CRTP base class** (`Expression<Derived>`) has no in-tree analog. Patterns must come from the cited external sources (Eigen `MatrixBase<Derived>`, xtensor `xexpression<E>`) per CONTEXT.md `<canonical_refs>`.
- **C++20 concept declaration** (`IsTensorExpr`) has no in-tree analog — Quiver's existing C++20 usage is limited to designated initializers (e.g., `{.read_only = false, .console_level = ...}`). The `is_base_of_template_v` idiom is documented in RESEARCH.md "State of the Art" as the recommended approach.

---

## Metadata

**Analog search scope:** `include/quiver/`, `tests/`, `cmake/`, top-level `CMakeLists.txt`, project `CLAUDE.md`, codebase docs `.planning/codebase/{STRUCTURE,ARCHITECTURE,CONVENTIONS}.md`
**Files scanned:** 14 source/header/CMake/markdown files (verified read; non-overlapping ranges)
**Pattern extraction date:** 2026-05-05

## PATTERN MAPPING COMPLETE

**Phase:** 1 - Storage and CRTP Scaffold
**Files classified:** 6 (5 NEW + 1 MOD)
**Analogs found:** 5 / 6

### Coverage
- Files with strong analog (header value-type, GoogleTest, CMakeLists.txt MOD): 5
- Files with role-match-only analog (CRTP/concept patterns are external — see CONTEXT.md `<canonical_refs>`): 0 in-tree
- Files with no in-tree analog: 1 (`cmake/CheckTensorIncludeContainment.cmake`)

### Key Patterns Identified
- **Header-only value type with Rule of Zero** (`include/quiver/binary/dimension.h`, `time_properties.h`) — direct precedent for `tensor.h`/`expression.h`/`shape.h` layout, namespace style, include-guard convention. New tensor headers nest under `quiver::tensor` (subsystem ns), not root `quiver`.
- **`noexcept` move ops on copyable value type** — Tensor<T> diverges from `Database`'s non-copyable resource pattern: Tensor keeps copy enabled while requiring `noexcept = default` on move (Anti-Pattern #6 prevention).
- **Three-pattern error format (Pattern 1 only)** — all four Phase 1 throws are `Cannot {op}: {reason}` per CLAUDE.md. Patterns 2 and 3 unused this phase.
- **GoogleTest fixture-less `TEST()` style** with 76-char `// ====` section dividers and `using namespace quiver::tensor;` (mirrors `tests/test_binary_metadata.cpp`).
- **`gtest_discover_tests` already covers new tests automatically** — no changes to discovery wiring; `add_executable` source list append is the only `quiver_tests`-target edit needed.
- **NET-NEW pattern flagged:** `add_test(NAME ... COMMAND ${CMAKE_COMMAND} -P ...)` for the BLD-04 leakage check. No precedent in `tests/CMakeLists.txt` or any `cmake/*.cmake` module; planner authors fresh from CONTEXT.md D-15..D-18.

### File Created
`C:\Development\DatabaseTmp\quiver\.planning\phases\01-storage-and-crtp-scaffold\01-PATTERNS.md`

### Ready for Planning
Pattern mapping complete. Planner can now reference these analog excerpts (with file:line citations) directly inside each PLAN.md action block.
