#include "quiver/expression/expression_node.h"

#include "quiver/binary/binary_file.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

ExpressionFile::ExpressionFile(const BinaryFile& file) : path_(file.get_file_path()), meta_(file.get_metadata()) {
    if (!file.is_read_mode()) {
        throw std::runtime_error("Cannot create_expression: BinaryFile must be opened in read mode");
    }
}

const BinaryMetadata& ExpressionFile::metadata() const {
    return meta_;
}

void ExpressionFile::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    if (!file_) {
        file_ = std::make_unique<BinaryFile>(BinaryFile::open_file(path_, 'r'));
        dim_map_.reserve(meta_.dimensions.size());
        for (const auto& dim : meta_.dimensions) {
            dim_map_[dim.name] = 0;
        }
    }
    for (size_t i = 0; i < meta_.dimensions.size(); ++i) {
        dim_map_[meta_.dimensions[i].name] = dims[i];
    }
    out = file_->read(dim_map_, /*allow_nulls=*/true);
}

ExpressionScalar::ExpressionScalar(double value, BinaryMetadata broadcast_meta)
    : value_(value), broadcast_meta_(std::move(broadcast_meta)) {}

const BinaryMetadata& ExpressionScalar::metadata() const {
    return broadcast_meta_;
}

void ExpressionScalar::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& out) const {
    out.assign(broadcast_meta_.labels.size(), value_);
}

namespace {

int find_dim_index(const std::vector<Dimension>& dims, const std::string& name) {
    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool any_time_dim(const std::vector<Dimension>& dims) {
    for (const auto& d : dims) {
        if (d.is_time_dimension()) {
            return true;
        }
    }
    return false;
}

std::string parent_name_of(int64_t parent_idx, const BinaryMetadata& m) {
    return (parent_idx >= 0) ? m.dimensions[parent_idx].name : std::string{};
}

void validate_compatibility(const BinaryMetadata& lhs, const BinaryMetadata& rhs) {
    if (lhs.unit != rhs.unit) {
        throw std::runtime_error("Cannot apply: units differ ('" + lhs.unit + "' vs '" + rhs.unit + "')");
    }

    for (const auto& l_dim : lhs.dimensions) {
        auto r_idx = find_dim_index(rhs.dimensions, l_dim.name);
        if (r_idx < 0) {
            continue;
        }

        auto l_size = l_dim.size;
        auto r_size = rhs.dimensions[r_idx].size;

        if (l_size == r_size) {
            continue;
        }

        if (l_size == 1 || r_size == 1) {
            continue;
        }

        throw std::runtime_error("Cannot apply: dimension '" + l_dim.name + "' has incompatible sizes " +
                                 std::to_string(l_size) + " vs " + std::to_string(r_size) +
                                 " (broadcasting requires n x n, 1 x n, or n x 1)");
    }

    for (const auto& l_dim : lhs.dimensions) {
        auto r_idx = find_dim_index(rhs.dimensions, l_dim.name);
        if (r_idx < 0) {
            continue;
        }

        const auto& r_dim = rhs.dimensions[r_idx];
        const auto l_time = l_dim.is_time_dimension();
        const auto r_time = r_dim.is_time_dimension();
        if (l_time != r_time) {
            const std::string time_side = l_time ? "lhs" : "rhs";
            const std::string nontime_side = l_time ? "rhs" : "lhs";
            throw std::runtime_error("Cannot apply: dimension '" + l_dim.name + "' is a time dimension on " +
                                     time_side + " but not on " + nontime_side);
        }

        if (!l_time) {
            continue;
        }

        const auto& lp = *l_dim.time;
        const auto& rp = *r_dim.time;
        const auto l_parent = parent_name_of(lp.parent_dimension_index, lhs);
        const auto r_parent = parent_name_of(rp.parent_dimension_index, rhs);
        if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value || l_parent != r_parent) {
            throw std::runtime_error("Cannot apply: time dimension '" + l_dim.name +
                                     "' has incompatible TimeProperties");
        }
    }

    const auto lhs_has_time = any_time_dim(lhs.dimensions);
    const auto rhs_has_time = any_time_dim(rhs.dimensions);
    if (lhs_has_time && rhs_has_time && lhs.initial_datetime != rhs.initial_datetime) {
        throw std::runtime_error("Cannot apply: initial_datetime differs");
    }
}

std::vector<std::string> compute_output_labels(const std::vector<std::string>& l_labels,
                                               const std::vector<std::string>& r_labels) {
    const auto ll = l_labels.size();
    const auto rl = r_labels.size();
    if (ll == rl) {
        if (l_labels != r_labels) {
            throw std::runtime_error("Cannot apply: labels have same size " + std::to_string(ll) +
                                     " but different content");
        }
        return l_labels;
    }
    if (ll == 1 && rl > 1) {
        return r_labels;
    }
    if (rl == 1 && ll > 1) {
        return l_labels;
    }
    throw std::runtime_error("Cannot apply: labels have incompatible sizes " + std::to_string(ll) + " vs " +
                             std::to_string(rl));
}

BinaryMetadata
build_broadcast_metadata(const BinaryMetadata& lhs, const BinaryMetadata& rhs, std::vector<std::string> output_labels) {
    BinaryMetadata out;
    out.version = lhs.version;
    out.unit = lhs.unit;
    out.labels = std::move(output_labels);

    const auto lhs_has_time = any_time_dim(lhs.dimensions);
    const auto rhs_has_time = any_time_dim(rhs.dimensions);
    out.initial_datetime =
        lhs_has_time ? lhs.initial_datetime : (rhs_has_time ? rhs.initial_datetime : lhs.initial_datetime);

    std::unordered_map<std::string, int> output_index_by_name;
    for (const auto& l_dim : lhs.dimensions) {
        auto r_idx = find_dim_index(rhs.dimensions, l_dim.name);
        int64_t out_size = (r_idx >= 0) ? std::max(l_dim.size, rhs.dimensions[r_idx].size) : l_dim.size;
        Dimension d{l_dim.name, out_size, l_dim.time};  // copy time props from lhs (rhs equal)
        out.dimensions.push_back(std::move(d));
        output_index_by_name[l_dim.name] = static_cast<int>(out.dimensions.size()) - 1;
    }
    for (const auto& r_dim : rhs.dimensions) {
        if (output_index_by_name.count(r_dim.name))
            continue;  // already placed
        out.dimensions.push_back(r_dim);
        output_index_by_name[r_dim.name] = static_cast<int>(out.dimensions.size()) - 1;
    }
    for (auto& out_d : out.dimensions) {
        if (!out_d.is_time_dimension())
            continue;
        auto src_idx = find_dim_index(lhs.dimensions, out_d.name);
        const auto* src_meta = &lhs;
        if (src_idx < 0) {
            src_idx = find_dim_index(rhs.dimensions, out_d.name);
            src_meta = &rhs;
        }
        int64_t src_parent_idx = src_meta->dimensions[src_idx].time->parent_dimension_index;
        if (src_parent_idx < 0) {
            out_d.time->parent_dimension_index = -1;
            continue;
        }
        const std::string& parent_name = src_meta->dimensions[src_parent_idx].name;
        out_d.time->parent_dimension_index = output_index_by_name.find(parent_name)->second;
    }
    for (const auto& d : out.dimensions) {
        if (d.is_time_dimension()) {
            ++out.number_of_time_dimensions;
        }
    }
    return out;
}

}  // namespace

double ExpressionBinary::apply(Operation operation, double lhs, double rhs) {
    switch (operation) {
    case Operation::Add:
        return lhs + rhs;
    case Operation::Subtract:
        return lhs - rhs;
    case Operation::Multiply:
        return lhs * rhs;
    case Operation::Divide:
        return lhs / rhs;
    }
    throw std::runtime_error("Cannot apply: unhandled ExpressionBinary::Operation variant");
}

ExpressionBinary::ExpressionBinary(Operation operation,
                                   std::shared_ptr<ExpressionNode> lhs,
                                   std::shared_ptr<ExpressionNode> rhs)
    : operation_(operation), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {
    const auto& lhs_meta = lhs_->metadata();
    const auto& rhs_meta = rhs_->metadata();

    validate_compatibility(lhs_meta, rhs_meta);
    auto output_labels = compute_output_labels(lhs_meta.labels, rhs_meta.labels);
    broadcast_meta_ = build_broadcast_metadata(lhs_meta, rhs_meta, std::move(output_labels));
    broadcast_meta_.validate();

    const auto& out_dims = broadcast_meta_.dimensions;
    lhs_dim_sizes_.assign(out_dims.size(), 0);
    rhs_dim_sizes_.assign(out_dims.size(), 0);
    lhs_to_out_.assign(lhs_meta.dimensions.size(), -1);
    rhs_to_out_.assign(rhs_meta.dimensions.size(), -1);

    for (size_t out_i = 0; out_i < out_dims.size(); ++out_i) {
        const auto li = find_dim_index(lhs_meta.dimensions, out_dims[out_i].name);
        const auto ri = find_dim_index(rhs_meta.dimensions, out_dims[out_i].name);
        lhs_dim_sizes_[out_i] = (li >= 0) ? lhs_meta.dimensions[li].size : 0;
        rhs_dim_sizes_[out_i] = (ri >= 0) ? rhs_meta.dimensions[ri].size : 0;
        if (li >= 0) {
            lhs_to_out_[li] = static_cast<int>(out_i);
        }
        if (ri >= 0) {
            rhs_to_out_[ri] = static_cast<int>(out_i);
        }
    }

    lhs_label_count_ = lhs_meta.labels.size();
    rhs_label_count_ = rhs_meta.labels.size();

    lhs_dims_buf_.resize(lhs_meta.dimensions.size());
    rhs_dims_buf_.resize(rhs_meta.dimensions.size());
    lhs_buf_.resize(lhs_label_count_);
    rhs_buf_.resize(rhs_label_count_);
}

const BinaryMetadata& ExpressionBinary::metadata() const {
    return broadcast_meta_;
}

void ExpressionBinary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count) {
        out.resize(out_label_count);
    }

    for (size_t li = 0; li < lhs_dims_buf_.size(); ++li) {
        const auto out_i = lhs_to_out_[li];
        auto coord = dims[out_i];
        if (lhs_dim_sizes_[out_i] == 1) {
            coord = 1;  // size-1 broadcast clamp
        }
        lhs_dims_buf_[li] = coord;
    }
    for (size_t ri = 0; ri < rhs_dims_buf_.size(); ++ri) {
        const auto out_i = rhs_to_out_[ri];
        auto coord = dims[out_i];
        if (rhs_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        rhs_dims_buf_[ri] = coord;
    }

    lhs_->compute_row(lhs_dims_buf_, lhs_buf_);
    rhs_->compute_row(rhs_dims_buf_, rhs_buf_);

    for (size_t k = 0; k < out_label_count; ++k) {
        const size_t li = (lhs_label_count_ == 1) ? 0 : k;
        const size_t ri = (rhs_label_count_ == 1) ? 0 : k;
        out[k] = apply(operation_, lhs_buf_[li], rhs_buf_[ri]);
    }
}

ExpressionUnary::ExpressionUnary(Operation operation, std::shared_ptr<ExpressionNode> operand)
    : operation_(operation), operand_(std::move(operand)) {}

const BinaryMetadata& ExpressionUnary::metadata() const {
    throw std::runtime_error("Cannot get_metadata: ExpressionUnary is not yet implemented");
}

void ExpressionUnary::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& /*out*/) const {
    throw std::runtime_error("Cannot compute_row: ExpressionUnary is not yet implemented");
}

ExpressionTernary::ExpressionTernary(Operation operation,
                                     std::shared_ptr<ExpressionNode> first,
                                     std::shared_ptr<ExpressionNode> second,
                                     std::shared_ptr<ExpressionNode> third)
    : operation_(operation), first_(std::move(first)), second_(std::move(second)), third_(std::move(third)) {}

const BinaryMetadata& ExpressionTernary::metadata() const {
    throw std::runtime_error("Cannot get_metadata: ExpressionTernary is not yet implemented");
}

void ExpressionTernary::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& /*out*/) const {
    throw std::runtime_error("Cannot compute_row: ExpressionTernary is not yet implemented");
}

ExpressionAggregation::ExpressionAggregation(Operation operation,
                                             std::shared_ptr<ExpressionNode> operand,
                                             std::string dimension_to_reduce)
    : operation_(operation), operand_(std::move(operand)), dimension_to_reduce_(std::move(dimension_to_reduce)) {}

const BinaryMetadata& ExpressionAggregation::metadata() const {
    throw std::runtime_error("Cannot get_metadata: ExpressionAggregation is not yet implemented");
}

void ExpressionAggregation::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& /*out*/) const {
    throw std::runtime_error("Cannot compute_row: ExpressionAggregation is not yet implemented");
}

}  // namespace quiver
