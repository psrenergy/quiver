#ifndef MARGAUX_ROW_H
#define MARGAUX_ROW_H

#include "export.h"
#include "value.h"

#include <optional>
#include <vector>

namespace margaux {

class MARGAUX_API Row {
public:
    explicit Row(std::vector<Value> values);

    size_t size() const;
    size_t column_count() const { return size(); }
    bool empty() const;

    const Value& at(size_t index) const;
    const Value& operator[](size_t index) const;

    // Type-specific getters (return optionals for safe access)
    bool is_null(size_t index) const;
    std::optional<int64_t> get_integer(size_t index) const;
    std::optional<double> get_float(size_t index) const;
    std::optional<std::string> get_string(size_t index) const;

    // Iterator support
    auto begin() const { return values_.begin(); }
    auto end() const { return values_.end(); }

private:
    std::vector<Value> values_;
};

}  // namespace margaux

#endif  // MARGAUX_ROW_H
