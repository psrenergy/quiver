# Phase 7: AbstractExpression in C++ - Pattern Map

**Mapped:** 2026-10-04
**Files analyzed:** 12 (1 new, 10 modified, 1 harness extended)
**Analogs found:** 12 / 12

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `include/quiver/expression/abstract_expression.h` (NEW) | public header / polymorphic base | transform | `include/quiver/expression/expression_node.h` (`ExpressionNode`: exported polymorphic base) + `expression.h` (guard/includes) | exact |
| `include/quiver/expression/expression.h` | public header | transform | itself (lines 1-125) | self |
| `include/quiver/expression/expression_node.h` | public header | transform | itself, `ExpressionAggregateAgents` alias line 172 | self |
| `include/quiver/binary/binary_file.h` | public header | file-I/O | itself (lines 17-46) | self |
| `src/expression/expression.cpp` | core impl | transform | itself (lines 18-110) | self |
| `src/expression/expression_file.cpp` | core impl | file-I/O | itself (`ExpressionFile` ctor line 10) | self |
| `src/c/expression/expression.cpp:248` | C API accessor | request-response | itself | self (1-token edit) |
| `src/lua_runner/expression.cpp:117` | Lua binder | request-response | itself | self (1-token edit) |
| `src/lua_runner/binary.cpp:8-9` | Lua binder comment | - | itself | self |
| `tests/test_expression.cpp` | test | transform | `ExpressionFixture` (lines 21-110) | exact |
| `CHANGELOG.md` | docs | - | `## [0.13.0] — unreleased` / `### Changed` BREAKING entries (lines 8-40) | exact |
| `build/fixes-check/linux.sh` (gitignored harness) | test script | batch | itself | self |
| `src/AGENTS.md` (lines per RESEARCH OQ4) | docs | - | itself | self |

## Pattern Assignments

### `include/quiver/expression/abstract_expression.h` (NEW)

**Analog:** `include/quiver/expression/expression.h` lines 1-15 (guard style is `#ifndef`, NOT `#pragma once`; relative includes; `namespace quiver {`):
```cpp
#ifndef QUIVER_EXPRESSION_H
#define QUIVER_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"
#include "expression_node.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiver {
```
Use guard `QUIVER_ABSTRACT_EXPRESSION_H`. Include ONLY `"../binary/binary_metadata.h"` and `"../export.h"` (never `binary_file.h` or `expression_node.h` — cycle). Forward-declare `class Expression; class ExpressionNode;`.

**Exported polymorphic base precedent** (`expression_node.h:18-21`):
```cpp
class QUIVER_API ExpressionNode {
public:
    virtual ~ExpressionNode() = default;
```
Body: copy RESEARCH.md Pattern 1 verbatim (one pure `node()`, non-pure virtual `get_metadata()`, protected defaulted copy/move, `enum class AggregateOperation` at namespace scope). Close with `}  // namespace quiver` and `#endif  // QUIVER_ABSTRACT_EXPRESSION_H` matching existing headers.

---

### `include/quiver/expression/expression.h`

Current shape to replace (lines 17-41): `class QUIVER_API Expression {`, implicit `Expression(const BinaryFile& file);`, `const BinaryMetadata& metadata() const;`, and the five method declarations taking `ExpressionAggregate::Operation`. Lines 43-89: the `friend QUIVER_API ...` block — delete entirely, `node_` stays private.

New: `class QUIVER_API Expression final : public AbstractExpression`, `explicit Expression(std::shared_ptr<ExpressionNode>)`, `explicit Expression(const AbstractExpression&)`, `std::shared_ptr<ExpressionNode> node() const override;`. Methods move to the base.

Free operator declarations (lines 95-125+) keep the three-combo layout, only the type changes:
```cpp
QUIVER_API Expression operator+(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator+(const Expression& lhs, double rhs);
QUIVER_API Expression operator+(double lhs, const Expression& rhs);
```
→ `const AbstractExpression&`. `ifelse` stays three expression parameters (no double overload). Reword comment at lines 118-121 (no word `friend` anywhere in the file).

---

### `include/quiver/expression/expression_node.h`

Line 142 `enum class Operation { Sum, Mean, Min, Max, Percentile };` → `using Operation = AggregateOperation;`. Copy the alias style already at line 172:
```cpp
    // The same five reductions as ExpressionAggregate: one enum serves both (and the C API and Lua).
    using Operation = ExpressionAggregate::Operation;
```
Optionally add `#include "abstract_expression.h"` beside the existing `#include "../binary/binary_file.h"` (line 4).

---

### `include/quiver/binary/binary_file.h`

Lines 4-5 includes become add `#include "../expression/abstract_expression.h"`. Lines 17-19:
```cpp
class QUIVER_API BinaryFile {
public:
    explicit BinaryFile(const std::string& file_path);
    ~BinaryFile();
```
→ `class QUIVER_API BinaryFile : public AbstractExpression {`, `~BinaryFile() override;`, add `std::shared_ptr<ExpressionNode> node() const override;`, mark `const BinaryMetadata& get_metadata() const override;` (both `override` required: clang `-Winconsistent-missing-override`, tidy `modernize-use-override`). Body of `get_metadata` in `src/binary/binary_file.cpp:310-312` unchanged. Copy ctor `= delete` stays.

---

### `src/expression/expression.cpp`

**Imports** (lines 1-14) unchanged; add `#include "quiver/expression/abstract_expression.h"` if desired.

**Ctor/accessor to replace** (lines 18-24):
```cpp
Expression::Expression(const BinaryFile& file) : node_(std::make_shared<ExpressionFile>(file.get_file_path())) {}
Expression::Expression(std::shared_ptr<ExpressionNode> node) : node_(std::move(node)) {}
const BinaryMetadata& Expression::metadata() const {
    return node_->metadata();
}
```
→ `Expression::Expression(const AbstractExpression& e) : node_(e.node()) {}`, out-of-line `Expression::node() const { return node_; }`, `AbstractExpression::get_metadata() const { return node()->metadata(); }`.

**Methods** (lines 26-47) become `AbstractExpression::` members with `node_` → `node()`, e.g.:
```cpp
Expression Expression::select_agents(const std::vector<std::string>& labels) const {
    return Expression(std::make_shared<ExpressionSelectAgents>(node_, labels));
}
```

**save** (lines 49-96): rename to `AbstractExpression::save`; first line `const auto root = node();`, replace every `node_->` (lines 51, 76, 85) with `root->`. Error text `"Cannot save: output path collides with input file '"` and `CloseOnExit` guard unchanged.

**Operators** (lines 98+, today two accesses per scalar operand):
```cpp
Expression operator+(const Expression& lhs, double rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(rhs, lhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Add, lhs.node_, scalar));
}
```
→ anonymous-namespace helpers `binary(op, AE, AE)`, `binary(op, AE, double)`, `binary(op, double, AE)`, `unary(op, AE)` per RESEARCH Pattern 3 (one `node()` local per operand, declaration order, scalar uses `l->metadata()` NOT `get_metadata()`); `ifelse` with c/t/e locals. Each operator becomes a one-liner.

---

### `src/expression/expression_file.cpp`

Includes (lines 1-6) already have `binary_file.h` + `expression_node.h`. Add after `ExpressionFile` members, inside `namespace quiver`:
```cpp
std::shared_ptr<ExpressionNode> BinaryFile::node() const {
    return std::make_shared<ExpressionFile>(get_file_path());
}
```
(identical to today's `expression.cpp:18` body). Add `#include <memory>`. The leaf ctor (line 10) builds its own `file_(path)`, so the caller's handle is never touched.

---

### `src/c/expression/expression.cpp:248` and `src/lua_runner/expression.cpp:117`

```cpp
*out = new quiver_binary_metadata{expression->expression.metadata()};   // C API :248
[](Expression& self) -> BinaryMetadata { return self.metadata(); },     // Lua :117, name "metadata" stays
```
→ `.get_metadata()`. Nothing else in `src/c/` changes (`:103` `quiver::Expression(file->binary_file)` and Lua `to_expression` `Expression(o.as<BinaryFile&>())` at lines 37-39 are already direct-init).

### `src/lua_runner/binary.cpp:8-9`

Keep the include; reword only:
```cpp
// Kept although nothing here names Expression: sol2 derives BinaryFile's automatic __lt/__le/__eq from the
// Expression operators (through the implicit Expression(const BinaryFile&)) when the usertype is created.
```
→ "...from the operators taking `const AbstractExpression&`, which a BinaryFile is..."

---

### `tests/test_expression.cpp` (test)

**Analog:** `ExpressionFixture` lines 1-110. Includes (lines 1-16) — add `#include <type_traits>` (and `quiver/expression/abstract_expression.h` optional, it comes via binary_file.h). Fixture provides `path_a/b/c/out/out2`, `make_simple_metadata()` (dims `row`,`col`, sizes 3x2, labels `val1`,`val2`), `make_single_label_metadata(label, unit)`, `write_qvr(path, meta, fn)`, `read_all_cells(path)` (line 100). Value-check idiom, line 132-133:
```cpp
    auto orig = read_all_cells(path_a);
    auto copy = read_all_cells(path_out);
```
Append new `TEST_F(ExpressionFixture, ...)` after the last one (line 2790 `LogicalDrivesIfElse`). New tests from RESEARCH "Code Examples": `AbstractExpressionShape` (static_asserts only — no existing static_assert tests in the repo, this is the first), `FileStaysOpenAndReadableAfterSave`, `ExpressionOutlivesItsFile`, no-wrapper operands, `get_metadata` address identity on unopened `BinaryFile(path_a)`, `node()` call counting via test-local `final` subclass of `AbstractExpression`. 26 `.metadata()` → `.get_metadata()` renames on Expression locals.

---

### `CHANGELOG.md`

Insert under `## [0.13.0] — unreleased` / `### Changed`, same format as line 12:
```markdown
- **BREAKING** **Lua table arguments are type-checked.** A value other than a table passed where
  a Lua method takes a table now raises ...
  ... Pass a table, e.g. ...
```
(bold BREAKING, bold one-sentence title, ~100-col wrap, ends with what the caller must do). Use the two entries drafted in RESEARCH "CHANGELOG lines" verbatim.

---

### `build/fixes-check/linux.sh` (gitignored harness)

Extend the two Lua-only filter lines:
```bash
./build/bin/quiver_tests --gtest_filter='Lua*' --gtest_brief=1 2>&1 | grep -E '^\[  (PASSED|FAILED|SKIPPED) |tests from .* ran'
./build/bin/quiver_c_tests --gtest_filter='LuaRunnerCApiTest.*' --gtest_brief=1 2>&1 | grep -E ...
```
→ add unfiltered `quiver_tests` and `quiver_c_tests` runs (same grep). Harness uses `git archive HEAD` → commit first. Invoke via `bash build/fixes-check/linux.sh gcc|clang`.

## Shared Patterns

- **Export macro:** every public class/free function is `QUIVER_API` (`include/quiver/export.h`); apply to `AbstractExpression` and all operator declarations.
- **Header guards:** `#ifndef QUIVER_<NAME>_H` / `#define` / `#endif`; relative `"../"` includes inside `include/quiver/`; `"quiver/..."` includes in `src/`.
- **Error messages:** none added; `save`'s Pattern 1 message unchanged.
- **Node evaluation:** one `node()` call per operand, stored in a local (save, operators, ifelse).

## No Analog Found

None. (static_assert-based shape test has no precedent in `tests/`; follow RESEARCH "Shape test".)

## Metadata

**Analog search scope:** `include/quiver/{expression,binary}`, `src/expression`, `src/c/expression`, `src/lua_runner`, `tests/test_expression.cpp`, `CHANGELOG.md`, `build/fixes-check`
**Pattern extraction date:** 2026-10-04
