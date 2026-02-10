#include "database_impl.h"

namespace quiver {

std::optional<std::string> Database::query_string(const std::string& sql, const std::vector<Value>& params) {
    auto result = execute(sql, params);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_string(0);
}

std::optional<int64_t> Database::query_integer(const std::string& sql, const std::vector<Value>& params) {
    auto result = execute(sql, params);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_integer(0);
}

std::optional<double> Database::query_float(const std::string& sql, const std::vector<Value>& params) {
    auto result = execute(sql, params);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_float(0);
}

}  // namespace quiver
