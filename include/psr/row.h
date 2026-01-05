#ifndef PSR_DATABASE_ROW_H
#define PSR_DATABASE_ROW_H

#include "export.h"
#include "value.h"

#include <optional>
#include <vector>

namespace psr {

class PSR_API Row {
public:
    explicit Row(std::vector<Value> values);

    std::size_t size() const;
    std::size_t column_count() const { return size(); }
    bool empty() const;

    const Value& at(std::size_t index) const;
    const Value& operator[](std::size_t index) const;

    // Type-specific getters (return optionals for safe access)
    bool is_null(std::size_t index) const;
    std::optional<int64_t> get_int(std::size_t index) const;
    std::optional<double> get_double(std::size_t index) const;
    std::optional<std::string> get_string(std::size_t index) const;
    std::optional<std::vector<uint8_t>> get_blob(std::size_t index) const;

    // Iterator support
    auto begin() const { return values_.begin(); }
    auto end() const { return values_.end(); }

private:
    std::vector<Value> values_;
};

}  // namespace psr

#endif  // PSR_DATABASE_ROW_H
