#ifndef QUIVER_EXPRESSION_NODE_H
#define QUIVER_EXPRESSION_NODE_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace quiver {

class QUIVER_API ExpressionNode {
public:
    virtual ~ExpressionNode() = default;

    virtual const BinaryMetadata& metadata() const = 0;

    virtual void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const = 0;

    virtual void collect_input_files(std::vector<BinaryFile*>& out) const = 0;
};

class QUIVER_API ExpressionFile final : public ExpressionNode {
public:
    explicit ExpressionFile(const std::string& path);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

    const std::string& path() const { return file_.get_file_path(); }

private:
    BinaryMetadata meta_;
    mutable BinaryFile file_;
    mutable std::unordered_map<std::string, int64_t> dim_map_;
};

class QUIVER_API ExpressionScalar final : public ExpressionNode {
public:
    ExpressionScalar(double value, BinaryMetadata broadcast_meta);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

private:
    double value_;
    BinaryMetadata broadcast_meta_;
};

// Per-operand broadcast machinery shared by ExpressionBinary and ExpressionTernary:
// translation tables from the broadcast output space into the operand's own
// dimension space, plus reusable per-row buffers.
struct BroadcastOperand {
    std::vector<int64_t> dim_sizes;  // indexed by output dimension; 0 when absent from this operand
    std::vector<int> to_out;         // operand dimension index -> output dimension index
    size_t label_count = 0;
    mutable std::vector<int64_t> dims_buf;
    mutable std::vector<double> row_buf;
};

class QUIVER_API ExpressionBinary final : public ExpressionNode {
public:
    enum class Operation { Add, Subtract, Multiply, Divide, Gt, Lt, Gte, Lte, Eq, Neq };

    ExpressionBinary(Operation operation, std::shared_ptr<ExpressionNode> lhs, std::shared_ptr<ExpressionNode> rhs);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

private:
    static double apply(Operation operation, double lhs, double rhs);

    Operation operation_;
    std::shared_ptr<ExpressionNode> lhs_;
    std::shared_ptr<ExpressionNode> rhs_;
    BinaryMetadata broadcast_meta_;
    BroadcastOperand lhs_op_;
    BroadcastOperand rhs_op_;
};

class QUIVER_API ExpressionUnary final : public ExpressionNode {
public:
    enum class Operation { Negate, Abs, Sqrt, Log, Exp };

    ExpressionUnary(Operation operation, std::shared_ptr<ExpressionNode> operand);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

private:
    static double apply(Operation operation, double x);

    Operation operation_;
    std::shared_ptr<ExpressionNode> operand_;
    mutable std::vector<double> operand_row_buf_;
};

class QUIVER_API ExpressionTernary final : public ExpressionNode {
public:
    enum class Operation { IfElse };

    ExpressionTernary(Operation operation,
                      std::shared_ptr<ExpressionNode> condition,
                      std::shared_ptr<ExpressionNode> then_value,
                      std::shared_ptr<ExpressionNode> else_value);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

private:
    static double apply(Operation operation, double condition, double then_value, double else_value);

    Operation operation_;
    std::shared_ptr<ExpressionNode> condition_;
    std::shared_ptr<ExpressionNode> then_value_;
    std::shared_ptr<ExpressionNode> else_value_;
    BinaryMetadata broadcast_meta_;
    BroadcastOperand condition_op_;
    BroadcastOperand then_op_;
    BroadcastOperand else_op_;
};

class QUIVER_API ExpressionAggregate final : public ExpressionNode {
public:
    enum class Operation { Sum, Mean, Min, Max, Percentile };

    ExpressionAggregate(Operation operation,
                        std::shared_ptr<ExpressionNode> operand,
                        std::string dimension_name,
                        std::optional<double> parameter = std::nullopt);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

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

    ExpressionAggregateAgents(Operation operation,
                              std::shared_ptr<ExpressionNode> operand,
                              std::optional<double> parameter = std::nullopt);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

private:
    Operation operation_;
    std::shared_ptr<ExpressionNode> operand_;
    std::optional<double> parameter_;

    BinaryMetadata output_meta_;
    mutable std::vector<double> operand_row_buf_;
};

class QUIVER_API ExpressionSelectAgents final : public ExpressionNode {
public:
    ExpressionSelectAgents(std::shared_ptr<ExpressionNode> operand, std::vector<std::string> labels);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

private:
    std::shared_ptr<ExpressionNode> operand_;
    BinaryMetadata output_meta_;
    std::vector<size_t> selected_indices_;
    mutable std::vector<double> operand_row_buf_;
};

class QUIVER_API ExpressionRenameAgents final : public ExpressionNode {
public:
    ExpressionRenameAgents(std::shared_ptr<ExpressionNode> operand,
                           std::vector<std::pair<std::string, std::string>> mapping);

    const BinaryMetadata& metadata() const override;
    void compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const override;
    void collect_input_files(std::vector<BinaryFile*>& out) const override;

private:
    std::shared_ptr<ExpressionNode> operand_;
    BinaryMetadata output_meta_;
};

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_NODE_H
