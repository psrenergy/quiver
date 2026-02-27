#ifndef QUIVER_DATA_TYPE_H
#define QUIVER_DATA_TYPE_H

#include <stdexcept>
#include <string>

namespace quiver {

enum class DataType { Integer, Real, Text, DateTime };

inline DataType data_type_from_string(const std::string& type_str) {
    if (type_str == "INTEGER")
        return DataType::Integer;
    else if (type_str == "REAL")
        return DataType::Real;
    else if (type_str == "TEXT")
        return DataType::Text;
    else if (type_str == "DATE_TIME")
        return DataType::DateTime;
    throw std::runtime_error("Unknown data type: " + type_str);
}

inline const char* data_type_to_string(DataType type) {
    switch (type) {
    case DataType::Integer:
        return "INTEGER";
    case DataType::Real:
        return "REAL";
    case DataType::Text:
        return "TEXT";
    case DataType::DateTime:
        return "DATE_TIME";
    default:
        throw std::runtime_error("Cannot data_type_to_string: unknown data type " + std::to_string(static_cast<int>(type)));
    }
}

// Check if a column name indicates a date time column
// Columns beginning with "date_" are treated as DATE_TIME
inline bool is_date_time_column(const std::string& name) {
    return name.starts_with("date_");
}

}  // namespace quiver

#endif  // QUIVER_DATA_TYPE_H
