#include "margaux/element.h"

#include <sstream>

namespace margaux {

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
            } else {
                return "<unknown>";
            }
        },
        value);
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

Element& Element::set(const std::string& name, const std::vector<int64_t>& values) {
    std::vector<Value> arr;
    arr.reserve(values.size());
    for (const auto& v : values) {
        arr.emplace_back(v);
    }
    arrays_[name] = std::move(arr);
    return *this;
}

Element& Element::set(const std::string& name, const std::vector<double>& values) {
    std::vector<Value> arr;
    arr.reserve(values.size());
    for (const auto& v : values) {
        arr.emplace_back(v);
    }
    arrays_[name] = std::move(arr);
    return *this;
}

Element& Element::set(const std::string& name, const std::vector<std::string>& values) {
    std::vector<Value> arr;
    arr.reserve(values.size());
    for (const auto& v : values) {
        arr.emplace_back(v);
    }
    arrays_[name] = std::move(arr);
    return *this;
}

const std::map<std::string, Value>& Element::scalars() const {
    return scalars_;
}

const std::map<std::string, std::vector<Value>>& Element::arrays() const {
    return arrays_;
}

bool Element::has_scalars() const {
    return !scalars_.empty();
}

bool Element::has_arrays() const {
    return !arrays_.empty();
}

void Element::clear() {
    scalars_.clear();
    arrays_.clear();
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

    if (!arrays_.empty()) {
        oss << "  arrays:\n";
        for (const auto& [name, values] : arrays_) {
            oss << "    " << name << ": [";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0)
                    oss << ", ";
                oss << value_to_string(values[i]);
            }
            oss << "]\n";
        }
    }

    oss << "}";
    return oss.str();
}

}  // namespace margaux
