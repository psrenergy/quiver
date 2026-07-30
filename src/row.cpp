#include "quiver/row.h"

#include "quiver/value.h"

namespace quiver {

Row::Row(std::vector<Value> values) : values_(std::move(values)) {}

size_t Row::size() const {
    return values_.size();
}

bool Row::empty() const {
    return values_.empty();
}

const Value& Row::at(size_t index) const {
    return values_.at(index);
}

const Value& Row::operator[](size_t index) const {
    return values_[index];
}

bool Row::is_null(size_t index) const {
    return std::holds_alternative<std::nullptr_t>(values_[index]);
}

std::optional<int64_t> Row::get_integer(size_t index) const {
    if (const auto* val = std::get_if<int64_t>(&values_[index])) {
        return *val;
    }
    return std::nullopt;
}

std::optional<double> Row::get_float(size_t index) const {
    if (const auto* val = std::get_if<double>(&values_[index])) {
        return *val;
    }
    // An int64 is accepted wherever a REAL is expected (the one scalar typing policy), and this is
    // the single extractor behind every float read: query_float, the bulk/by-id scalar, vector and
    // set readers. SQLite answers COUNT(*)/SUM(int_col) as INTEGER, and an integer stored in a REAL
    // column stays INTEGER, so without this they all read back as "no value".
    if (const auto* val = std::get_if<int64_t>(&values_[index])) {
        return static_cast<double>(*val);
    }
    return std::nullopt;
}

std::optional<std::string> Row::get_string(size_t index) const {
    if (const auto* val = std::get_if<std::string>(&values_[index])) {
        return *val;
    }
    return std::nullopt;
}

}  // namespace quiver