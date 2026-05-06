#include "quiver/expr/node.h"

#include "quiver/binary/binary_file.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver::expr {

// ============================================================================
// FileNode
// ============================================================================

FileNode::FileNode(const BinaryFile& file) : path_(file.get_file_path()), meta_(file.get_metadata()) {}

BinaryMetadata FileNode::metadata() const {
    return meta_;
}

void FileNode::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    // D-10: lazy open on first compute_row call. Each FileNode owns its own
    // BinaryFile handle; multiple FileNodes referring to the same path open the
    // file independently (no cache).
    if (!file_) {
        file_ = std::make_unique<BinaryFile>(BinaryFile::open_file(path_, 'r'));
    }
    // The trusted-caller fast path read_into skips dimension validation per D-13.
    // The parent (BinaryOpNode or save engine) is responsible for delivering dims
    // that are valid for THIS node's metadata. When FileNode is the root of the
    // expression tree, dims come straight from quiver::binary::next_dimensions
    // against this same metadata — bounds-correct by construction.
    file_->read_into(dims, out, /*allow_nulls=*/true);
}

// ============================================================================
// ScalarNode
// ============================================================================

ScalarNode::ScalarNode(double value, BinaryMetadata broadcast_meta)
    : value_(value), broadcast_meta_(std::move(broadcast_meta)) {}

BinaryMetadata ScalarNode::metadata() const {
    return broadcast_meta_;
}

void ScalarNode::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& out) const {
    // The scalar value broadcasts uniformly across the labels axis of the
    // adopted (sibling) metadata. compute_row writes value_ into every label slot.
    out.assign(broadcast_meta_.labels.size(), value_);
}

// ============================================================================
// BinaryOpNode helpers (anonymous namespace)
// ============================================================================

namespace {

// Returns the index of `name` in the dimensions array, or -1 if absent.
int find_dim_index(const std::vector<Dimension>& dims, const std::string& name) {
    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
}

bool any_time_dim(const std::vector<Dimension>& dims) {
    for (const auto& d : dims)
        if (d.is_time_dimension())
            return true;
    return false;
}

// Apply binary op to two doubles. Division by zero produces IEEE 754 inf/NaN
// (project policy: clean code over defensive — see T-1-203 in plan threat model;
// downstream consumers handle NaN/inf, no defensive zero check here).
double apply_op(BinaryOp op, double a, double b) {
    switch (op) {
    case BinaryOp::Add:
        return a + b;
    case BinaryOp::Subtract:
        return a - b;
    case BinaryOp::Multiply:
        return a * b;
    case BinaryOp::Divide:
        return a / b;
    }
    return 0.0;  // unreachable; silences MSVC missing-return warning
}

}  // namespace

// ============================================================================
// BinaryOpNode
// ============================================================================

BinaryOpNode::BinaryOpNode(BinaryOp op, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
    : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {

    const auto lhs_meta = lhs_->metadata();
    const auto rhs_meta = rhs_->metadata();

    // ---- D-07: unit must match ----
    if (lhs_meta.unit != rhs_meta.unit) {
        throw std::runtime_error("Cannot apply binary operation: units differ ('" + lhs_meta.unit + "' vs '" +
                                 rhs_meta.unit + "')");
    }

    // ---- D-01 + D-06: shape rule (n×n / 1×n / n×1 per same-name dim) ----
    // For each dim shared by name: sizes must be equal, OR exactly one side is 1.
    // Dims that exist on only one side are kept verbatim in the output (no compatibility check).
    for (const auto& l_dim : lhs_meta.dimensions) {
        int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
        if (r_idx < 0)
            continue;  // dim only on lhs; D-02 keeps verbatim
        int64_t l_size = l_dim.size;
        int64_t r_size = rhs_meta.dimensions[r_idx].size;
        if (l_size == r_size)
            continue;
        if (l_size == 1 || r_size == 1)
            continue;
        throw std::runtime_error("Cannot apply binary operation: dimension '" + l_dim.name +
                                 "' has incompatible sizes " + std::to_string(l_size) + " vs " +
                                 std::to_string(r_size) + " (broadcasting requires n×n, 1×n, or n×1)");
    }

    // ---- D-04: same-name time dims → TimeProperties must match exactly ----
    for (const auto& l_dim : lhs_meta.dimensions) {
        if (!l_dim.is_time_dimension())
            continue;
        int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
        if (r_idx < 0)
            continue;
        const auto& r_dim = rhs_meta.dimensions[r_idx];
        if (!r_dim.is_time_dimension()) {
            throw std::runtime_error("Cannot apply binary operation: dimension '" + l_dim.name +
                                     "' is a time dimension on lhs but not on rhs");
        }
        const auto& lp = *l_dim.time;
        const auto& rp = *r_dim.time;
        if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value ||
            lp.parent_dimension_index != rp.parent_dimension_index) {
            throw std::runtime_error("Cannot apply binary operation: time dimension '" + l_dim.name +
                                     "' has incompatible TimeProperties");
        }
    }

    // ---- D-09: initial_datetime must match when both sides have any time dim ----
    const bool lhs_has_time = any_time_dim(lhs_meta.dimensions);
    const bool rhs_has_time = any_time_dim(rhs_meta.dimensions);
    if (lhs_has_time && rhs_has_time && lhs_meta.initial_datetime != rhs_meta.initial_datetime) {
        throw std::runtime_error("Cannot apply binary operation: initial_datetime differs");
    }

    // ---- D-08: labels axis broadcast (n×n / 1×n / n×1) ----
    const auto& l_labels = lhs_meta.labels;
    const auto& r_labels = rhs_meta.labels;
    const size_t ll = l_labels.size();
    const size_t rl = r_labels.size();
    std::vector<std::string> output_labels;
    if (ll == rl) {
        if (l_labels != r_labels) {
            throw std::runtime_error("Cannot apply binary operation: labels have same size " + std::to_string(ll) +
                                     " but different content");
        }
        output_labels = l_labels;
    } else if (ll == 1 && rl > 1) {
        output_labels = r_labels;
    } else if (rl == 1 && ll > 1) {
        output_labels = l_labels;
    } else {
        throw std::runtime_error("Cannot apply binary operation: labels have incompatible sizes " +
                                 std::to_string(ll) + " vs " + std::to_string(rl));
    }

    // ---- Build broadcast_meta_ (D-02 dim order: lhs.dims ++ rhs-only) ----
    // Sizes per same-name dim: max(lhs_size, rhs_size). Time properties carried over,
    // with parent_dimension_index recomputed to point at the new index (Pitfall 6).
    broadcast_meta_ = BinaryMetadata{};
    broadcast_meta_.version = lhs_meta.version;
    broadcast_meta_.unit = lhs_meta.unit;  // D-07 ensured equality
    broadcast_meta_.labels = output_labels;
    broadcast_meta_.initial_datetime = lhs_has_time
                                           ? lhs_meta.initial_datetime
                                           : (rhs_has_time ? rhs_meta.initial_datetime : lhs_meta.initial_datetime);

    // Map from dim name → final index in output, used to recompute parent_dimension_index.
    std::unordered_map<std::string, int> output_index_by_name;

    // Pass 1: lhs dims in lhs order
    for (const auto& l_dim : lhs_meta.dimensions) {
        int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
        int64_t out_size =
            (r_idx >= 0) ? std::max(l_dim.size, rhs_meta.dimensions[r_idx].size) : l_dim.size;
        Dimension d{l_dim.name, out_size, l_dim.time};  // copy time props from lhs (rhs equal per D-04)
        broadcast_meta_.dimensions.push_back(std::move(d));
        output_index_by_name[l_dim.name] = static_cast<int>(broadcast_meta_.dimensions.size()) - 1;
    }
    // Pass 2: rhs-only dims in rhs order
    for (const auto& r_dim : rhs_meta.dimensions) {
        if (output_index_by_name.count(r_dim.name))
            continue;  // already placed
        broadcast_meta_.dimensions.push_back(r_dim);
        output_index_by_name[r_dim.name] = static_cast<int>(broadcast_meta_.dimensions.size()) - 1;
    }
    // Recompute parent_dimension_index for time dims to point at new positions (Pitfall 6).
    // The parent dim was identified by INDEX in the source operand's array; we need to find
    // the parent BY NAME in the source operand and translate to the output index.
    for (size_t out_i = 0; out_i < broadcast_meta_.dimensions.size(); ++out_i) {
        auto& out_d = broadcast_meta_.dimensions[out_i];
        if (!out_d.is_time_dimension())
            continue;
        // Determine source: prefer lhs (Pass 1) if present; else rhs.
        int src_idx = find_dim_index(lhs_meta.dimensions, out_d.name);
        const auto* src_meta = &lhs_meta;
        if (src_idx < 0) {
            src_idx = find_dim_index(rhs_meta.dimensions, out_d.name);
            src_meta = &rhs_meta;
        }
        // src_idx is non-negative because out_d came from one of the operands.
        int64_t src_parent_idx = src_meta->dimensions[src_idx].time->parent_dimension_index;
        if (src_parent_idx < 0) {
            out_d.time->parent_dimension_index = -1;
            continue;
        }
        const std::string& parent_name = src_meta->dimensions[src_parent_idx].name;
        auto it = output_index_by_name.find(parent_name);
        // Parent must exist in output (parent dim is always declared before child dim in the source).
        out_d.time->parent_dimension_index = it->second;
    }
    // Compute number_of_time_dimensions
    for (const auto& d : broadcast_meta_.dimensions) {
        if (d.is_time_dimension())
            ++broadcast_meta_.number_of_time_dimensions;
    }

    // Defense-in-depth: validate the constructed metadata against the project's
    // existing invariants (positive sizes, consistent time-dim metadata, etc.).
    broadcast_meta_.validate();

    // ---- Pre-compute translation tables (D-05) and per-dim sizes for compute_row clamp ----
    const auto& out_dims = broadcast_meta_.dimensions;
    lhs_dim_translate_.resize(out_dims.size());
    rhs_dim_translate_.resize(out_dims.size());
    lhs_dim_sizes_.assign(out_dims.size(), 0);
    rhs_dim_sizes_.assign(out_dims.size(), 0);
    for (size_t i = 0; i < out_dims.size(); ++i) {
        int li = find_dim_index(lhs_meta.dimensions, out_dims[i].name);
        int ri = find_dim_index(rhs_meta.dimensions, out_dims[i].name);
        lhs_dim_translate_[i] = li;
        rhs_dim_translate_[i] = ri;
        lhs_dim_sizes_[i] = (li >= 0) ? lhs_meta.dimensions[li].size : 0;
        rhs_dim_sizes_[i] = (ri >= 0) ? rhs_meta.dimensions[ri].size : 0;
    }

    lhs_label_count_ = ll;
    rhs_label_count_ = rl;

    // Pre-size mutable buffers (avoid per-row allocation in compute_row — CORE-06).
    lhs_dims_buf_.resize(lhs_meta.dimensions.size());
    rhs_dims_buf_.resize(rhs_meta.dimensions.size());
    lhs_buf_.resize(lhs_label_count_);
    rhs_buf_.resize(rhs_label_count_);
}

BinaryMetadata BinaryOpNode::metadata() const {
    return broadcast_meta_;
}

void BinaryOpNode::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const size_t out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count)
        out.resize(out_label_count);

    // Translate output dims → lhs operand dims (D-05). For each lhs operand dim,
    // find its corresponding output index (search by name via the translation table),
    // then take dims[output_idx], with size-1 broadcast clamping.
    for (size_t li = 0; li < lhs_dims_buf_.size(); ++li) {
        int out_i = -1;
        for (size_t k = 0; k < lhs_dim_translate_.size(); ++k) {
            if (lhs_dim_translate_[k] == static_cast<int>(li)) {
                out_i = static_cast<int>(k);
                break;
            }
        }
        // out_i is guaranteed >= 0: every lhs dim appears in the output (Pass 1 of dim union).
        int64_t coord = dims[out_i];
        if (lhs_dim_sizes_[out_i] == 1)
            coord = 1;  // size-1 broadcast clamp
        lhs_dims_buf_[li] = coord;
    }
    // Same for rhs.
    for (size_t ri = 0; ri < rhs_dims_buf_.size(); ++ri) {
        int out_i = -1;
        for (size_t k = 0; k < rhs_dim_translate_.size(); ++k) {
            if (rhs_dim_translate_[k] == static_cast<int>(ri)) {
                out_i = static_cast<int>(k);
                break;
            }
        }
        int64_t coord = dims[out_i];
        if (rhs_dim_sizes_[out_i] == 1)
            coord = 1;
        rhs_dims_buf_[ri] = coord;
    }

    lhs_->compute_row(lhs_dims_buf_, lhs_buf_);
    rhs_->compute_row(rhs_dims_buf_, rhs_buf_);

    // D-08 label-axis broadcast: when one side has 1 label and the other has N, replicate.
    for (size_t k = 0; k < out_label_count; ++k) {
        const size_t li = (lhs_label_count_ == 1) ? 0 : k;
        const size_t ri = (rhs_label_count_ == 1) ? 0 : k;
        out[k] = apply_op(op_, lhs_buf_[li], rhs_buf_[ri]);
    }
}

}  // namespace quiver::expr
