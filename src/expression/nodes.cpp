#include "quiver/binary/binary_file.h"
#include "quiver/expression/node.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

FileNode::FileNode(const BinaryFile& file) : path_(file.get_file_path()), meta_(file.get_metadata()) {}

BinaryMetadata FileNode::metadata() const {
    return meta_;
}

void FileNode::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    if (!file_) {
        file_ = std::make_unique<BinaryFile>(BinaryFile::open_file(path_, 'r'));
    }
    std::unordered_map<std::string, int64_t> dim_map;
    dim_map.reserve(meta_.dimensions.size());
    for (size_t i = 0; i < meta_.dimensions.size(); ++i) {
        dim_map[meta_.dimensions[i].name] = dims[i];
    }
    out = file_->read(dim_map, /*allow_nulls=*/true);
}

ScalarNode::ScalarNode(double value, BinaryMetadata broadcast_meta)
    : value_(value), broadcast_meta_(std::move(broadcast_meta)) {}

BinaryMetadata ScalarNode::metadata() const {
    return broadcast_meta_;
}

void ScalarNode::compute_row(const std::vector<int64_t>& /*dims*/, std::vector<double>& out) const {
    out.assign(broadcast_meta_.labels.size(), value_);
}

namespace {

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
    throw std::runtime_error("Cannot apply: unhandled BinaryOp variant");
}

}  // namespace

BinaryOpNode::BinaryOpNode(BinaryOp op, std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
    : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {
    const auto lhs_meta = lhs_->metadata();
    const auto rhs_meta = rhs_->metadata();

    if (lhs_meta.unit != rhs_meta.unit) {
        throw std::runtime_error("Cannot apply: units differ ('" + lhs_meta.unit + "' vs '" + rhs_meta.unit + "')");
    }

    for (const auto& l_dim : lhs_meta.dimensions) {
        int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
        if (r_idx < 0)
            continue;
        int64_t l_size = l_dim.size;
        int64_t r_size = rhs_meta.dimensions[r_idx].size;
        if (l_size == r_size)
            continue;
        if (l_size == 1 || r_size == 1)
            continue;
        throw std::runtime_error("Cannot apply: dimension '" + l_dim.name + "' has incompatible sizes " +
                                 std::to_string(l_size) + " vs " + std::to_string(r_size) +
                                 " (broadcasting requires n×n, 1×n, or n×1)");
    }

    auto parent_name_of = [](int64_t parent_idx, const BinaryMetadata& m) -> std::string {
        return (parent_idx >= 0) ? m.dimensions[parent_idx].name : std::string{};
    };
    for (const auto& l_dim : lhs_meta.dimensions) {
        int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
        if (r_idx < 0)
            continue;
        const auto& r_dim = rhs_meta.dimensions[r_idx];
        const auto l_time = l_dim.is_time_dimension();
        const auto r_time = r_dim.is_time_dimension();
        if (l_time != r_time) {
            const std::string time_side = l_time ? "lhs" : "rhs";
            const std::string nontime_side = l_time ? "rhs" : "lhs";
            throw std::runtime_error("Cannot apply: dimension '" + l_dim.name + "' is a time dimension on " +
                                     time_side + " but not on " + nontime_side);
        }
        if (!l_time)
            continue;
        const auto& lp = *l_dim.time;
        const auto& rp = *r_dim.time;
        const auto l_parent = parent_name_of(lp.parent_dimension_index, lhs_meta);
        const auto r_parent = parent_name_of(rp.parent_dimension_index, rhs_meta);
        if (lp.frequency != rp.frequency || lp.initial_value != rp.initial_value || l_parent != r_parent) {
            throw std::runtime_error("Cannot apply: time dimension '" + l_dim.name +
                                     "' has incompatible TimeProperties");
        }
    }

    const bool lhs_has_time = any_time_dim(lhs_meta.dimensions);
    const bool rhs_has_time = any_time_dim(rhs_meta.dimensions);
    if (lhs_has_time && rhs_has_time && lhs_meta.initial_datetime != rhs_meta.initial_datetime) {
        throw std::runtime_error("Cannot apply: initial_datetime differs");
    }

    const auto& l_labels = lhs_meta.labels;
    const auto& r_labels = rhs_meta.labels;
    const auto ll = l_labels.size();
    const auto rl = r_labels.size();
    std::vector<std::string> output_labels;
    if (ll == rl) {
        if (l_labels != r_labels) {
            throw std::runtime_error("Cannot apply: labels have same size " + std::to_string(ll) +
                                     " but different content");
        }
        output_labels = l_labels;
    } else if (ll == 1 && rl > 1) {
        output_labels = r_labels;
    } else if (rl == 1 && ll > 1) {
        output_labels = l_labels;
    } else {
        throw std::runtime_error("Cannot apply: labels have incompatible sizes " + std::to_string(ll) + " vs " +
                                 std::to_string(rl));
    }

    broadcast_meta_ = BinaryMetadata{};
    broadcast_meta_.version = lhs_meta.version;
    broadcast_meta_.unit = lhs_meta.unit;
    broadcast_meta_.labels = output_labels;
    broadcast_meta_.initial_datetime = lhs_has_time
                                           ? lhs_meta.initial_datetime
                                           : (rhs_has_time ? rhs_meta.initial_datetime : lhs_meta.initial_datetime);

    std::unordered_map<std::string, int> output_index_by_name;

    for (const auto& l_dim : lhs_meta.dimensions) {
        int r_idx = find_dim_index(rhs_meta.dimensions, l_dim.name);
        int64_t out_size = (r_idx >= 0) ? std::max(l_dim.size, rhs_meta.dimensions[r_idx].size) : l_dim.size;
        Dimension d{l_dim.name, out_size, l_dim.time};  // copy time props from lhs (rhs equal)
        broadcast_meta_.dimensions.push_back(std::move(d));
        output_index_by_name[l_dim.name] = static_cast<int>(broadcast_meta_.dimensions.size()) - 1;
    }
    for (const auto& r_dim : rhs_meta.dimensions) {
        if (output_index_by_name.count(r_dim.name))
            continue;  // already placed
        broadcast_meta_.dimensions.push_back(r_dim);
        output_index_by_name[r_dim.name] = static_cast<int>(broadcast_meta_.dimensions.size()) - 1;
    }
    for (size_t out_i = 0; out_i < broadcast_meta_.dimensions.size(); ++out_i) {
        auto& out_d = broadcast_meta_.dimensions[out_i];
        if (!out_d.is_time_dimension())
            continue;
        auto src_idx = find_dim_index(lhs_meta.dimensions, out_d.name);
        const auto* src_meta = &lhs_meta;
        if (src_idx < 0) {
            src_idx = find_dim_index(rhs_meta.dimensions, out_d.name);
            src_meta = &rhs_meta;
        }
        int64_t src_parent_idx = src_meta->dimensions[src_idx].time->parent_dimension_index;
        if (src_parent_idx < 0) {
            out_d.time->parent_dimension_index = -1;
            continue;
        }
        const std::string& parent_name = src_meta->dimensions[src_parent_idx].name;
        auto it = output_index_by_name.find(parent_name);
        out_d.time->parent_dimension_index = it->second;
    }
    for (const auto& d : broadcast_meta_.dimensions) {
        if (d.is_time_dimension())
            ++broadcast_meta_.number_of_time_dimensions;
    }

    broadcast_meta_.validate();

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

    lhs_to_out_.assign(lhs_meta.dimensions.size(), -1);
    rhs_to_out_.assign(rhs_meta.dimensions.size(), -1);
    for (size_t out_i = 0; out_i < out_dims.size(); ++out_i) {
        if (lhs_dim_translate_[out_i] >= 0)
            lhs_to_out_[lhs_dim_translate_[out_i]] = static_cast<int>(out_i);
        if (rhs_dim_translate_[out_i] >= 0)
            rhs_to_out_[rhs_dim_translate_[out_i]] = static_cast<int>(out_i);
    }

    lhs_label_count_ = ll;
    rhs_label_count_ = rl;

    lhs_dims_buf_.resize(lhs_meta.dimensions.size());
    rhs_dims_buf_.resize(rhs_meta.dimensions.size());
    lhs_buf_.resize(lhs_label_count_);
    rhs_buf_.resize(rhs_label_count_);
}

BinaryMetadata BinaryOpNode::metadata() const {
    return broadcast_meta_;
}

void BinaryOpNode::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count)
        out.resize(out_label_count);

    for (size_t li = 0; li < lhs_dims_buf_.size(); ++li) {
        const int out_i = lhs_to_out_[li];
        int64_t coord = dims[out_i];
        if (lhs_dim_sizes_[out_i] == 1)
            coord = 1;  // size-1 broadcast clamp
        lhs_dims_buf_[li] = coord;
    }
    for (size_t ri = 0; ri < rhs_dims_buf_.size(); ++ri) {
        const int out_i = rhs_to_out_[ri];
        int64_t coord = dims[out_i];
        if (rhs_dim_sizes_[out_i] == 1)
            coord = 1;
        rhs_dims_buf_[ri] = coord;
    }

    lhs_->compute_row(lhs_dims_buf_, lhs_buf_);
    rhs_->compute_row(rhs_dims_buf_, rhs_buf_);

    for (size_t k = 0; k < out_label_count; ++k) {
        const size_t li = (lhs_label_count_ == 1) ? 0 : k;
        const size_t ri = (rhs_label_count_ == 1) ? 0 : k;
        out[k] = apply_op(op_, lhs_buf_[li], rhs_buf_[ri]);
    }
}

}  // namespace quiver
