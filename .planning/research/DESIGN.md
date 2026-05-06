# Design — Lazy Expressions Subsystem

**Status:** Architectural sketch, decided 2026-05-05. Open questions land in discuss-phase.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Subsystem name | `expressions` (`quiver::expr`, `quiver_expression_*`) | Standard math/compiler vocabulary, clean across all 5 bindings, no collision with Database verbs (create/read/update/delete) |
| Architecture | Runtime AST with virtual `Node` base | FFI requires opaque handles for 5 bindings; CRTP types are not ABI-stable. Hot path is I/O, not arithmetic — virtual dispatch is invisible against per-row file reads. |

Rejected: CRTP / expression templates (Eigen/Blaze style). Optimizes a bottleneck Quiver doesn't have, hostile to FFI, header bloat, slow compilation, template error storms on every new op.

## Public C++ API (sketch)

```cpp
// include/quiver/expr/expression.h
namespace quiver::expr {

class Node;  // opaque

class Expression {
public:
    Expression(const BinaryFile& file);                // implicit leaf
    explicit Expression(std::shared_ptr<Node> node);

    BinaryMetadata metadata() const;
    void save(const std::string& path) const;          // materializes row-by-row

    const std::shared_ptr<Node>& node() const;
};

Expression operator+(const Expression&, const Expression&);
Expression operator-(const Expression&, const Expression&);
Expression operator*(const Expression&, const Expression&);
Expression operator/(const Expression&, const Expression&);

// Scalar broadcast (sibling provides metadata for shape):
Expression operator+(const Expression&, double);
Expression operator+(double, const Expression&);
// (same for -, *, /)

}  // namespace quiver::expr
```

## Internal AST

```cpp
// include/quiver/expr/node.h
namespace quiver::expr {

class Node {
public:
    virtual ~Node() = default;
    virtual BinaryMetadata metadata() const = 0;
    virtual void compute_row(const std::vector<int64_t>& dims,
                             std::vector<double>& out) const = 0;
};

class FileNode final : public Node {
    std::string path_;
    BinaryMetadata meta_;
    mutable std::unique_ptr<BinaryFile> file_;  // lazily opened
public:
    FileNode(std::string path, BinaryMetadata meta);
    BinaryMetadata metadata() const override { return meta_; }
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
};

class ScalarNode final : public Node {
    double value_;
    BinaryMetadata broadcast_;
public:
    ScalarNode(double value, BinaryMetadata broadcast);
    BinaryMetadata metadata() const override { return broadcast_; }
    void compute_row(const std::vector<int64_t>&, std::vector<double>& out) const override;
};

enum class BinaryOp { Add, Sub, Mul, Div };

class BinaryOpNode final : public Node {
    BinaryOp op_;
    std::shared_ptr<Node> lhs_;
    std::shared_ptr<Node> rhs_;
public:
    BinaryOpNode(BinaryOp op, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
    BinaryMetadata metadata() const override { return lhs_->metadata(); }
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
};

}  // namespace quiver::expr
```

## Engine — `save()` row-by-row

```cpp
// src/expr/expression.cpp (engine + operators)
void Expression::save(const std::string& path) const {
    const auto meta = node_->metadata();
    auto writer = BinaryFile::open_file(path, 'w', meta);

    std::vector<int64_t> dims = first_dimensions(meta);
    std::vector<double> row;
    do {
        node_->compute_row(dims, row);
        writer.write(dims, row);
    } while (next_dimensions(dims, meta));
}
```

The engine is dumb-on-purpose: walk every row index combination, ask the root node for the row, write it. Per-row temporaries reused across iterations (single allocation amortization). All op fusion happens *inside* `BinaryOpNode::compute_row` for that single row's `vector<double>`.

## C API surface

```c
// include/quiver/c/expr/expression.h
typedef struct quiver_expression quiver_expression_t;

QUIVER_C_API quiver_error_t quiver_expression_from_file(quiver_binary_file_t* file,
                                                        quiver_expression_t** out);

QUIVER_C_API quiver_error_t quiver_expression_add(quiver_expression_t* a,
                                                  quiver_expression_t* b,
                                                  quiver_expression_t** out);
QUIVER_C_API quiver_error_t quiver_expression_sub(...);
QUIVER_C_API quiver_error_t quiver_expression_mul(...);
QUIVER_C_API quiver_error_t quiver_expression_div(...);

QUIVER_C_API quiver_error_t quiver_expression_add_scalar(quiver_expression_t* a,
                                                         double s,
                                                         quiver_expression_t** out);
// (same for sub/mul/div, plus scalar-on-left variants)

QUIVER_C_API quiver_error_t quiver_expression_save(quiver_expression_t* expr,
                                                   const char* path);

QUIVER_C_API void quiver_expression_free(quiver_expression_t* expr);
```

Each binding wraps these to expose native operators.

## Binding sketches

### Julia
```julia
struct Expression
    handle::Ptr{Cvoid}
end
Base.:+(a::Expression, b::Expression) = Expression(@ccall quiver_expression_add(a.handle, b.handle, ...)::...)
Base.:+(a::BinaryFile, b::BinaryFile) = expression(a) + expression(b)
save(e::Expression, path::AbstractString) = @ccall quiver_expression_save(e.handle, path::Cstring)::Cint
```

### Python
```python
class Expression:
    def __init__(self, handle): self._h = handle
    def __add__(self, other):
        if isinstance(other, (int, float)):
            return Expression(_ffi.expression_add_scalar(self._h, float(other)))
        return Expression(_ffi.expression_add(self._h, other._h))
    def save(self, path): _ffi.expression_save(self._h, path)
```

Equivalents in Dart, JS, Lua follow the same pattern (mirror the C API, native operators delegate).

## Build wiring

- `cmake/Dependencies.cmake` — no new deps; uses existing BinaryFile.
- `src/CMakeLists.txt` — add `src/expr/expression.cpp` + `src/expr/nodes.cpp` to `quiver` target sources.
- `src/c/CMakeLists.txt` — add `src/c/expr/expression.cpp` to `quiver_c` target sources.
- `tests/CMakeLists.txt` — add `tests/test_expression.cpp` to `quiver_tests`; add `tests/test_c_api_expression.cpp` to `quiver_c_tests`.

## Test plan

- `test_expression.cpp` — C++ core:
  - `AddTwoFilesAndScale`: `(a + b) * 2.0` → expected values
  - `ScalarBroadcast`: `a + 1.0`, `1.0 + a`, all four ops
  - `Chained`: `((a + b) - c) / 2.0`
  - `IdentityFile`: single file, no ops, save copies file
  - `MismatchedShapes`: throws clean error

- `test_c_api_expression.cpp` — FFI symmetry, free-after-save, error paths.

- Per-binding tests: one round-trip test mirroring the C++ AddTwoFilesAndScale case.

## Open questions for discuss-phase

1. **Shape compatibility.** When `a.metadata() != b.metadata()`, do we error (strict) or broadcast (loose)? Strict is simpler.
2. **Output metadata inheritance.** `(a + b).metadata()` returns `a`'s. Is that always right when `b` has different labels? Probably need a compatibility check at `BinaryOpNode` construction.
3. **Op set.** v1 = `+ - * /`. Do we want `unary minus`, `pow`, `abs`, `min(a, b)`, `where(cond, a, b)`?
4. **Reduction ops** (sum across a dim, mean) — out of scope for v1, since output shape differs.
5. **Concurrent reads.** Multiple `FileNode`s open the same file twice in a chain like `a + a`. Cache by path? Or accept duplicate handles?
6. **`save()` while source file is registered as open writer** — already blocked by binary write registry. Document.
7. **Error message conventions.** Reuse the three patterns from CLAUDE.md (Cannot…, Not found…, Failed to…).

## Cross-binding naming (per CLAUDE.md transformation rules)

| C++                       | C API                            | Julia        | Dart         | Python      | Lua         |
|---------------------------|----------------------------------|--------------|--------------|-------------|-------------|
| `Expression::save`        | `quiver_expression_save`         | `save`       | `save`       | `save`      | `save`      |
| `operator+`               | `quiver_expression_add`          | `Base.:+`    | `operator +` | `__add__`   | `__add`     |
| `Expression(BinaryFile&)` | `quiver_expression_from_file`    | `Expression` | `Expression` | `Expression`| `expression`|

## Principles enforced

- **Clean code:** no defensive null checks inside nodes (callers obey contracts; null reaches the C boundary, not the C++ core).
- **Pimpl only when hiding deps:** `Expression` is a value type wrapping `shared_ptr<Node>`. No Pimpl — there's nothing private to hide.
- **Ownership:** `shared_ptr<Node>` for tree sharing; `unique_ptr<BinaryFile>` inside `FileNode` for exclusive read handle.
- **RAII:** file handles close on destruction; no manual cleanup paths.
- **Errors:** Use the three CLAUDE.md patterns. `BinaryFile::open_file` already throws — bubble up.

---

*Design captured 2026-05-05. Code lives in `src/expr/` and `include/quiver/expr/` once the implementation phase begins.*
