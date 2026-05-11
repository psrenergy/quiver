#ifndef QUIVER_EXPRESSION_NODE_H
#define QUIVER_EXPRESSION_NODE_H

#include "../binary/binary_metadata.h"
#include "../export.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

class BinaryFile;

class QUIVER_API ExpressionNode {
public:
    virtual ~ExpressionNode() = default;

    virtual const BinaryMetadata& metadata() const = 0;

    virtual void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const = 0;

    virtual void collect_input_paths(std::vector<std::string>& out) const = 0;
};

class QUIVER_API ExpressionFile final : public ExpressionNode {
public:
    explicit ExpressionFile(const BinaryFile& file);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_paths(std::vector<std::string>& out) const override;

    const std::string& path() const { return path_; }

private:
    std::string path_;
    BinaryMetadata meta_;

    mutable std::unique_ptr<BinaryFile> file_;
    mutable std::unordered_map<std::string, int64_t> dim_map_;
};

class QUIVER_API ExpressionScalar final : public ExpressionNode {
public:
    ExpressionScalar(double value, BinaryMetadata broadcast_meta);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_paths(std::vector<std::string>& out) const override;

private:
    double value_;
    BinaryMetadata broadcast_meta_;
};

class QUIVER_API ExpressionBinary final : public ExpressionNode {
public:
    enum class Operation { Add, Subtract, Multiply, Divide };

    ExpressionBinary(Operation operation, std::shared_ptr<ExpressionNode> lhs, std::shared_ptr<ExpressionNode> rhs);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_paths(std::vector<std::string>& out) const override;

private:
    static double apply(Operation operation, double lhs, double rhs);

    Operation operation_;
    std::shared_ptr<ExpressionNode> lhs_;
    std::shared_ptr<ExpressionNode> rhs_;
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

class QUIVER_API ExpressionUnary final : public ExpressionNode {
public:
    enum class Operation { Unspecified };

    ExpressionUnary(Operation operation, std::shared_ptr<ExpressionNode> operand);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_paths(std::vector<std::string>& out) const override;

private:
    Operation operation_;
    std::shared_ptr<ExpressionNode> operand_;
};

class QUIVER_API ExpressionTernary final : public ExpressionNode {
public:
    enum class Operation { Unspecified };

    ExpressionTernary(Operation operation,
                      std::shared_ptr<ExpressionNode> first,
                      std::shared_ptr<ExpressionNode> second,
                      std::shared_ptr<ExpressionNode> third);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_paths(std::vector<std::string>& out) const override;

private:
    Operation operation_;
    std::shared_ptr<ExpressionNode> first_;
    std::shared_ptr<ExpressionNode> second_;
    std::shared_ptr<ExpressionNode> third_;
};

class QUIVER_API ExpressionAggregate final : public ExpressionNode {
public:
    enum class Operation { Sum, Mean, Min, Max, Percentile };

    static Operation parse_operation(const std::string& name);

    ExpressionAggregate(Operation operation,
                        std::shared_ptr<ExpressionNode> operand,
                        std::string dimension_name,
                        std::optional<double> parameter = std::nullopt);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_paths(std::vector<std::string>& out) const override;

private:
    Operation operation_;
    std::shared_ptr<ExpressionNode> operand_;
    std::string dimension_name_;
    std::optional<double> parameter_;

    BinaryMetadata output_meta_;
    int reduced_operand_index_ = -1;
    std::vector<int> operand_to_out_;
    mutable std::vector<int64_t> operand_dims_buf_;
    mutable std::vector<double> operand_row_buf_;
    mutable std::vector<std::vector<double>> percentile_scratch_;
};

class QUIVER_API ExpressionAggregateAgents final : public ExpressionNode {
public:
    enum class Operation { Sum, Mean, Min, Max, Percentile };

    static Operation parse_operation(const std::string& name);

    ExpressionAggregateAgents(Operation operation,
                              std::shared_ptr<ExpressionNode> operand,
                              std::optional<double> parameter = std::nullopt);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_paths(std::vector<std::string>& out) const override;

private:
    Operation operation_;
    std::shared_ptr<ExpressionNode> operand_;
    std::optional<double> parameter_;

    BinaryMetadata output_meta_;
    mutable std::vector<double> operand_row_buf_;
};

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_NODE_H
