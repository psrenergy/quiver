#include "quiver/expression/expression_node.h"

#include "quiver/binary/binary_file.h"
#include "quiver/binary/iteration.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

ExpressionFile::ExpressionFile(const std::string& path) : meta_(BinaryMetadata::from_toml_file(path)), file_(path) {
    dim_map_.reserve(meta_.dimensions.size());
}

const BinaryMetadata& ExpressionFile::metadata() const {
    return meta_;
}

void ExpressionFile::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    for (size_t i = 0; i < meta_.dimensions.size(); ++i) {
        dim_map_[meta_.dimensions[i].name] = dims[i];
    }
    out = file_.read(dim_map_, /*allow_nulls=*/true);
}

void ExpressionFile::collect_input_files(std::vector<BinaryFile*>& out) const {
    out.push_back(&file_);
}

ExpressionScalar::ExpressionScalar(double value, BinaryMetadata broadcast_meta)
    : value_(value), broadcast_meta_(std::move(broadcast_meta)) {}

const BinaryMetadata& ExpressionScalar::metadata() const {
    return broadcast_meta_;
}

void ExpressionScalar::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& out) const {
    out.assign(broadcast_meta_.labels.size(), value_);
}

void ExpressionScalar::collect_input_files(std::vector<BinaryFile*>& /*out*/) const {}

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

void validate_unit_match(const BinaryMetadata& lhs, const BinaryMetadata& rhs) {
    if (lhs.unit != rhs.unit) {
        throw std::runtime_error("Cannot apply: units differ ('" + lhs.unit + "' vs '" + rhs.unit + "')");
    }
}

void validate_shape_compatibility(const BinaryMetadata& lhs, const BinaryMetadata& rhs) {
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

void validate_compatibility(const BinaryMetadata& lhs, const BinaryMetadata& rhs) {
    validate_unit_match(lhs, rhs);
    validate_shape_compatibility(lhs, rhs);
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

std::vector<std::string> compute_ternary_output_labels(const std::vector<std::string>& c_labels,
                                                       const std::vector<std::string>& t_labels,
                                                       const std::vector<std::string>& e_labels) {
    const std::vector<const std::vector<std::string>*> non_singleton = [&] {
        std::vector<const std::vector<std::string>*> v;
        if (c_labels.size() > 1)
            v.push_back(&c_labels);
        if (t_labels.size() > 1)
            v.push_back(&t_labels);
        if (e_labels.size() > 1)
            v.push_back(&e_labels);
        return v;
    }();

    if (non_singleton.empty()) {
        return t_labels;
    }

    for (size_t i = 1; i < non_singleton.size(); ++i) {
        if (*non_singleton[i] != *non_singleton[0]) {
            throw std::runtime_error("Cannot apply: labels are incompatible across operands "
                                     "(non-singleton label sets must match)");
        }
    }
    return *non_singleton[0];
}

BinaryMetadata build_ternary_broadcast_metadata(const BinaryMetadata& cond,
                                                const BinaryMetadata& then_meta,
                                                const BinaryMetadata& else_meta,
                                                std::vector<std::string> output_labels) {
    BinaryMetadata out;
    out.version = then_meta.version;
    out.unit = then_meta.unit;  // validated == else_meta.unit; cond.unit is ignored
    out.labels = std::move(output_labels);

    const auto then_has_time = any_time_dim(then_meta.dimensions);
    const auto else_has_time = any_time_dim(else_meta.dimensions);
    const auto cond_has_time = any_time_dim(cond.dimensions);
    if (then_has_time) {
        out.initial_datetime = then_meta.initial_datetime;
    } else if (else_has_time) {
        out.initial_datetime = else_meta.initial_datetime;
    } else if (cond_has_time) {
        out.initial_datetime = cond.initial_datetime;
    } else {
        out.initial_datetime = then_meta.initial_datetime;
    }

    const std::vector<const BinaryMetadata*> sources = {&cond, &then_meta, &else_meta};
    std::unordered_map<std::string, int> output_index_by_name;
    for (const auto* src : sources) {
        for (const auto& dim : src->dimensions) {
            if (output_index_by_name.count(dim.name))
                continue;
            int64_t out_size = dim.size;
            for (const auto* other : sources) {
                if (other == src)
                    continue;
                auto idx = find_dim_index(other->dimensions, dim.name);
                if (idx >= 0) {
                    out_size = std::max(out_size, other->dimensions[idx].size);
                }
            }
            Dimension d{dim.name, out_size, dim.time};
            out.dimensions.push_back(std::move(d));
            output_index_by_name[dim.name] = static_cast<int>(out.dimensions.size()) - 1;
        }
    }

    for (auto& out_d : out.dimensions) {
        if (!out_d.is_time_dimension())
            continue;
        const BinaryMetadata* src_meta = nullptr;
        int src_idx = -1;
        for (const auto* s : sources) {
            src_idx = find_dim_index(s->dimensions, out_d.name);
            if (src_idx >= 0) {
                src_meta = s;
                break;
            }
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

template <typename Op>
Op parse_aggregation_operation_name(const std::string& name, const std::string& fn_label) {
    if (name == "sum")
        return Op::Sum;
    if (name == "mean")
        return Op::Mean;
    if (name == "min")
        return Op::Min;
    if (name == "max")
        return Op::Max;
    if (name == "percentile")
        return Op::Percentile;
    throw std::runtime_error("Cannot " + fn_label + ": unknown operation '" + name +
                             "' (expected one of: sum, mean, min, max, percentile)");
}

template <typename Op>
std::string aggregation_operation_label(Op op) {
    switch (op) {
    case Op::Sum:
        return "sum";
    case Op::Mean:
        return "mean";
    case Op::Min:
        return "min";
    case Op::Max:
        return "max";
    case Op::Percentile:
        return "percentile";
    }
    throw std::runtime_error("Cannot label aggregation: unhandled Operation variant");
}

template <typename Op>
void validate_aggregation_param(Op op, std::optional<double> parameter, const std::string& fn_label) {
    const bool needs_param = (op == Op::Percentile);
    if (needs_param && !parameter.has_value()) {
        throw std::runtime_error("Cannot " + fn_label + ": operation 'percentile' requires a parameter");
    }
    if (!needs_param && parameter.has_value()) {
        throw std::runtime_error("Cannot " + fn_label + ": operation '" + aggregation_operation_label(op) +
                                 "' does not accept a parameter");
    }
    if (needs_param && (*parameter < 0.0 || *parameter > 1.0)) {
        throw std::runtime_error("Cannot " + fn_label + ": percentile must be in [0, 1], got " +
                                 std::to_string(*parameter));
    }
}

double compute_percentile(std::vector<double>& values, double fraction) {
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::sort(values.begin(), values.end());
    const auto n = values.size();
    if (n == 1) {
        return values[0];
    }
    const auto pos = static_cast<double>(n - 1) * fraction;
    const auto lo = static_cast<size_t>(std::floor(pos));
    const auto hi = static_cast<size_t>(std::ceil(pos));
    if (lo == hi) {
        return values[lo];
    }
    const double frac = pos - static_cast<double>(lo);
    return values[lo] * (1.0 - frac) + values[hi] * frac;
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

void ExpressionBinary::collect_input_files(std::vector<BinaryFile*>& out) const {
    lhs_->collect_input_files(out);
    rhs_->collect_input_files(out);
}

double ExpressionUnary::apply(Operation operation, double x) {
    switch (operation) {
    case Operation::Negate:
        return -x;
    case Operation::Abs:
        return std::abs(x);
    case Operation::Sqrt:
        return std::sqrt(x);
    case Operation::Log:
        return std::log(x);
    case Operation::Exp:
        return std::exp(x);
    }
    throw std::runtime_error("Cannot apply: unhandled ExpressionUnary::Operation variant");
}

ExpressionUnary::ExpressionUnary(Operation operation, std::shared_ptr<ExpressionNode> operand)
    : operation_(operation), operand_(std::move(operand)) {
    operand_row_buf_.resize(operand_->metadata().labels.size());
}

const BinaryMetadata& ExpressionUnary::metadata() const {
    return operand_->metadata();
}

void ExpressionUnary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto n = operand_row_buf_.size();
    if (out.size() != n) {
        out.resize(n);
    }
    operand_->compute_row(dims, operand_row_buf_);
    for (size_t k = 0; k < n; ++k) {
        out[k] = apply(operation_, operand_row_buf_[k]);
    }
}

void ExpressionUnary::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

double ExpressionTernary::apply(Operation operation, double condition, double then_value, double else_value) {
    switch (operation) {
    case Operation::IfElse:
        if (std::isnan(condition)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return (condition != 0.0) ? then_value : else_value;
    }
    throw std::runtime_error("Cannot apply: unhandled ExpressionTernary::Operation variant");
}

ExpressionTernary::ExpressionTernary(Operation operation,
                                     std::shared_ptr<ExpressionNode> condition,
                                     std::shared_ptr<ExpressionNode> then_value,
                                     std::shared_ptr<ExpressionNode> else_value)
    : operation_(operation), condition_(std::move(condition)), then_value_(std::move(then_value)),
      else_value_(std::move(else_value)) {
    const auto& condition_meta = condition_->metadata();
    const auto& then_meta = then_value_->metadata();
    const auto& else_meta = else_value_->metadata();

    validate_unit_match(then_meta, else_meta);
    validate_shape_compatibility(condition_meta, then_meta);
    validate_shape_compatibility(then_meta, else_meta);
    validate_shape_compatibility(condition_meta, else_meta);

    auto output_labels = compute_ternary_output_labels(condition_meta.labels, then_meta.labels, else_meta.labels);
    broadcast_meta_ = build_ternary_broadcast_metadata(condition_meta, then_meta, else_meta, std::move(output_labels));
    broadcast_meta_.validate();

    const auto& out_dims = broadcast_meta_.dimensions;
    condition_dim_sizes_.assign(out_dims.size(), 0);
    then_dim_sizes_.assign(out_dims.size(), 0);
    else_dim_sizes_.assign(out_dims.size(), 0);
    condition_to_out_.assign(condition_meta.dimensions.size(), -1);
    then_to_out_.assign(then_meta.dimensions.size(), -1);
    else_to_out_.assign(else_meta.dimensions.size(), -1);

    for (size_t out_i = 0; out_i < out_dims.size(); ++out_i) {
        const auto ci = find_dim_index(condition_meta.dimensions, out_dims[out_i].name);
        const auto ti = find_dim_index(then_meta.dimensions, out_dims[out_i].name);
        const auto ei = find_dim_index(else_meta.dimensions, out_dims[out_i].name);
        condition_dim_sizes_[out_i] = (ci >= 0) ? condition_meta.dimensions[ci].size : 0;
        then_dim_sizes_[out_i] = (ti >= 0) ? then_meta.dimensions[ti].size : 0;
        else_dim_sizes_[out_i] = (ei >= 0) ? else_meta.dimensions[ei].size : 0;
        if (ci >= 0)
            condition_to_out_[ci] = static_cast<int>(out_i);
        if (ti >= 0)
            then_to_out_[ti] = static_cast<int>(out_i);
        if (ei >= 0)
            else_to_out_[ei] = static_cast<int>(out_i);
    }

    condition_label_count_ = condition_meta.labels.size();
    then_label_count_ = then_meta.labels.size();
    else_label_count_ = else_meta.labels.size();

    condition_dims_buf_.resize(condition_meta.dimensions.size());
    then_dims_buf_.resize(then_meta.dimensions.size());
    else_dims_buf_.resize(else_meta.dimensions.size());
    condition_buf_.resize(condition_label_count_);
    then_buf_.resize(then_label_count_);
    else_buf_.resize(else_label_count_);
}

const BinaryMetadata& ExpressionTernary::metadata() const {
    return broadcast_meta_;
}

void ExpressionTernary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count) {
        out.resize(out_label_count);
    }

    for (size_t ci = 0; ci < condition_dims_buf_.size(); ++ci) {
        const auto out_i = condition_to_out_[ci];
        auto coord = dims[out_i];
        if (condition_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        condition_dims_buf_[ci] = coord;
    }
    for (size_t ti = 0; ti < then_dims_buf_.size(); ++ti) {
        const auto out_i = then_to_out_[ti];
        auto coord = dims[out_i];
        if (then_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        then_dims_buf_[ti] = coord;
    }
    for (size_t ei = 0; ei < else_dims_buf_.size(); ++ei) {
        const auto out_i = else_to_out_[ei];
        auto coord = dims[out_i];
        if (else_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        else_dims_buf_[ei] = coord;
    }

    condition_->compute_row(condition_dims_buf_, condition_buf_);
    then_value_->compute_row(then_dims_buf_, then_buf_);
    else_value_->compute_row(else_dims_buf_, else_buf_);

    for (size_t k = 0; k < out_label_count; ++k) {
        const size_t ck = (condition_label_count_ == 1) ? 0 : k;
        const size_t tk = (then_label_count_ == 1) ? 0 : k;
        const size_t ek = (else_label_count_ == 1) ? 0 : k;
        out[k] = apply(operation_, condition_buf_[ck], then_buf_[tk], else_buf_[ek]);
    }
}

void ExpressionTernary::collect_input_files(std::vector<BinaryFile*>& out) const {
    condition_->collect_input_files(out);
    then_value_->collect_input_files(out);
    else_value_->collect_input_files(out);
}

ExpressionAggregate::Operation ExpressionAggregate::parse_operation(const std::string& name) {
    return parse_aggregation_operation_name<Operation>(name, "aggregate");
}

ExpressionAggregate::ExpressionAggregate(Operation operation,
                                         std::shared_ptr<ExpressionNode> operand,
                                         std::string dimension_name,
                                         std::optional<double> parameter)
    : operation_(operation), operand_(std::move(operand)), dimension_name_(std::move(dimension_name)),
      parameter_(parameter) {
    validate_aggregation_param(operation_, parameter_, "aggregate");

    const auto& operand_meta = operand_->metadata();

    const int reduced_idx = find_dim_index(operand_meta.dimensions, dimension_name_);
    if (reduced_idx < 0) {
        throw std::runtime_error("Dimension not found: '" + dimension_name_ + "' in operand metadata");
    }
    reduced_operand_index_ = reduced_idx;

    const auto& reduced_dim = operand_meta.dimensions[reduced_idx];
    const int64_t grandparent_orig_idx =
        reduced_dim.is_time_dimension() ? reduced_dim.time->parent_dimension_index : -1;

    output_meta_ = operand_meta;
    output_meta_.dimensions.erase(output_meta_.dimensions.begin() + reduced_idx);
    if (reduced_dim.is_time_dimension()) {
        --output_meta_.number_of_time_dimensions;
    }

    operand_to_out_.assign(operand_meta.dimensions.size(), -1);
    for (size_t i = 0; i < operand_meta.dimensions.size(); ++i) {
        if (static_cast<int>(i) == reduced_idx) {
            continue;
        }
        operand_to_out_[i] = (static_cast<int>(i) < reduced_idx) ? static_cast<int>(i) : static_cast<int>(i) - 1;
    }

    for (size_t out_i = 0; out_i < output_meta_.dimensions.size(); ++out_i) {
        auto& out_dim = output_meta_.dimensions[out_i];
        if (!out_dim.is_time_dimension()) {
            continue;
        }
        const int operand_idx =
            (static_cast<int>(out_i) < reduced_idx) ? static_cast<int>(out_i) : static_cast<int>(out_i) + 1;
        const int64_t orig_parent = operand_meta.dimensions[operand_idx].time->parent_dimension_index;
        if (orig_parent < 0) {
            out_dim.time->parent_dimension_index = -1;
        } else if (orig_parent == reduced_idx) {
            if (grandparent_orig_idx < 0) {
                out_dim.time->parent_dimension_index = -1;
            } else {
                out_dim.time->parent_dimension_index =
                    (grandparent_orig_idx < reduced_idx) ? grandparent_orig_idx : grandparent_orig_idx - 1;
            }
        } else {
            out_dim.time->parent_dimension_index = (orig_parent < reduced_idx) ? orig_parent : orig_parent - 1;
        }
    }

    output_meta_.validate();

    operand_dims_buf_.resize(operand_meta.dimensions.size());
    operand_row_buf_.resize(operand_meta.labels.size());
    if (operation_ == Operation::Percentile) {
        percentile_scratch_.resize(operand_meta.labels.size());
    }
}

const BinaryMetadata& ExpressionAggregate::metadata() const {
    return output_meta_;
}

void ExpressionAggregate::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto label_count = operand_row_buf_.size();
    if (out.size() != label_count) {
        out.resize(label_count);
    }

    const auto& operand_meta = operand_->metadata();

    for (size_t i = 0; i < operand_meta.dimensions.size(); ++i) {
        if (static_cast<int>(i) == reduced_operand_index_) {
            continue;
        }
        operand_dims_buf_[i] = dims[operand_to_out_[i]];
    }
    operand_dims_buf_[reduced_operand_index_] = 1;

    const auto& reduced_dim = operand_meta.dimensions[reduced_operand_index_];
    int64_t start = 1;
    int64_t end = reduced_dim.size;
    if (reduced_dim.is_time_dimension()) {
        const auto& tp = *reduced_dim.time;
        const int64_t parent_idx = tp.parent_dimension_index;
        const auto sizes = dimension_sizes_at_values(operand_meta, operand_dims_buf_);
        end = sizes[reduced_operand_index_];
        if (parent_idx < 0) {
            start = tp.initial_value;
        } else {
            const auto& parent_dim = operand_meta.dimensions[parent_idx];
            const int64_t parent_initial = parent_dim.is_time_dimension() ? parent_dim.time->initial_value : 1;
            start = (operand_dims_buf_[parent_idx] == parent_initial) ? tp.initial_value : 1;
        }
    }

    std::vector<double> sum_buf(label_count, 0.0);
    std::vector<int64_t> count_buf(label_count, 0);
    std::vector<double> min_buf(label_count, std::numeric_limits<double>::infinity());
    std::vector<double> max_buf(label_count, -std::numeric_limits<double>::infinity());

    if (operation_ == Operation::Percentile) {
        for (auto& scratch : percentile_scratch_) {
            scratch.clear();
        }
    }

    for (int64_t v = start; v <= end; ++v) {
        operand_dims_buf_[reduced_operand_index_] = v;
        operand_->compute_row(operand_dims_buf_, operand_row_buf_);

        for (size_t k = 0; k < label_count; ++k) {
            const double value = operand_row_buf_[k];
            if (std::isnan(value)) {
                continue;
            }
            switch (operation_) {
            case Operation::Sum:
            case Operation::Mean:
                sum_buf[k] += value;
                ++count_buf[k];
                break;
            case Operation::Min:
                if (value < min_buf[k]) {
                    min_buf[k] = value;
                }
                ++count_buf[k];
                break;
            case Operation::Max:
                if (value > max_buf[k]) {
                    max_buf[k] = value;
                }
                ++count_buf[k];
                break;
            case Operation::Percentile:
                percentile_scratch_[k].push_back(value);
                break;
            }
        }
    }

    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    for (size_t k = 0; k < label_count; ++k) {
        switch (operation_) {
        case Operation::Sum:
            out[k] = (count_buf[k] > 0) ? sum_buf[k] : nan_value;
            break;
        case Operation::Mean:
            out[k] = (count_buf[k] > 0) ? sum_buf[k] / static_cast<double>(count_buf[k]) : nan_value;
            break;
        case Operation::Min:
            out[k] = (count_buf[k] > 0) ? min_buf[k] : nan_value;
            break;
        case Operation::Max:
            out[k] = (count_buf[k] > 0) ? max_buf[k] : nan_value;
            break;
        case Operation::Percentile:
            out[k] = compute_percentile(percentile_scratch_[k], *parameter_);
            break;
        }
    }
}

void ExpressionAggregate::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

ExpressionAggregateAgents::Operation ExpressionAggregateAgents::parse_operation(const std::string& name) {
    return parse_aggregation_operation_name<Operation>(name, "aggregate_agents");
}

ExpressionAggregateAgents::ExpressionAggregateAgents(Operation operation,
                                                     std::shared_ptr<ExpressionNode> operand,
                                                     std::optional<double> parameter)
    : operation_(operation), operand_(std::move(operand)), parameter_(parameter) {
    validate_aggregation_param(operation_, parameter_, "aggregate_agents");

    const auto& operand_meta = operand_->metadata();
    output_meta_ = operand_meta;
    output_meta_.labels = {aggregation_operation_label(operation_)};
    output_meta_.validate();

    operand_row_buf_.resize(operand_meta.labels.size());
}

const BinaryMetadata& ExpressionAggregateAgents::metadata() const {
    return output_meta_;
}

void ExpressionAggregateAgents::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    if (out.size() != 1) {
        out.resize(1);
    }

    operand_->compute_row(dims, operand_row_buf_);

    const double nan_value = std::numeric_limits<double>::quiet_NaN();

    switch (operation_) {
    case Operation::Sum: {
        double sum = 0.0;
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            sum += v;
            ++count;
        }
        out[0] = (count > 0) ? sum : nan_value;
        break;
    }
    case Operation::Mean: {
        double sum = 0.0;
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            sum += v;
            ++count;
        }
        out[0] = (count > 0) ? sum / static_cast<double>(count) : nan_value;
        break;
    }
    case Operation::Min: {
        double m = std::numeric_limits<double>::infinity();
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            if (v < m) {
                m = v;
            }
            ++count;
        }
        out[0] = (count > 0) ? m : nan_value;
        break;
    }
    case Operation::Max: {
        double m = -std::numeric_limits<double>::infinity();
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            if (v > m) {
                m = v;
            }
            ++count;
        }
        out[0] = (count > 0) ? m : nan_value;
        break;
    }
    case Operation::Percentile: {
        std::vector<double> scratch;
        scratch.reserve(operand_row_buf_.size());
        for (double v : operand_row_buf_) {
            if (!std::isnan(v)) {
                scratch.push_back(v);
            }
        }
        out[0] = compute_percentile(scratch, *parameter_);
        break;
    }
    }
}

void ExpressionAggregateAgents::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

}  // namespace quiver
