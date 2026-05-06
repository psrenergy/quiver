#ifndef QUIVER_EXPR_NODE_H
#define QUIVER_EXPR_NODE_H

#include "../binary/binary_metadata.h"
#include "../export.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace quiver {
class BinaryFile;  // forward-declared; FileNode ctor uses BinaryFile& but the body lives in nodes.cpp
}  // namespace quiver

namespace quiver::expr {

// Virtual base for the expression AST. Logically immutable: same input dims
// produce the same output. Concrete subclasses may use `mutable` members to
// cache reusable buffers without violating logical immutability.
class QUIVER_API Node {
public:
    virtual ~Node() = default;

    // Returns the metadata of this subtree's output. For leaf nodes (FileNode,
    // ScalarNode), returns the captured / broadcast metadata. For BinaryOpNode,
    // returns the eagerly-validated, broadcast-merged metadata of lhs and rhs.
    virtual BinaryMetadata metadata() const = 0;

    // Computes one output row for the position described by `dims`.
    // `dims` is in the OUTPUT metadata's dimension declaration order.
    // `out` is resized to metadata().labels.size() if needed; values are written in place.
    // Each concrete Node is responsible for translating `dims` (output-order) into its
    // own operand-order coords (D-05).
    virtual void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const = 0;
};

// Leaf node wrapping a .qvr file. Captures path + metadata snapshot at construction
// (via BinaryFile::get_file_path and get_metadata). Lazily opens the file on first
// compute_row call (D-10). Each FileNode owns its own BinaryFile handle; multiple
// FileNodes referring to the same path open the file independently — no caching (D-10).
class QUIVER_API FileNode final : public Node {
public:
    explicit FileNode(const BinaryFile& file);

    BinaryMetadata metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;

private:
    std::string path_;
    BinaryMetadata meta_;

    // mutable: lazy-open and reuse buffer. Logically immutable from caller's perspective —
    // the same `dims` always produces the same `out`. NOT thread-safe (project policy).
    mutable std::unique_ptr<BinaryFile> file_;
    mutable std::vector<int64_t> own_dims_buf_;
};

// Leaf node holding a scalar literal that broadcasts against a sibling. The
// sibling's metadata (passed by the operator overload) becomes this node's reported
// metadata so downstream BinaryOpNode validation sees a compatible shape.
class QUIVER_API ScalarNode final : public Node {
public:
    ScalarNode(double value, BinaryMetadata broadcast_meta);

    BinaryMetadata metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;

private:
    double value_;
    BinaryMetadata broadcast_meta_;
};

// The four binary operators supported in v1.
enum class BinaryOp { Add, Subtract, Multiply, Divide };

// Internal node combining two operands with a binary op. Performs eager validation
// at construction (D-03):
//   - D-07: units must match exactly
//   - D-01 + D-06: shape rule (n×n / 1×n / n×1) per same-name dim; throws on n×m mismatch
//   - D-04: same-name time dims → TimeProperties must match exactly
//   - D-09: when both sides have any time dim → initial_datetime must match
//   - D-08: labels axis broadcasts under the same n×n / 1×n / n×1 rule
// Reports broadcast-merged metadata via metadata() (D-02 dim order: lhs ++ rhs-only).
// compute_row translates output coords to per-operand coords (D-05) and applies the
// op element-wise across the labels axis (D-08).
class QUIVER_API BinaryOpNode final : public Node {
public:
    BinaryOpNode(BinaryOp op, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

    BinaryMetadata metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;

private:
    BinaryOp op_;
    std::shared_ptr<Node> lhs_;
    std::shared_ptr<Node> rhs_;
    BinaryMetadata broadcast_meta_;

    // For each output-dim index: the corresponding index in lhs/rhs metadata.dimensions,
    // or -1 if absent. Built once at construction; used by compute_row (D-05 translation).
    std::vector<int> lhs_dim_translate_;
    std::vector<int> rhs_dim_translate_;

    // For each output-dim index: lhs/rhs's size for that dim, or 0 if absent. Used to
    // detect size-1 broadcast at compute_row time (clamp output coord to 1 on size-1 dims).
    std::vector<int64_t> lhs_dim_sizes_;
    std::vector<int64_t> rhs_dim_sizes_;

    // Label-axis broadcast (D-08).
    size_t lhs_label_count_ = 0;
    size_t rhs_label_count_ = 0;

    // Reused buffers — mutable per the Node logical-immutability contract. Single-threaded use only.
    mutable std::vector<int64_t> lhs_dims_buf_;
    mutable std::vector<int64_t> rhs_dims_buf_;
    mutable std::vector<double> lhs_buf_;
    mutable std::vector<double> rhs_buf_;
};

}  // namespace quiver::expr

#endif  // QUIVER_EXPR_NODE_H
