#ifndef QUIVER_EXPR_EXPRESSION_H
#define QUIVER_EXPR_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"
#include "node.h"

#include <memory>
#include <string>

namespace quiver::expr {

// Public value type for the lazy expression subsystem. Wraps shared_ptr<Node>
// (no Pimpl per CLAUDE.md "Pimpl rule": no private dependencies to hide).
// Rule of Zero: copy/move/dtor are compiler-generated.
class QUIVER_API Expression {
public:
    // CORE-01: implicit conversion from BinaryFile so users can write
    //   Expression e = a;
    // where `a` is a `BinaryFile`. Constructs a FileNode capturing a's path
    // and metadata snapshot (D-10).
    Expression(const BinaryFile& file);  // NOT explicit by design — see CORE-01

    // Wrap an existing Node directly. Used internally by operator overloads.
    explicit Expression(std::shared_ptr<Node> node);

    // Returns the output metadata of this expression's tree.
    BinaryMetadata metadata() const;

    // CORE-05: materialize the lazy tree to a new .qvr file at `path`.
    // Performs the D-11 self-save check at entry (throws if any FileNode in the
    // tree references the same canonicalized path as `path`, BEFORE opening writer).
    // Iterates row-by-row reusing a single std::vector<double> buffer (CORE-06).
    void save(const std::string& path) const;

    // Accessor for the root node — used by operator overloads when constructing
    // BinaryOpNode children. Bindings (Phase 2+) do not need this.
    const std::shared_ptr<Node>& node() const;

private:
    std::shared_ptr<Node> node_;
};

// ============================================================================
// Free-function binary operators (12 overloads: 3 per op × 4 ops).
// Each operator constructs a BinaryOpNode (which validates eagerly per D-01..D-09).
// The (Expression, double) and (double, Expression) variants construct a
// ScalarNode using the sibling's metadata for broadcast shape (D-08 / CORE-03).
// ============================================================================

QUIVER_API Expression operator+(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator+(const Expression& lhs, double rhs);
QUIVER_API Expression operator+(double lhs, const Expression& rhs);

QUIVER_API Expression operator-(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator-(const Expression& lhs, double rhs);
QUIVER_API Expression operator-(double lhs, const Expression& rhs);

QUIVER_API Expression operator*(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator*(const Expression& lhs, double rhs);
QUIVER_API Expression operator*(double lhs, const Expression& rhs);

QUIVER_API Expression operator/(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator/(const Expression& lhs, double rhs);
QUIVER_API Expression operator/(double lhs, const Expression& rhs);

}  // namespace quiver::expr

#endif  // QUIVER_EXPR_EXPRESSION_H
