# Phase 7: AbstractExpression in C++ - Research

**Researched:** 2026-10-04
**Domain:** C++20 class hierarchy refactor (polymorphic exported base, operator overloads with base-class parameters) in the Quiver expression subsystem
**Confidence:** HIGH (Windows tree read in full; hierarchy shape compiled and run on GCC 13.3, GCC 14.4 and Clang 18.1.3/libc++, including across a `-fvisibility=hidden` shared-library boundary)

## Summary

This phase is a contained refactor. Eight source files change, plus one new header, one test file, and the CHANGELOG. There are no new dependencies and no C API signature change. The milestone design study (`.planning/research/DESIGN-STUDY.md` lines 1048-1323 and 1664-1760, with the judges' merge at lines 2062-2072) already sketches the code. This research checks that sketch against the current tree (after Phase 6) and against the locked success criteria. It found three places where the literal spec and the sketch disagree, and the planner has to settle them (see Open Questions):
1. Whether `get_metadata` is virtual. It must be, and it must not be pure.
2. `ifelse(file > 1.0, file, 0.0)` does not compile, because `ifelse` has no `double` overload.
3. Phase 7 must also produce the two baselines that Phase 8's success criteria compare against.

Today `Expression(const BinaryFile&)` is implicit and copies only the path. `ExpressionFile` re-reads the `.toml` and owns its own unopened `BinaryFile`, which `save()` opens and closes. So the ownership guarantee is already in place: `BinaryFile::node()` returns exactly that leaf, and `AbstractExpression::save` must hold the root in a local. The 40 friend declarations exist only because the operators read `node_`. Once `node()` is public they go, and the 36 operator bodies collapse onto four small helpers that each call `node()` once per operand.

**Primary recommendation:** Follow the design study's code sketch. Write one new header, `abstract_expression.h`, with `AggregateOperation` at namespace scope. It declares `node()` as the one pure virtual, and `get_metadata()` as a non-pure virtual whose base body `return node()->metadata();` serves `Expression` and which `BinaryFile` overrides with the handle's in-memory copy. Put the method bodies in `src/expression/expression.cpp` and `BinaryFile::node()` in `src/expression/expression_file.cpp`. Make the constructor `explicit Expression(const AbstractExpression&)`. Put about 6 new `ExpressionFixture` tests in `tests/test_expression.cpp`, plus the 26 mechanical `.metadata()` to `.get_metadata()` renames there. Make one accessor edit each in `src/c/expression/expression.cpp:248` and `src/lua_runner/expression.cpp:117`, and fix one stale comment in `src/lua_runner/binary.cpp:8-9`.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Abstract expression type, operators, methods | C++ core (`include/quiver/expression`, `src/expression`) | — | Root rule: logic lives in C++; bindings stay thin |
| File as expression leaf (`BinaryFile::node()`) | C++ core (`src/expression/expression_file.cpp`) | — | Builds an `ExpressionFile`, so it lives beside the leaf it constructs |
| Metadata accessor rename | C++ core | C API (one call site), Lua binder (one call site, Lua name unchanged) | C ABI and the Lua surface stay frozen this phase |
| Lua `f == g` / `f < g` automagic | sol2 in `src/lua_runner/binary.cpp` | — | Must keep seeing the operators (keep the `expression.h` include) |
| Julia | none this phase | — | `c_api.jl` must be untouched (Phase 9 owns Julia) |

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| EXPR-01 | `abstract_expression.h`: `class QUIVER_API AbstractExpression`, public virtual dtor, protected copy/move, exactly one pure virtual `node()`, non-virtual `save`/`aggregate`/`aggregate_agents`/`select_agents`/`rename_agents` built on `node()` | Pattern 1; shape probe compiled on GCC 13/14 and Clang 18/libc++ (Code Examples, Environment) |
| EXPR-02 | `Expression final` keeps `node_`; `BinaryFile::node()` returns a fresh path-based `ExpressionFile`; `save` holds the root in a local; ownership tests | Pattern 2; Pitfall 1; test list |
| EXPR-03 | Every operator and free function takes `const AbstractExpression&`; `double` overloads stay; friend block deleted; `node()` once per operand | Pattern 3; Pitfalls 3 and 4; Open Question 2 (`ifelse` has no double overload) |
| EXPR-04 | `explicit Expression(const AbstractExpression&)` replaces the implicit ctor; direct-init sites compile | Every call site in the tree is direct-init (grep below); CHANGELOG text |
| EXPR-05 | `get_metadata()` is the one accessor; `Expression::metadata()` renamed; file returns the in-memory copy | Open Question 1 / Pattern 1; 26 + 1 + 1 rename sites |
| EXPR-06 | `quiver::AggregateOperation` at namespace scope; alias kept; no C API change; only `src/c/` edit is line 248 | Pattern 4; the C API uses `ExpressionAggregate::Operation` through the alias (lines 77-88) |
| EXPR-07 | Linux GCC 13 + Clang 18/libc++ pass C++, C API, Lua suites | Probe results; Linux harness extension (Validation section) |
</phase_requirements>

## Project Constraints (from AGENTS.md)

No `CLAUDE.md` exists. `./AGENTS.md` (root), `src/AGENTS.md` and `tests/AGENTS.md` are the project instructions.
- WIP project: breaking changes are acceptable and there is no backwards compatibility. **Delete unused code, do not deprecate.** Keep no leftover implicit constructor and no `metadata()` alias.
- Clean code over defensive code. Calling `node()` on a moved-from `BinaryFile` dereferences a null `impl_`, as `Expression(moved_from)` already does today. Document it as a precondition; do not guard it.
- Error messages follow Patterns 1/2/3. This phase adds no new message.
- **Changelog:** user-visible changes go under `## [0.13.0] — unreleased`. A breaking entry is prefixed **BREAKING** and says what the caller must do. No planning IDs in entries. No version bump (memory: the entry goes under the next untagged version).
- **Self-Updating:** keep the nearest AGENTS.md accurate. Note that the roadmap puts the full AGENTS.md rewrite (DOC-01) in Phase 9. See Open Question 4.
- C++20, Rule of Zero for plain value types. Pimpl only to hide private dependencies (`BinaryFile` already uses it).
- Do-Not-Fix: the binary hot-path decisions in `src/AGENTS.md`. This phase does not touch `read`/`write`.
- Python runs through `uv`. Git Bash commands use `MSYS_NO_PATHCONV=1` for Docker volume paths.
- Commit messages end with the Co-Authored-By and Claude-Session lines from the session reminder.

## Standard Stack

No new packages. Everything is in-tree: C++20, MSVC (`/W4 /permissive-`), GCC/Clang (`-Wall -Wextra -Wpedantic -Wno-unused-parameter`) [VERIFIED: cmake/CompilerOptions.cmake:6-20], googletest v1.17.0, and sol2 v3.5.0, which is touched only to keep compiling.

## Package Legitimacy Audit

Not applicable. This phase installs no external package.

## Current Code Facts (verified this session)

- `include/quiver/expression/expression.h:17-21` [VERIFIED: Read via cat -n]: `class QUIVER_API Expression {` / `Expression(const BinaryFile& file);` (implicit) / `explicit Expression(std::shared_ptr<ExpressionNode> node);` / `const BinaryMetadata& metadata() const;`. Lines 43-89 hold 43 `friend QUIVER_API ...` declarations (`git grep -n friend` = 43 lines). Lines 118-121 hold a comment claiming the compiler "does not synthesize" C++20 reversed candidates. That is inaccurate; see Pitfall 5.
- `src/expression/expression.cpp:18` [VERIFIED]: `Expression::Expression(const BinaryFile& file) : node_(std::make_shared<ExpressionFile>(file.get_file_path())) {}`, so only the path is copied.
- `src/expression/expression_file.cpp:10` [VERIFIED]: `ExpressionFile::ExpressionFile(const std::string& path) : meta_(BinaryMetadata::from_toml_file(path)), file_(path) {` (own `BinaryFile`, metadata from disk); `collect_input_files` pushes `&file_` (line 25-27).
- `src/expression/expression.cpp:49-96` [VERIFIED]: `save` collects inputs, does the `weakly_canonical` collision check (`"Cannot save: output path collides with input file '"`), opens each input `'r'` under a `CloseOnExit` guard, then writes. The scalar overloads call `lhs.metadata()` and then `lhs.node_`, which is two accesses (lines 101-108 and the like).
- `include/quiver/binary/binary_file.h:17-46` [VERIFIED: Read]: `class QUIVER_API BinaryFile {`, `~BinaryFile();`, copy `= delete`, `BinaryFile(BinaryFile&& other) noexcept;`, `BinaryFile& operator=(BinaryFile&& other) noexcept;`, `const BinaryMetadata& get_metadata() const;`, `const std::string& get_file_path() const;`. This is the same signature the base virtual must have.
- `src/binary/binary_file.cpp:45-46` [VERIFIED]: the path constructor sets `BinaryMetadata{}`, so an unopened handle's `get_metadata()` is empty while its `.toml` is not. That is the cleanest test that the accessor returns the in-memory copy. `get_metadata()` is at lines 310-312 and returns `impl_->metadata`. The moves are `= default` out-of-line (lines 49-50).
- `include/quiver/expression/expression_node.h:140-142` [VERIFIED: Read]: `class QUIVER_API ExpressionAggregate final : public ExpressionNode {` / `enum class Operation { Sum, Mean, Min, Max, Percentile };`; line 172: `using Operation = ExpressionAggregate::Operation;` (in `ExpressionAggregateAgents`). `expression_node.h` includes `binary_file.h`, so `abstract_expression.h` cannot include `expression_node.h` (that would be a cycle).
- `include/quiver/binary/binary_metadata.h` includes only `../element.h`, `../export.h` and `dimension.h`. It declares `struct QUIVER_API BinaryMetadata` (class-key `struct`). It is safe to include from `abstract_expression.h`, so no forward declaration is needed (avoids MSVC C4099 struct/class mismatch).
- `src/c/expression/expression.cpp:103` [VERIFIED: Read]: `*out = new quiver_expression(quiver::Expression(file->binary_file));` is direct-init, so it compiles with `explicit`. `:248` [VERIFIED: Read]: `*out = new quiver_binary_metadata{expression->expression.metadata()};` is the only `src/c/` edit (becomes `get_metadata()`). `dispatch` (lines 14-43) is a template over `const Lhs&`/`const Rhs&` and resolves to the new base-parameter operators unchanged. `from_c` (77-88) spells `quiver::ExpressionAggregate::Operation::Sum` and so on, which compiles through the alias.
- `src/c/internal.h:32-40` [VERIFIED]: `struct quiver_binary_file { quiver::BinaryFile binary_file; ... }` and `struct quiver_expression { quiver::Expression expression; ... }` hold by value, so a vptr is added to each and the C ABI does not change (opaque handles).
- `src/lua_runner/expression.cpp:33-41` [VERIFIED]: `to_expression` returns `o.as<Expression>()` or `Expression(o.as<BinaryFile&>())`, which is direct-init. `:117`: `[](Expression& self) -> BinaryMetadata { return self.metadata(); },` must become `self.get_metadata()` while the Lua name stays `"metadata"`. `parse_aggregate_op` returns `ExpressionAggregate::Operation` (alias, no change).
- `src/lua_runner/binary.cpp:8-10` [VERIFIED]: a comment says the include is kept because sol2 derives BinaryFile `__lt/__le/__eq` "through the implicit Expression(const BinaryFile&)". After this phase the mechanism is the base-class operators. Keep the include and reword the comment. `src/AGENTS.md:667-670` says no test pins this ("removing it changes `f < g` and `f == g` in scripts while every test stays green").
- sol2 automagic [VERIFIED: build/_deps/sol2-src/include/sol/usertype_core.hpp:130-150, stack_core.hpp:1388-1418]: `__eq`/`__lt`/`__le` are registered when `std::declval<T&>() == std::declval<T&>()` is well-formed. `comparsion_operator_wrap` pushes whatever `op(l, r)` returns, here an `Expression` userdata, which is truthy. With base-class operators, `BinaryFile& == BinaryFile&` stays well-formed, so the settled "`f == g` is true" behaviour is unchanged.
- Rename sites [VERIFIED: git grep -c '\.metadata()']: `tests/test_expression.cpp` has 26, all on `Expression` locals (`e`, `out`, `neg`). `src/expression/expression.cpp` has 24, which disappear when the operator bodies collapse. `src/c/expression/expression.cpp` has 1 and `src/lua_runner/expression.cpp` has 1. `tests/test_lua_binary.cpp` has 2 `quiver.metadata()` hits, which are a different function: do not touch them. Lua `agg:metadata()` in `tests/test_lua_expression.cpp:238,240,302` stays (Lua keeps `metadata` until Phase 8).
- Copy-init from a file [VERIFIED: grep]: there are none in `tests/` or `src/`. All 115 `Expression(` uses in tests are direct-init. The only `const Expression&` parameters outside the core are the C API `dispatch_unary`/`dispatch_ternary` and the test lambdas at `tests/test_expression.cpp:2558-2563`, and they always receive `Expression`s.
- Visibility [VERIFIED: Read cmake/Platform.cmake:41-43]: `set(CMAKE_CXX_VISIBILITY_PRESET hidden)`, `set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)`. `QUIVER_API` is `__attribute__((visibility("default")))` off Windows and `__declspec(dllexport/dllimport)` on Windows (`include/quiver/export.h`). Precedent: `ExpressionNode` is already an exported polymorphic base with an inline `virtual ~ExpressionNode() = default;` and passes Linux CI.
- Baseline counts [VERIFIED: gtest --gtest_list_tests on build/bin at HEAD 5760d17]: `ExpressionFixture` 116, `Lua*` 477, `quiver_tests` 1454, `ExpressionCApiFixture` 73, `quiver_c_tests` 543. Phase 6 records the same counts in Release (`06-03-SUMMARY.md:68`).

## Architecture Patterns

### System Architecture Diagram

```
caller (C++ test / C API dispatch / Lua binop via to_expression)
   │  const AbstractExpression& operands (+ double)
   ▼
operator / free fn ──► helper binary()/unary()/ifelse: node() ONCE per operand, into locals
   │                          │
   │      ┌───────────────────┴──────────────────────┐
   │      ▼ virtual dispatch                          ▼
   │  Expression::node() → node_ (shared)     BinaryFile::node() → make_shared<ExpressionFile>(path)
   │                                            (reads .toml, owns its OWN unopened BinaryFile)
   ▼
new Expression(make_shared<ExpressionBinary|Unary|Ternary|...>(...))
   │
   ▼ AbstractExpression::save(path)
const auto root = node();  ─► root->collect_input_files() ─► collision check ─► open leaves 'r'
   ─► compute_row per cell ─► BinaryFile::open_file(path,'w') writer ─► CloseOnExit closes leaves
   (the caller's BinaryFile handle is never collected, opened or closed)
```

### Files Touched

```
include/quiver/expression/abstract_expression.h   NEW: AggregateOperation + AbstractExpression
include/quiver/expression/expression.h            Expression final; operators on the base; friends gone
include/quiver/expression/expression_node.h       ExpressionAggregate: using Operation = AggregateOperation;
include/quiver/binary/binary_file.h               : public AbstractExpression; node() override; get_metadata() override; ~BinaryFile() override
src/expression/expression.cpp                     base methods + Expression members + helper-based operators
src/expression/expression_file.cpp                BinaryFile::node()
src/c/expression/expression.cpp:248               .metadata() -> .get_metadata()
src/lua_runner/expression.cpp:117                 .metadata() -> .get_metadata() (Lua name "metadata" unchanged)
src/lua_runner/binary.cpp:8-9                     comment reworded (include kept)
tests/test_expression.cpp                         26 renames + ~6 new ExpressionFixture tests
CHANGELOG.md                                      two BREAKING lines
```
No CMake change: there is no new `.cpp`, and the new header is header-only. No `tests/CMakeLists.txt` change either.

### Pattern 1: The base class (EXPR-01, EXPR-05, EXPR-06)

```cpp
// include/quiver/expression/abstract_expression.h  (sketch: DESIGN-STUDY.md:1121-1174, adjusted per Open Question 1)
#include "../binary/binary_metadata.h"
#include "../export.h"
#include <memory> <optional> <string> <utility> <vector>
namespace quiver {
class Expression;
class ExpressionNode;

// at namespace scope: this header cannot include expression_node.h (which includes binary_file.h -> here)
enum class AggregateOperation { Sum, Mean, Min, Max, Percentile };

class QUIVER_API AbstractExpression {
public:
    virtual ~AbstractExpression() = default;
    virtual std::shared_ptr<ExpressionNode> node() const = 0;
    // node()->metadata(): right for an Expression, whose node_ keeps the node alive. BinaryFile overrides
    // it (its node() is a fresh temporary leaf, and it must return the handle's in-memory copy).
    virtual const BinaryMetadata& get_metadata() const;

    void save(const std::string& path) const;
    Expression aggregate(const std::string& dimension, AggregateOperation operation,
                         std::optional<double> parameter = std::nullopt) const;
    Expression aggregate_agents(AggregateOperation operation, std::optional<double> parameter = std::nullopt) const;
    Expression select_agents(const std::vector<std::string>& labels) const;
    Expression rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const;

protected:
    AbstractExpression() = default;
    AbstractExpression(const AbstractExpression&) = default;
    AbstractExpression(AbstractExpression&&) noexcept = default;
    AbstractExpression& operator=(const AbstractExpression&) = default;
    AbstractExpression& operator=(AbstractExpression&&) noexcept = default;
};
}
```
- Returning the incomplete `Expression` by value from a member declaration is legal. Any TU that calls `file.aggregate(...)` must include `expression.h`. Tests, the C API and Lua already do.
- `ExpressionAggregate` gets `using Operation = AggregateOperation;`. `ExpressionAggregateAgents` keeps `using Operation = ExpressionAggregate::Operation;` (same type), so every `ExpressionAggregate::Operation::Sum` spelling compiles: 77 hits across include/src/tests [VERIFIED: git grep count]. `expression_node.h` may add `#include "abstract_expression.h"` explicitly. It already gets it through `binary_file.h`.

### Pattern 2: Derived classes (EXPR-02, EXPR-04)

```cpp
// binary_file.h
#include "../expression/abstract_expression.h"
class QUIVER_API BinaryFile : public AbstractExpression {
    ~BinaryFile() override;                                // `override` (tidy modernize-use-override; header is in tidy's filter)
    std::shared_ptr<ExpressionNode> node() const override; // defined in src/expression/expression_file.cpp
    const BinaryMetadata& get_metadata() const override;   // body unchanged (binary_file.cpp:310)
    ...
};
// src/expression/expression_file.cpp
std::shared_ptr<ExpressionNode> BinaryFile::node() const {
    return std::make_shared<ExpressionFile>(get_file_path());   // exactly today's expression.cpp:18
}
// expression.h
class QUIVER_API Expression final : public AbstractExpression {
public:
    explicit Expression(std::shared_ptr<ExpressionNode> node);
    explicit Expression(const AbstractExpression& expression);   // : node_(expression.node())
    std::shared_ptr<ExpressionNode> node() const override;      // out-of-line (anchors Expression's vtable)
private:
    std::shared_ptr<ExpressionNode> node_;
};
```
`Expression(expr)`, where `expr` is an `Expression`, still selects the implicit copy constructor (exact match beats derived-to-base), so its meaning does not change. `BinaryFile`'s defaulted out-of-line moves call the protected base moves, which is accessible.

### Pattern 3: Operators collapse onto helpers (EXPR-03)

```cpp
namespace {
Expression binary(ExpressionBinary::Operation op, const AbstractExpression& lhs, const AbstractExpression& rhs) {
    auto l = lhs.node();          // locals: fixed order, leftmost error first, one .toml parse each
    auto r = rhs.node();
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(l), std::move(r)));
}
Expression binary(ExpressionBinary::Operation op, const AbstractExpression& lhs, double rhs) {
    auto l = lhs.node();
    auto scalar = std::make_shared<ExpressionScalar>(rhs, l->metadata());   // node's metadata, NOT get_metadata()
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(l), std::move(scalar)));
}
// (double, AE) mirror; unary(op, const AE&); ifelse with c/t/e locals
}
Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs) { return binary(ExpressionBinary::Operation::Add, lhs, rhs); }
// ... 35 more one-liners; abs/sqrt/log/exp/-/! -> unary(...)
```
`AbstractExpression::save` is today's body with `node_->` spelled `root->`, and its first line is `const auto root = node();`. The other four methods are one line each over `node()`.

### Anti-Patterns to Avoid
- **Caching a leaf in `BinaryFile`, or giving `node()` a reference to `*this`.** `save()` would then close the caller's handle, and the cache would go stale after a reopen with `'w'`. The design study rejected this (DESIGN-STUDY.md:1041).
- **`get_metadata()` returning `node()->metadata()` for a file.** That is a dangling reference into a temporary leaf.
- **Calling `get_metadata()` in the scalar broadcast.** For an unopened file that is empty metadata. Use `l->metadata()`.
- **A comment containing the word `friend` in `expression.h`, or the string `Expression::metadata` anywhere under `include/` or `src/`.** Either one fails a criterion grep.
- **Adding `ifelse` double overloads** to satisfy one test line (see Open Question 2).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Linux build proof | a new Docker script from scratch | extend `build/fixes-check/linux.sh` (ubuntu:24.04, GCC 13 / Clang 18 + `-stdlib=libc++`, `git archive HEAD`) | Proven in Phases 2-6 |
| Lua output unchanged | new Lua tests | `build/layout-check/golden.sh debug|release` (byte-diff of probe-script JSON through `quiver_cli`) | Phase 6 harness, already baselined |
| f:read/f:write timing | an ad-hoc benchmark | `build/perf/` method from v0.12.9 04-04 (Release `quiver_cli`, one untimed warm-up, five interleaved runs, median = 3rd of sorted five, Git Bash `date +%s%N`) | Same protocol Phase 8 must reproduce |
| Counting `node()` calls | instrumenting the library | a test-local `final` subclass of `AbstractExpression` (protected ctor allows it) wrapping an `Expression` | Probe confirmed it links across the shared-library boundary |

## Runtime State Inventory

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None. `.qvr`/`.toml` formats unchanged; no DB schema touched (verified: no change to binary_metadata/serialization) | none |
| Live service config | None (library; no services) | none |
| OS-registered state | None | none |
| Secrets/env vars | None | none |
| Build artifacts | `libquiver` class layout changes: `BinaryFile` and `Expression` gain a vptr. Stale prebuilt natives (the Dart hook cache, `build/release`, any `build/perf-before` copies) must be rebuilt, not reused. The C ABI is opaque-handle, so binding sources need no change | Rebuild Debug and Release. If `scripts/test-all.bat` is run, clear the Dart hook cache first (v0.12.9 lesson) |

## Common Pitfalls

### Pitfall 1: The temporary leaf in `save` dangles
**What goes wrong:** `node()->collect_input_files(files)` on a `BinaryFile` destroys the leaf at the end of the full-expression, so `files` holds a dangling `BinaryFile*` (undefined behaviour).
**How to avoid:** `const auto root = node();` on the first line of `save`, and use `root` throughout. The test `file.save(out)` with a value check catches it in MSVC Debug (freed memory is filled with 0xDD).

### Pitfall 2: The `get_metadata` design choice
**What goes wrong:** If it is non-virtual on the base, `BinaryFile::get_metadata` hides it, and a call through `const AbstractExpression&` builds a temporary leaf, which re-reads the `.toml` or dangles. If it is pure virtual, it breaks "exactly one pure virtual".
**How to avoid:** Make it a non-pure virtual with the base body `node()->metadata()` (valid while `node()` returns a node the object owns, which `Expression` does), and give `BinaryFile` an override. Test it with **address identity**: `&ae.get_metadata() == &file.get_metadata()` on an *unopened* `BinaryFile f(path_a)`, whose labels are empty while its `.toml` has labels, and `&e.get_metadata() == &e.node()->metadata()`.

### Pitfall 3: Two `node()` calls per scalar operand
**What goes wrong:** Writing `lhs.node()->metadata()` and then `lhs.node()` again parses the `.toml` twice and builds two different leaves.
**How to avoid:** Use one local per operand, as in Pattern 3. Pin it with the counting-subclass test: `x + 2.0`, `2.0 + x`, `abs(x)` should each count 1, and `x + x` and `ifelse(x, x, x)` should count 2 and 3.

### Pitfall 4: Argument evaluation order
**What goes wrong:** `ExpressionBinary(op, lhs.node(), rhs.node())` has unspecified order, so which operand's error is reported varies by compiler.
**How to avoid:** Use locals in declaration order (binary and ifelse helpers).

### Pitfall 5: C++20 rewritten `==`/`!=` with base-class parameters
**What goes wrong:** You might fear an ambiguity or the GCC warning "C++20 says that these are ambiguous, even though the second is reversed".
**Facts:** Every `==` overload has a matching `!=` with the same parameters. Under P2468R2 that means no `==` is a rewrite target. Even without P2468R2, the reversed candidate has identical conversion sequences (derived-to-base on both, or exact on `double`), so the non-rewritten one wins the tie-break. [VERIFIED: probe compiled with no diagnostics on GCC 13.3 and 14.4 and on Clang 18.1.3/libc++, `-Wall -Wextra -Wpedantic`, covering `f == g`, `e == f`, `f != e`, `2.0 == f`, `f != 2.0`, `std::less<>{}(f, g)` and `std::equal_to<>{}(f, g)`.] MSVC compiled the same shape in the design study's real-tree spike (DESIGN-STUDY.md:1045, 1114). Optionally reword the inaccurate comment at `expression.h:118-121` to: "each `==`/`!=` overload has a matching partner, so no C++20 rewritten candidate is formed."

### Pitfall 6: New warnings in headers counted by tidy and compilers
- Clang's `-Winconsistent-missing-override` is on by default. Once `node()` is marked `override`, `get_metadata` must be too. Tidy's `modernize-use-override` also flags `~BinaryFile();` without `override`. `binary_file.h` falls inside the tidy `HeaderFilterRegex` and is included by `src/lua_runner` TUs, so a missed `override` changes the Phase 6 tidy-pair baseline (14).
- GCC 13 enables `-Woverloaded-virtual` in `-Wall` [CITED: GCC 13 build-break reports, LOW confidence on the exact level]. Every derived `get_metadata` must override with the identical signature `const BinaryMetadata& get_metadata() const`.

### Pitfall 7: Lua `f == g` silently changes
**What goes wrong:** If `binary.cpp` stops including `expression.h`, or the operators stop accepting `BinaryFile&`, sol2 drops the automatic `__eq`/`__lt`/`__le`. No test notices (`src/AGENTS.md:667-670`).
**How to avoid:** Keep the include and reword its comment. Verify by hand with Debug and Release `quiver_cli` on a file-backed db with two `.qvr` files: `return { f == g, e == e2, e == f }` must print `[true,true,false]`, the settled behaviour.

### Pitfall 8: Linux harness runs only the Lua suites
`build/fixes-check/linux.sh` greps only `Lua*` and `LuaRunnerCApiTest`. Criterion 5 needs the full `quiver_tests` and `quiver_c_tests` on Linux, so extend it. It also archives `HEAD`, so commit before running it. Expected on Linux: `Lua*` 475 listed (474 pass + 1 root skip; the 2 `_WIN32`-only tests are absent), C API 543. The full `quiver_tests` Linux count has Linux-only skips (Phase 2 saw `1404 + 3 skip / 1407` at that time), so record it rather than assume it.

## Code Examples

### Ownership tests (EXPR-02; criterion 2)
```cpp
TEST_F(ExpressionFixture, FileStaysOpenAndReadableAfterSave) {
    write_qvr(path_a, make_simple_metadata(), [](const std::vector<int64_t>& d, size_t k) { return double(d[0] + d[1] + k); });
    auto a = BinaryFile::open_file(path_a, 'r');
    a.save(path_out);
    EXPECT_TRUE(a.is_open());
    EXPECT_EQ(a.read({{"row", 1}, {"col", 1}}).size(), 2u);
    (a * 2.0).save(path_out2);
    EXPECT_TRUE(a.is_open());
    EXPECT_EQ(a.read({{"row", 1}, {"col", 1}}).size(), 2u);
    // + value checks of path_out == path_a and path_out2 == 2 * path_a via read_all_cells
}

TEST_F(ExpressionFixture, ExpressionOutlivesItsFile) {
    write_qvr(path_a, make_simple_metadata(), ...);
    Expression e = [&] { auto a = BinaryFile::open_file(path_a, 'r'); Expression built(a); a.close(); return built; }();
    e.save(path_out);   // a is closed and destroyed
    // read_all_cells(path_out) == read_all_cells(path_a)
}
```
`row`/`col` are the dimension names of `make_simple_metadata()` [VERIFIED: tests/test_expression.cpp:49-58 `.set("dimensions", {"row", "col"})`, `.set("dimension_sizes", {3, 2})`, `.set("labels", {"val1", "val2"})`]. Non-time dimension values are 1-based [VERIFIED: src/binary/iteration.cpp:120 `result.push_back(dim.is_time_dimension() ? dim.time->initial_value : 1);`].

### Shape test (criterion 1), all compile-time
```cpp
TEST_F(ExpressionFixture, AbstractExpressionShape) {
    static_assert(std::is_abstract_v<AbstractExpression>);
    static_assert(std::has_virtual_destructor_v<AbstractExpression>);
    static_assert(!std::is_copy_assignable_v<AbstractExpression> && !std::is_move_assignable_v<AbstractExpression>);
    static_assert(std::is_base_of_v<AbstractExpression, BinaryFile> && std::is_base_of_v<AbstractExpression, Expression>);
    static_assert(std::is_final_v<Expression>);
    static_assert(std::is_constructible_v<Expression, const BinaryFile&>);
    static_assert(!std::is_convertible_v<const BinaryFile&, Expression>);
    static_assert(std::is_same_v<ExpressionAggregate::Operation, AggregateOperation>);
    static_assert(std::is_same_v<ExpressionAggregateAgents::Operation, AggregateOperation>);
}
```
All of these traits were checked on GCC 13/14 and Clang 18 in the probe. Add `#include <type_traits>` to the test file.

### No-wrapper operands (criterion 3; also exercises the Linux C++20 rewrite)
`a + b`, `2.0 + a`, `ifelse(a > 1.0, a, b)` (see Open Question 2), `a == b`, `a != Expression(b)`, `2.0 == a`, `-a`, `!a`, `abs(a)`, `a && b`, and `a.aggregate("col", AggregateOperation::Sum)`, `a.select_agents({"val1"})`, `a.rename_agents({{"val1","x"}})` on a raw file. Save a few of them and check the values.

### CHANGELOG lines (under `## [0.13.0] — unreleased` / `### Changed`)
```markdown
- **BREAKING** **C++: `Expression` is no longer implicitly constructed from a `BinaryFile`.** A
  `BinaryFile` is now an expression itself (both derive from `quiver::AbstractExpression`), so every
  operator, `abs`/`sqrt`/`log`/`exp`, `ifelse`, `save`, `aggregate`, `aggregate_agents`,
  `select_agents` and `rename_agents` take a file directly: `file_a + file_b`, `2.0 * file` and
  `file.save(path)` need no wrapper. `Expression e = file;` no longer compiles; write `Expression e(file);`.
- **BREAKING** **C++: `Expression::metadata()` is renamed `get_metadata()`**, the name `BinaryFile`
  and the C API already use. Call `get_metadata()`.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Implicit `Expression(const BinaryFile&)` + friend operators on `Expression` | `AbstractExpression` base, operators on the base, `explicit` freeze ctor | this phase | A file is an operand without a wrapper; copy-init from a file breaks |
| `ExpressionAggregate::Operation` nested enum | `quiver::AggregateOperation` + alias | this phase | Source-compatible |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `get_metadata` as a non-pure virtual (base body serves `Expression`) is what "exactly one pure virtual" intends | Open Question 1 | Low: if the user wants it pure, it is a one-token change plus an `Expression` override |
| A2 | GCC 13 enables `-Woverloaded-virtual` at `-Wall` | Pitfall 6 | None: the probe showed no warning either way |
| A3 | MSVC compiles the base-parameter `==`/`!=` without C2666 in `/std:c++20 /permissive-` | Pitfall 5 | Only the design study's spike evidence; the Windows build is the first local check |

## Open Questions (RESOLVED)

1. **Is `get_metadata()` virtual?** (EXPR-05 versus criterion 1, "exactly one pure virtual")
   - What we know: through a `const AbstractExpression&` it must return the file handle's in-memory copy, so it must be virtual. The design study made it pure (DESIGN-STUDY.md:1152), but the locked requirement says "exactly one pure virtual `node()`". The judges' merge note says "virtual, with the file override returning the in-memory copy" (DESIGN-STUDY.md:2066).
   - RESOLVED: non-pure `virtual const BinaryMetadata& get_metadata() const;` with the base body `return node()->metadata();`, overridden only by `BinaryFile`. This meets every locked wording: one pure virtual, an `Expression` returns its node's metadata, and a file returns the handle's. Document the base body's precondition in a one-line comment.

2. **`ifelse(file > 1.0, file, 0.0)` (criterion 3) does not compile.**
   - What we know: `ifelse` has only `(const Expression&, const Expression&, const Expression&)` today [VERIFIED: expression.h:116]. There is no `double` overload in C++, the C API (`quiver_expression_apply_ternary` takes three expressions), Lua (`to_expression` on all three) or Julia (expression.jl:163-176). EXPR-03 says the `double` overloads "stay", not that new ones are added.
   - RESOLVED: do not add an overload (it would need 7 combinations and has no binding counterpart). Write the test as `ifelse(a > 1.0, a, b)` with two raw files, or `ifelse(a > 1.0, a, a * 0.0)`, and note the deviation from the criterion's literal text in the plan and the summary so the verifier does not flag it.

3. **Phase 8's baselines must be produced in Phase 7.**
   - What we know: Phase 8 criterion 4 needs (a) a Release benchmark of 1M `f:read` and 1M `f:write` "taken at the end of Phase 7", and (b) "no warning that the Phase 7 Release log did not have" for `src/lua_runner/`.
   - RESOLVED: Phase 7's last plan does two things after the final code:
     - Save the Release build log of the `quiver` target to a gitignored harness file (e.g. `build/abstract-check/release-build.log`). Phase 7 changes headers every `src/lua_runner` TU includes, so they all recompile. Record its `src/lua_runner` warning count (`C4702` is expected to be 0) in the SUMMARY.
     - Run 1M-call workloads with Release `quiver_cli` (for example a 1000 x 1000 x 1-label file: `f:write({1.0}, {row=r,col=c})` and `#f:read({row=r,col=c})`), using the v0.12.9 protocol. Record the medians in the SUMMARY and in STATE.md's PR notes.

4. **How much AGENTS.md to touch now.**
   - What we know: the roadmap gives DOC-01 (Design Decision, cross-layer rows, the "caches an open BinaryFile" line) to Phase 9. The root "Self-Updating" rule says to keep the nearest AGENTS.md accurate.
   - RESOLVED: in Phase 7, edit only the `src/AGENTS.md` lines this phase makes false:
     - 1024: the implicit constructor bullet becomes `explicit Expression(const AbstractExpression&)`.
     - 1025: `metadata()` becomes `get_metadata()`.
     - 1027, 1042, 1047: the enum is `AggregateOperation` at namespace scope.
     - 31: add `abstract_expression.h` to the file map.
     - 668-669: the include rationale.
     - The comparison note in the 1030s: the "compiler does not synthesize" wording.
     Leave the Design Decision, the cross-layer rows and the "Caches an open BinaryFile" line to Phase 9, so its grep criterion stays meaningful.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSVC + Ninja + CMake | Windows Debug/Release | yes | cmake 4.3.1-msvc1, ninja 1.13.2 | — |
| Configured build dirs | Debug `build/`, Release `build/release` | yes | both `CMakeCache.txt` present | — |
| Docker daemon | Linux GCC 13 / Clang 18 builds | yes | 29.6.2; images `ubuntu:24.04`, `gcc:14` cached | none (blocking for EXPR-07) |
| uv | tidy runner (`run-clang-tidy` via uv) | yes | 0.12.3 | — |
| Release `quiver_cli` | benchmark + Lua equality check | yes | `build/release/bin/quiver_cli.exe` | — |
| bun | lua-api sync test (gate.sh) | yes (used in Phase 6) | — | — |

No dependency is missing.

## Validation Architecture

Skipped: `workflow.nyquist_validation` is `false` in `.planning/config.json`. The phase gate, for the planner's verification steps, is:
- Windows Debug and Release: `quiver_tests` = 1454 + N (record N) all pass; `ExpressionFixture` = 116 + N; `quiver_c_tests` 543 and `ExpressionCApiFixture` 73 pass; `Lua*` 477 in 12 suites; `LuaRunnerCApiTest` 27.
- `git diff <base> -- 'tests/test_lua*.cpp' 'tests/test_c_api*.cpp'` is empty, and `golden.sh debug` and `golden.sh release` are byte-identical.
- Grep gates:
  - `grep -n friend include/quiver/expression/expression.h` prints nothing.
  - `git grep -n 'Expression::metadata' -- include src` prints nothing.
  - `git diff <base> -- include/quiver/c bindings/julia/src/c_api.jl` is empty.
  - `git diff <base> --stat -- src/c` shows one line in `src/c/expression/expression.cpp`.
- The extended `linux.sh` passes for gcc and clang (Pitfall 8). The `quiver_cli` equality check prints `[true,true,false]`. The two baselines in Open Question 3 are recorded.

## Security Domain

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication / V3 Session / V4 Access Control | no | — |
| V5 Input Validation | no new input path | existing Lua sandbox (`resolve_sandboxed_path`) untouched |
| V6 Cryptography | no | — |

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Use-after-free via dangling leaf in `save` (Pitfall 1) | Tampering / DoS | `const auto root = node();` + test |
| Expression closing or writing through the caller's handle | Tampering | path-based leaf only; tests that the file stays open and the expression outlives the file |
| Lua sandbox bypass via file-as-expression `save` | Elevation | Not reachable this phase: Lua still has no `f:save`. Phase 8 keeps the three guards |

## Sources

### Primary (HIGH confidence)
- Repository (read this session): `include/quiver/expression/{expression.h,expression_node.h}`, `include/quiver/binary/{binary_file.h,binary_metadata.h}`, `include/quiver/export.h`, `src/expression/{expression.cpp,expression_file.cpp}`, `src/binary/binary_file.cpp`, `src/c/expression/expression.cpp`, `src/c/internal.h`, `src/lua_runner/{expression.cpp,binary.cpp}`, `cmake/{Platform,CompilerOptions}.cmake`, `src/CMakeLists.txt`, `.clang-tidy`, `tests/test_expression.cpp`, `CHANGELOG.md`, `src/AGENTS.md`, `tests/AGENTS.md`
- sol2 v3.5.0 source: `build/_deps/sol2-src/include/sol/usertype_core.hpp:125-180`, `stack_core.hpp:1387-1418`
- `.planning/research/DESIGN-STUDY.md` (designs, judges, spike) and `SUMMARY.md`
- Local probes (scratchpad `probe/`): single-TU shape probe on GCC 14.4 (`gcc:14`), GCC 13.3 and Clang 18.1.3/libc++ (`ubuntu:24.04`); shared-library probe with `-fvisibility=hidden -fvisibility-inlines-hidden` on GCC 13.3 and Clang 18.1.3/libc++. In both cases the exe links and runs, and it covers dispatch through the base, an exe-side subclass, and `dynamic_cast` across the boundary.

### Secondary (MEDIUM/LOW confidence)
- [P2468R2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2468r2.html), [Clang C++ status](https://clang.llvm.org/cxx_status.html), [D134529](https://reviews.llvm.org/D134529): rewrite-target rule and compiler support
- GCC 13 `-Woverloaded-virtual` in `-Wall`: [intel/media-driver#1639](https://github.com/intel/media-driver/issues/1639), [gem5 PR](https://github.com/lhartung/gem5-ci-work/pull/67) (LOW)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH. No new dependencies.
- Architecture: HIGH. Every call site was grepped, and the shape compiled on three Linux toolchains plus the design study's MSVC spike.
- Pitfalls: HIGH. Each one is tied to a line in the tree or to a probe result.

**Research date:** 2026-10-04
**Valid until:** 2026-11-04 (stable; in-tree only)
