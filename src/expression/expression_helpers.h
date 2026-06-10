#ifndef QUIVER_EXPRESSION_HELPERS_H
#define QUIVER_EXPRESSION_HELPERS_H

#include "quiver/binary/binary_metadata.h"
#include "quiver/expression/expression_node.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

inline int find_dim_index(const std::vector<Dimension>& dims, const std::string& name) {
    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline bool any_time_dim(const std::vector<Dimension>& dims) {
    for (const auto& d : dims) {
        if (d.is_time_dimension()) {
            return true;
        }
    }
    return false;
}

inline std::string parent_name_of(int64_t parent_idx, const BinaryMetadata& m) {
    return (parent_idx >= 0) ? m.dimensions[parent_idx].name : std::string{};
}

inline void validate_unit_match(const BinaryMetadata& lhs, const BinaryMetadata& rhs) {
    if (lhs.unit != rhs.unit) {
        throw std::runtime_error("Cannot apply: units differ ('" + lhs.unit + "' vs '" + rhs.unit + "')");
    }
}

inline void validate_shape_compatibility(const BinaryMetadata& lhs, const BinaryMetadata& rhs) {
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

inline void validate_compatibility(const BinaryMetadata& lhs, const BinaryMetadata& rhs) {
    validate_unit_match(lhs, rhs);
    validate_shape_compatibility(lhs, rhs);
}

inline std::vector<std::string> compute_output_labels(const std::vector<std::string>& l_labels,
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

inline BinaryMetadata
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
        Dimension d{l_dim.name, out_size, l_dim.time};
        out.dimensions.push_back(std::move(d));
        output_index_by_name[l_dim.name] = static_cast<int>(out.dimensions.size()) - 1;
    }
    for (const auto& r_dim : rhs.dimensions) {
        if (output_index_by_name.count(r_dim.name))
            continue;
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
    return out;
}

inline std::vector<std::string> compute_ternary_output_labels(const std::vector<std::string>& c_labels,
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

inline BinaryMetadata build_ternary_broadcast_metadata(const BinaryMetadata& cond,
                                                       const BinaryMetadata& then_meta,
                                                       const BinaryMetadata& else_meta,
                                                       std::vector<std::string> output_labels) {
    BinaryMetadata out;
    out.version = then_meta.version;
    out.unit = then_meta.unit;
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
    return out;
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

inline double compute_percentile(std::vector<double>& values, double fraction) {
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

// ============================================================================
// Broadcast operand helpers (shared by ExpressionBinary / ExpressionTernary)
// ============================================================================

inline BroadcastOperand make_broadcast_operand(const BinaryMetadata& operand_meta,
                                               const std::vector<Dimension>& out_dims) {
    BroadcastOperand op;
    op.dim_sizes.assign(out_dims.size(), 0);
    op.to_out.assign(operand_meta.dimensions.size(), -1);
    for (size_t out_i = 0; out_i < out_dims.size(); ++out_i) {
        const auto idx = find_dim_index(operand_meta.dimensions, out_dims[out_i].name);
        if (idx >= 0) {
            op.dim_sizes[out_i] = operand_meta.dimensions[idx].size;
            op.to_out[idx] = static_cast<int>(out_i);
        }
    }
    op.label_count = operand_meta.labels.size();
    op.dims_buf.resize(operand_meta.dimensions.size());
    op.row_buf.resize(op.label_count);
    return op;
}

// Translate output coordinates into the operand's dimension space (size-1
// dimensions broadcast by pinning their coordinate to 1) and compute the
// operand's row into its reusable row_buf.
inline void compute_broadcast_operand_row(const BroadcastOperand& op,
                                          const ExpressionNode& node,
                                          const std::vector<int64_t>& dims) {
    for (size_t i = 0; i < op.dims_buf.size(); ++i) {
        const auto out_i = op.to_out[i];
        auto coord = dims[out_i];
        if (op.dim_sizes[out_i] == 1) {
            coord = 1;
        }
        op.dims_buf[i] = coord;
    }
    node.compute_row(op.dims_buf, op.row_buf);
}

// Single-label operands broadcast their one value across all output labels
inline size_t broadcast_label_index(const BroadcastOperand& op, size_t k) {
    return (op.label_count == 1) ? 0 : k;
}

// ============================================================================
// Aggregation accumulation (shared by ExpressionAggregate / ExpressionAggregateAgents)
// ============================================================================

struct AggregationState {
    double sum = 0.0;
    int64_t count = 0;
    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();
};

// NaN inputs are skipped; Percentile collects into the caller's scratch buffer.
template <typename Op>
void aggregation_accumulate(Op op, AggregationState& state, std::vector<double>& percentile_scratch, double value) {
    if (std::isnan(value)) {
        return;
    }
    switch (op) {
    case Op::Sum:
    case Op::Mean:
        state.sum += value;
        ++state.count;
        break;
    case Op::Min:
        if (value < state.min) {
            state.min = value;
        }
        ++state.count;
        break;
    case Op::Max:
        if (value > state.max) {
            state.max = value;
        }
        ++state.count;
        break;
    case Op::Percentile:
        percentile_scratch.push_back(value);
        break;
    }
}

// An all-NaN (empty) accumulation yields NaN.
template <typename Op>
double aggregation_finalize(Op op,
                            const AggregationState& state,
                            std::vector<double>& percentile_scratch,
                            const std::optional<double>& parameter) {
    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    switch (op) {
    case Op::Sum:
        return (state.count > 0) ? state.sum : nan_value;
    case Op::Mean:
        return (state.count > 0) ? state.sum / static_cast<double>(state.count) : nan_value;
    case Op::Min:
        return (state.count > 0) ? state.min : nan_value;
    case Op::Max:
        return (state.count > 0) ? state.max : nan_value;
    case Op::Percentile:
        return compute_percentile(percentile_scratch, *parameter);
    }
    return nan_value;
}

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_HELPERS_H
