#include "quiver/expression/expression_node.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace quiver {

ExpressionRenameAgents::ExpressionRenameAgents(std::shared_ptr<ExpressionNode> operand,
                                               std::vector<std::pair<std::string, std::string>> mapping)
    : operand_(std::move(operand)) {
    const auto& operand_meta = operand_->metadata();

    std::unordered_map<std::string, std::string> rename_map;
    std::unordered_map<std::string, bool> used;
    rename_map.reserve(mapping.size());
    used.reserve(mapping.size());
    for (auto& entry : mapping) {
        if (!rename_map.emplace(entry.first, std::move(entry.second)).second) {
            throw std::runtime_error("Cannot rename_agents: duplicate key '" + entry.first + "'");
        }
        used.emplace(entry.first, false);
    }

    output_meta_ = operand_meta;
    for (auto& label : output_meta_.labels) {
        auto it = rename_map.find(label);
        if (it != rename_map.end()) {
            label = it->second;
            used[it->first] = true;
        }
    }

    for (const auto& entry : used) {
        if (!entry.second) {
            throw std::runtime_error("Cannot rename_agents: label not found: '" + entry.first + "'");
        }
    }

    output_meta_.validate();
}

const BinaryMetadata& ExpressionRenameAgents::metadata() const {
    return output_meta_;
}

void ExpressionRenameAgents::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    operand_->compute_row(dims, out);
}

void ExpressionRenameAgents::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

}  // namespace quiver
