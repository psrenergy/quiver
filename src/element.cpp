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
                    if (i > 0) result += ", ";
                    result += std::to_string(arg[i]);
                }
                return result + "]";
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                std::string result = "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += std::to_string(arg[i]);
                }
                return result + "]";
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                std::string result = "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += "\"" + arg[i] + "\"";
                }
                return result + "]";
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

Element& Element::set_vector(const std::string& name, std::vector<int64_t> values) {
    vectors_[name] = std::move(values);
    return *this;
}

Element& Element::set_vector(const std::string& name, std::vector<double> values) {
    vectors_[name] = std::move(values);
    return *this;
}

Element& Element::set_vector(const std::string& name, std::vector<std::string> values) {
    vectors_[name] = std::move(values);
    return *this;
}

const std::map<std::string, Value>& Element::scalars() const {
    return scalars_;
}

const std::map<std::string, Value>& Element::vectors() const {
    return vectors_;
}

bool Element::has_scalars() const {
    return !scalars_.empty();
}

bool Element::has_vectors() const {
    return !vectors_.empty();
}

void Element::clear() {
    scalars_.clear();
    vectors_.clear();
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

    if (!vectors_.empty()) {
        oss << "  vectors:\n";
        for (const auto& [name, value] : vectors_) {
            oss << "    " << name << ": " << value_to_string(value) << "\n";
        }
    }

    oss << "}";
    return oss.str();
}

}  // namespace psr
