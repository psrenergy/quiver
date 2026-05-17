#include "quiver/expression/expression_node.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace quiver {

ExpressionScalar::ExpressionScalar(double value, BinaryMetadata broadcast_meta)
    : value_(value), broadcast_meta_(std::move(broadcast_meta)) {}

const BinaryMetadata& ExpressionScalar::metadata() const {
    return broadcast_meta_;
}

void ExpressionScalar::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& out) const {
    out.assign(broadcast_meta_.labels.size(), value_);
}

void ExpressionScalar::collect_input_files(std::vector<BinaryFile*>& /*out*/) const {}

}  // namespace quiver
