#ifndef QUIVER_ROW_H
#define QUIVER_ROW_H

#include "export.h"
#include "value.h"

#include <optional>
#include <vector>

namespace quiver {

class QUIVER_API Row {
public:
    explicit Row(std::vector<Value> values);

    const Value& operator[](size_t index) const;

    // Type-specific getters (return optionals for safe access)
    bool is_null(size_t index) const;
    std::optional<int64_t> get_integer(size_t index) const;
    std::optional<double> get_float(size_t index) const;
    std::optional<std::string> get_string(size_t index) const;

private:
    std::vector<Value> values_;
};

}  // namespace quiver

#endif  // QUIVER_ROW_H
