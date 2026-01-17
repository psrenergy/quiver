#ifndef PSR_DATABASE_TEMPLATES_H
#define PSR_DATABASE_TEMPLATES_H

#include "psr/result.h"
#include "psr/value.h"

#include <optional>
#include <vector>

namespace psr {
namespace detail {

// Type traits for extracting values from Row
template <typename T>
struct RowExtractor;

template <>
struct RowExtractor<int64_t> {
    static std::optional<int64_t> extract(const Row& row, size_t index) {
        return row.get_int(index);
    }
};

template <>
struct RowExtractor<double> {
    static std::optional<double> extract(const Row& row, size_t index) {
        return row.get_double(index);
    }
};

template <>
struct RowExtractor<std::string> {
    static std::optional<std::string> extract(const Row& row, size_t index) {
        return row.get_string(index);
    }
};

// Generic scalar reader
template <typename T>
std::vector<T> read_scalar_generic(const Result& result) {
    std::vector<T> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = RowExtractor<T>::extract(result[i], 0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

// Generic scalar by ID reader
template <typename T>
std::optional<T> read_scalar_by_id_generic(const Result& result) {
    if (result.empty()) {
        return std::nullopt;
    }
    return RowExtractor<T>::extract(result[0], 0);
}

// Generic vector reader
template <typename T>
std::vector<std::vector<T>> read_vector_generic(const Result& result) {
    std::vector<std::vector<T>> vectors;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_int(0);
        auto val = RowExtractor<T>::extract(result[i], 1);

        if (!id)
            continue;

        if (*id != current_id) {
            vectors.emplace_back();
            current_id = *id;
        }

        if (val) {
            vectors.back().push_back(*val);
        }
    }
    return vectors;
}

// Generic vector by ID reader
template <typename T>
std::vector<T> read_vector_by_id_generic(const Result& result) {
    std::vector<T> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = RowExtractor<T>::extract(result[i], 0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

// Generic set reader (same as vector reader)
template <typename T>
std::vector<std::vector<T>> read_set_generic(const Result& result) {
    return read_vector_generic<T>(result);
}

// Generic set by ID reader (same as vector by ID reader)
template <typename T>
std::vector<T> read_set_by_id_generic(const Result& result) {
    return read_vector_by_id_generic<T>(result);
}

}  // namespace detail
}  // namespace psr

#endif  // PSR_DATABASE_TEMPLATES_H
