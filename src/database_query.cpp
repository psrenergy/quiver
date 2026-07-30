#include "database_impl.h"

namespace quiver {

std::optional<std::string> Database::query_string(const std::string& sql, const std::vector<Value>& parameters) {
    auto result = execute(sql, parameters);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_string(0);
}

std::optional<int64_t> Database::query_integer(const std::string& sql, const std::vector<Value>& parameters) {
    auto result = execute(sql, parameters);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_integer(0);
}

std::optional<double> Database::query_float(const std::string& sql, const std::vector<Value>& parameters) {
    auto result = execute(sql, parameters);
    if (result.empty()) {
        return std::nullopt;
    }
    if (auto value = result[0].get_float(0)) {
        return value;
    }
    // Widen an INTEGER result, matching the one scalar typing policy (an int64 is accepted
    // wherever a REAL is expected). Without this, COUNT(*)/SUM(int_col) - which SQLite returns
    // as INTEGER - would silently read back as "no value".
    if (auto value = result[0].get_integer(0)) {
        return static_cast<double>(*value);
    }
    return std::nullopt;
}

}  // namespace quiver
