#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

std::optional<std::string> Database::query_string(const std::string& sql, const std::vector<Value>& parameters) {
    return internal::read_single_value<std::string>(impl_->execute(sql, parameters));
}

std::optional<int64_t> Database::query_integer(const std::string& sql, const std::vector<Value>& parameters) {
    return internal::read_single_value<int64_t>(impl_->execute(sql, parameters));
}

std::optional<double> Database::query_float(const std::string& sql, const std::vector<Value>& parameters) {
    return internal::read_single_value<double>(impl_->execute(sql, parameters));
}

}  // namespace quiver
