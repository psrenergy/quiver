#include "psr/element.h"

#include <sstream>

namespace psr {

namespace {

std::string value_to_string(const Value& value) {
    return std::visit(
        [](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return "null";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(arg);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "\"" + arg + "\"";
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                return "<blob:" + std::to_string(arg.size()) + " bytes>";
            } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                std::string result = "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += std::to_string(arg[i]);
                }
                return result + "]";
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                std::string result = "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += std::to_string(arg[i]);
                }
                return result + "]";
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                std::string result = "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += "\"" + arg[i] + "\"";
                }
                return result + "]";
            } else {
                return "<unknown>";
            }
        },
        value);
}

std::string row_to_string(const VectorRow& row) {
    std::string result = "{";
    bool first = true;
    for (const auto& [name, value] : row) {
        if (!first)
            result += ", ";
        result += name + ": " + value_to_string(value);
        first = false;
    }
    return result + "}";
}

}  // namespace

Element& Element::set(const std::string& name, int64_t value) {
    scalars_[name] = value;
    return *this;
}

Element& Element::set(const std::string& name, double value) {
    scalars_[name] = value;
    return *this;
}

Element& Element::set(const std::string& name, const std::string& value) {
    scalars_[name] = value;
    return *this;
}

Element& Element::set_null(const std::string& name) {
    scalars_[name] = nullptr;
    return *this;
}

Element& Element::add_vector_group(const std::string& group_name, VectorGroup rows) {
    vector_groups_[group_name] = std::move(rows);
    return *this;
}

Element& Element::add_set_group(const std::string& group_name, SetGroup rows) {
    set_groups_[group_name] = std::move(rows);
    return *this;
}

const std::map<std::string, Value>& Element::scalars() const {
    return scalars_;
}

const std::map<std::string, VectorGroup>& Element::vector_groups() const {
    return vector_groups_;
}

const std::map<std::string, SetGroup>& Element::set_groups() const {
    return set_groups_;
}

bool Element::has_scalars() const {
    return !scalars_.empty();
}

bool Element::has_vector_groups() const {
    return !vector_groups_.empty();
}

bool Element::has_set_groups() const {
    return !set_groups_.empty();
}

void Element::clear() {
    scalars_.clear();
    vector_groups_.clear();
    set_groups_.clear();
}

std::string Element::to_string() const {
    std::ostringstream oss;
    oss << "Element {\n";

    if (!scalars_.empty()) {
        oss << "  scalars:\n";
        for (const auto& [name, value] : scalars_) {
            oss << "    " << name << ": " << value_to_string(value) << "\n";
        }
    }

    if (!vector_groups_.empty()) {
        oss << "  vector_groups:\n";
        for (const auto& [name, rows] : vector_groups_) {
            oss << "    " << name << ": [\n";
            for (size_t i = 0; i < rows.size(); ++i) {
                oss << "      [" << i << "] " << row_to_string(rows[i]) << "\n";
            }
            oss << "    ]\n";
        }
    }

    if (!set_groups_.empty()) {
        oss << "  set_groups:\n";
        for (const auto& [name, rows] : set_groups_) {
            oss << "    " << name << ": [\n";
            for (const auto& row : rows) {
                oss << "      " << row_to_string(row) << "\n";
            }
            oss << "    ]\n";
        }
    }

    oss << "}";
    return oss.str();
}

}  // namespace psr
