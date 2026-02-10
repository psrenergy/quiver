#include "quiver/enum_map.h"

#include <stdexcept>

namespace quiver {

EnumMap::EnumMap(Data data) : data_(std::move(data)) {}

bool EnumMap::contains(const std::string& column) const {
    return data_.contains(column);
}

std::string EnumMap::get_first_locale() const {
    if (data_.empty())
        throw std::runtime_error("EnumMap is empty");

    const auto& locales = data_.begin()->second;
    if (locales.empty())
        throw std::runtime_error("EnumMap column has no locales");

    return locales.begin()->first;
}

std::optional<std::string>
EnumMap::get_enum_label(const std::string& column, int64_t id, const std::string& locale) const {
    auto col_it = data_.find(column);
    if (col_it == data_.end())
        return std::nullopt;

    auto loc_it = col_it->second.find(locale);
    if (loc_it == col_it->second.end())
        return std::nullopt;

    for (const auto& [label, label_id] : loc_it->second) {
        if (label_id == id)
            return label;
    }
    return std::nullopt;
}

std::optional<int64_t>
EnumMap::get_enum_id(const std::string& column, const std::string& locale, const std::string& label) const {
    auto col_it = data_.find(column);
    if (col_it == data_.end())
        return std::nullopt;

    auto loc_it = col_it->second.find(locale);
    if (loc_it == col_it->second.end())
        return std::nullopt;

    auto label_it = loc_it->second.find(label);
    if (label_it == loc_it->second.end())
        return std::nullopt;

    return label_it->second;
}

std::optional<int64_t> EnumMap::find_enum_id(const std::string& column, const std::string& label) const {
    auto col_it = data_.find(column);
    if (col_it == data_.end())
        return std::nullopt;

    for (const auto& [locale, labels] : col_it->second) {
        auto label_it = labels.find(label);
        if (label_it != labels.end())
            return label_it->second;
    }
    return std::nullopt;
}

bool EnumMap::empty() const {
    return data_.empty();
}

}  // namespace quiver
