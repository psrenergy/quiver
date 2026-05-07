#ifndef QUIVER_EXPRESSION_NODE_H
#define QUIVER_EXPRESSION_NODE_H

#include "../binary/binary_metadata.h"
#include "../export.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

class BinaryFile;

class QUIVER_API Node {
public:
    virtual ~Node() = default;

    virtual const BinaryMetadata& metadata() const = 0;

    virtual void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const = 0;
};

class QUIVER_API FileNode final : public Node {
public:
    explicit FileNode(const BinaryFile& file);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;

    const std::string& path() const { return path_; }

private:
    std::string path_;
    BinaryMetadata meta_;

    mutable std::unique_ptr<BinaryFile> file_;
    mutable std::unordered_map<std::string, int64_t> dim_map_;
};

class QUIVER_API ScalarNode final : public Node {
public:
    ScalarNode(double value, BinaryMetadata broadcast_meta);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;

private:
    double value_;
    BinaryMetadata broadcast_meta_;
};

enum class BinaryOp { Add, Subtract, Multiply, Divide };

class QUIVER_API BinaryOpNode final : public Node {
public:
    BinaryOpNode(BinaryOp op, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;

    const std::shared_ptr<Node>& lhs() const { return lhs_; }
    const std::shared_ptr<Node>& rhs() const { return rhs_; }

private:
    BinaryOp op_;
    std::shared_ptr<Node> lhs_;
    std::shared_ptr<Node> rhs_;
    BinaryMetadata broadcast_meta_;

    std::vector<int64_t> lhs_dim_sizes_;
    std::vector<int64_t> rhs_dim_sizes_;

    std::vector<int> lhs_to_out_;
    std::vector<int> rhs_to_out_;

    size_t lhs_label_count_ = 0;
    size_t rhs_label_count_ = 0;

    mutable std::vector<int64_t> lhs_dims_buf_;
    mutable std::vector<int64_t> rhs_dims_buf_;
    mutable std::vector<double> lhs_buf_;
    mutable std::vector<double> rhs_buf_;
};

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_NODE_H
