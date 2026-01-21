#ifndef MARGAUX_DATA_TYPE_H
#define MARGAUX_DATA_TYPE_H

#include <stdexcept>
#include <string>

namespace psr {

enum class DataType { Integer, Real, Text };

inline DataType data_type_from_string(const std::string& type_str) {
    if (type_str == "INTEGER")
        return DataType::Integer;
    else if (type_str == "REAL")
        return DataType::Real;
    else if (type_str == "TEXT")
        return DataType::Text;
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
    }
    return "UNKNOWN";
}

}  // namespace psr

#endif  // MARGAUX_DATA_TYPE_H
