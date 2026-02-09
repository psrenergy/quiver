#ifndef QUIVER_ENUM_MAP_H
#define QUIVER_ENUM_MAP_H

#include "export.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace quiver {

class QUIVER_API EnumMap {
public:
    // Internal storage: column -> locale -> {label -> id}
    using Data = std::map<std::string, std::map<std::string, std::map<std::string, int64_t>>>;

    EnumMap() = default;
    explicit EnumMap(Data data);

    bool contains(const std::string& column) const;
    std::string get_first_locale() const;
    std::optional<std::string> get_enum_label(const std::string& column, int64_t id, const std::string& locale) const;
    std::optional<int64_t>
    get_enum_id(const std::string& column, const std::string& locale, const std::string& label) const;
    std::optional<int64_t> find_enum_id(const std::string& column, const std::string& label) const;
    bool empty() const;

private:
    Data data_;
};

}  // namespace quiver

#endif  // QUIVER_ENUM_MAP_H
