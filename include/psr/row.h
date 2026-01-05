#ifndef PSR_DATABASE_ROW_H
#define PSR_DATABASE_ROW_H

#include "export.h"

#include "value.h"
#include <vector>
#include <optional>

namespace psr {

class PSR_API Row {
public:
    explicit Row(std::vector<Value> values);

    size_t size() const;
    size_t column_count() const { return size(); }
    bool empty() const;

    const Value& at(size_t index) const;
    const Value& operator[](size_t index) const;

    // Type-specific getters (return optionals for safe access)
    bool is_null(size_t index) const;
    std::optional<int64_t> get_int(size_t index) const;
    std::optional<double> get_double(size_t index) const;
    std::optional<std::string> get_string(size_t index) const;
    std::optional<std::vector<uint8_t>> get_blob(size_t index) const;

    // Iterator support
    auto begin() const { return values_.begin(); }
    auto end() const { return values_.end(); }

private:
    std::vector<Value> values_;
};

}  // namespace psr

#endif  // PSR_DATABASE_VALUE_H
