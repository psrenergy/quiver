#ifndef QUIVER_RESULT_H
#define QUIVER_RESULT_H

#include "row.h"

#include <string>
#include <utility>
#include <vector>

namespace quiver {

class Result {
public:
    Result(std::vector<std::string> columns, std::vector<Row> rows)
        : columns_(std::move(columns)), rows_(std::move(rows)) {}

    const std::vector<std::string>& columns() const {
        return columns_;
    }
    size_t row_count() const {
        return rows_.size();
    }
    bool empty() const {
        return rows_.empty();
    }

    const Row& operator[](size_t index) const {
        return rows_[index];
    }

    // Iterator support
    auto begin() const {
        return rows_.begin();
    }
    auto end() const {
        return rows_.end();
    }

private:
    std::vector<std::string> columns_;
    std::vector<Row> rows_;
};

}  // namespace quiver

#endif  // QUIVER_RESULT_H
