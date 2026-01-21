#include "psr/result.h"

#include "psr/row.h"

namespace margaux {

Result::Result() : columns_(), rows_() {}

Result::Result(std::vector<std::string> columns, std::vector<Row> rows)
    : columns_(std::move(columns)), rows_(std::move(rows)) {}

const std::vector<std::string>& Result::columns() const {
    return columns_;
}

size_t Result::column_count() const {
    return columns_.size();
}

size_t Result::row_count() const {
    return rows_.size();
}

bool Result::empty() const {
    return rows_.empty();
}

const Row& Result::at(size_t index) const {
    return rows_.at(index);
}

const Row& Result::operator[](size_t index) const {
    return rows_[index];
}
}  // namespace margaux