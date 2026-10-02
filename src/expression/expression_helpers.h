#ifndef QUIVER_EXPRESSION_HELPERS_H
#define QUIVER_EXPRESSION_HELPERS_H

#include "quiver/binary/binary_metadata.h"
#include "quiver/expression/expression_node.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
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

        throw std::runtime_error(
            "Cannot apply: dimension '" + l_dim.name + "' has incompatible sizes " + std::to_string(l_size) + " vs " +
            std::to_string(r_size) + " (broadcasting requires n x n, 1 x n, or n x 1)"
        );
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
            throw std::runtime_error(
                "Cannot apply: dimension '" + l_dim.name + "' is a time dimension on " + time_side + " but not on " +
                nontime_side
            );
        }

        if (!l_time) {
            continue;
        }

        const auto& lp = *l_dim.time;
        const auto& rp = *r_dim.time;
        const auto l_parent = parent_name_of(lp.parent_dimension_index, lhs);
        const auto r_parent = parent_name_of(rp.parent_dimension_index, rhs);
        if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value || l_parent != r_parent) {
            throw std::runtime_error(
                "Cannot apply: time dimension '" + l_dim.name + "' has incompatible TimeProperties"
            );
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

// The one label rule for every broadcasting node: every operand with more than one label must carry
// the same label set, and a single-label operand broadcasts its one value across it whatever that
// label is called. When every operand has a single label, the output takes the primary operand's.
inline std::vector<std::string>
broadcast_labels(std::initializer_list<const BinaryMetadata*> sources, const BinaryMetadata& primary) {
    const std::vector<std::string>* labels = nullptr;
    for (const auto* src : sources) {
        if (src->labels.size() <= 1) {
            continue;
        }
        if (labels == nullptr) {
            labels = &src->labels;
        } else if (src->labels != *labels) {
            throw std::runtime_error(
                "Cannot apply: labels are incompatible across operands "
                "(non-singleton label sets must match)"
            );
        }
    }
    return labels != nullptr ? *labels : primary.labels;
}

// Output metadata of a broadcasting node, shared by ExpressionBinary ({lhs, rhs}, primary lhs) and
// ExpressionTernary ({cond, then, else}, primary then). The order of `sources` is the output
// dimension order: the union of dimension names, first occurrence first, each sized as the max over
// the sources that have it (validate_shape_compatibility has already checked the sizes broadcast),
// with time properties and the parent link taken from the first source that has it. version and
// unit come from `primary`. initial_datetime comes from the first source with a time dimension,
// else from `primary`. The pairwise validate_shape_compatibility calls force every time-bearing
// source to agree, so only that fallback depends on which operand is primary.
inline BinaryMetadata
build_broadcast_metadata(std::initializer_list<const BinaryMetadata*> sources, const BinaryMetadata& primary) {
    BinaryMetadata out;
    out.version = primary.version;
    out.unit = primary.unit;
    out.labels = broadcast_labels(sources, primary);
    out.initial_datetime = primary.initial_datetime;
    for (const auto* src : sources) {
        if (any_time_dim(src->dimensions)) {
            out.initial_datetime = src->initial_datetime;
            break;
        }
    }

    std::unordered_map<std::string, int> output_index_by_name;
    for (const auto* src : sources) {
        for (const auto& dim : src->dimensions) {
            if (output_index_by_name.count(dim.name)) {
                continue;
            }
            int64_t out_size = dim.size;
            for (const auto* other : sources) {
                const auto idx = find_dim_index(other->dimensions, dim.name);
                if (idx >= 0) {
                    out_size = std::max(out_size, other->dimensions[idx].size);
                }
            }
            out.dimensions.push_back(Dimension{dim.name, out_size, dim.time});
            output_index_by_name[dim.name] = static_cast<int>(out.dimensions.size()) - 1;
        }
    }

    for (auto& out_d : out.dimensions) {
        if (!out_d.is_time_dimension()) {
            continue;
        }
        const BinaryMetadata* src_meta = nullptr;
        int src_idx = -1;
        for (const auto* s : sources) {
            src_idx = find_dim_index(s->dimensions, out_d.name);
            if (src_idx >= 0) {
                src_meta = s;
                break;
            }
        }
        const int64_t src_parent_idx = src_meta->dimensions[src_idx].time->parent_dimension_index;
        if (src_parent_idx < 0) {
            out_d.time->parent_dimension_index = -1;
            continue;
        }
        const std::string& parent_name = src_meta->dimensions[src_parent_idx].name;
        out_d.time->parent_dimension_index = output_index_by_name.find(parent_name)->second;
    }
    return out;
}

inline std::string aggregation_operation_label(ExpressionAggregate::Operation op) {
    switch (op) {
    case ExpressionAggregate::Operation::Sum:
        return "sum";
    case ExpressionAggregate::Operation::Mean:
        return "mean";
    case ExpressionAggregate::Operation::Min:
        return "min";
    case ExpressionAggregate::Operation::Max:
        return "max";
    case ExpressionAggregate::Operation::Percentile:
        return "percentile";
    }
    throw std::runtime_error("Cannot label aggregation: unhandled Operation variant");
}

inline void validate_aggregation_param(
    ExpressionAggregate::Operation op,
    std::optional<double> parameter,
    const std::string& fn_label
) {
    const bool needs_param = (op == ExpressionAggregate::Operation::Percentile);
    if (needs_param && !parameter.has_value()) {
        throw std::runtime_error("Cannot " + fn_label + ": operation 'percentile' requires a parameter");
    }
    if (!needs_param && parameter.has_value()) {
        throw std::runtime_error(
            "Cannot " + fn_label + ": operation '" + aggregation_operation_label(op) + "' does not accept a parameter"
        );
    }
    if (needs_param && (*parameter < 0.0 || *parameter > 1.0)) {
        throw std::runtime_error(
            "Cannot " + fn_label + ": percentile must be in [0, 1], got " + std::to_string(*parameter)
        );
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

inline BroadcastOperand
make_broadcast_operand(const BinaryMetadata& operand_meta, const std::vector<Dimension>& out_dims) {
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
inline void compute_broadcast_operand_row(
    const BroadcastOperand& op,
    const ExpressionNode& node,
    const std::vector<int64_t>& dims
) {
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
inline void aggregation_accumulate(
    ExpressionAggregate::Operation op,
    AggregationState& state,
    std::vector<double>& percentile_scratch,
    double value
) {
    if (std::isnan(value)) {
        return;
    }
    switch (op) {
    case ExpressionAggregate::Operation::Sum:
    case ExpressionAggregate::Operation::Mean:
        state.sum += value;
        ++state.count;
        break;
    case ExpressionAggregate::Operation::Min:
        if (value < state.min) {
            state.min = value;
        }
        ++state.count;
        break;
    case ExpressionAggregate::Operation::Max:
        if (value > state.max) {
            state.max = value;
        }
        ++state.count;
        break;
    case ExpressionAggregate::Operation::Percentile:
        percentile_scratch.push_back(value);
        break;
    }
}

// An all-NaN (empty) accumulation yields NaN.
inline double aggregation_finalize(
    ExpressionAggregate::Operation op,
    const AggregationState& state,
    std::vector<double>& percentile_scratch,
    const std::optional<double>& parameter
) {
    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    switch (op) {
    case ExpressionAggregate::Operation::Sum:
        return (state.count > 0) ? state.sum : nan_value;
    case ExpressionAggregate::Operation::Mean:
        return (state.count > 0) ? state.sum / static_cast<double>(state.count) : nan_value;
    case ExpressionAggregate::Operation::Min:
        return (state.count > 0) ? state.min : nan_value;
    case ExpressionAggregate::Operation::Max:
        return (state.count > 0) ? state.max : nan_value;
    case ExpressionAggregate::Operation::Percentile:
        return compute_percentile(percentile_scratch, *parameter);
    }
    return nan_value;
}

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_HELPERS_H
