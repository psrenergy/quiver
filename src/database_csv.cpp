#include "database_impl.h"
#include "quiver/csv.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace quiver {

// RFC 4180 field escaping: if field contains comma, double-quote, newline,
// or carriage return, wrap in double quotes and double any internal quotes.
static std::string csv_escape(const std::string& field) {
    bool needs_quoting = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quoting = true;
            break;
        }
    }
    if (!needs_quoting) {
        return field;
    }
    std::string escaped = "\"";
    for (char c : field) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped += c;
        }
    }
    escaped += "\"";
    return escaped;
}

// Cross-platform ISO 8601 parser using std::get_time (not strptime).
// Tries "YYYY-MM-DDTHH:MM:SS" first, then "YYYY-MM-DD HH:MM:SS".
static bool parse_iso8601(const std::string& datetime_str, std::tm& tm) {
    std::memset(&tm, 0, sizeof(tm));
    std::istringstream ss(datetime_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (!ss.fail()) {
        return true;
    }
    ss.clear();
    ss.str(datetime_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return !ss.fail();
}

// Format a datetime value using strftime. Returns raw_value if parsing fails.
static std::string format_datetime(const std::string& raw_value, const std::string& format) {
    std::tm tm{};
    if (!parse_iso8601(raw_value, tm)) {
        return raw_value;
    }
    char buffer[256];
    if (std::strftime(buffer, sizeof(buffer), format.c_str(), &tm) == 0) {
        return raw_value;
    }
    return std::string(buffer);
}

// Convert a Value to its CSV string representation.
// NULL -> empty string (unquoted), integers check enum_labels,
// floats use %g for clean output, strings apply DateTime formatting.
static std::string value_to_csv_string(const Value& value,
                                       const std::string& column_name,
                                       DataType data_type,
                                       const CSVExportOptions& options) {
    // NULL -> empty field
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return "";
    }

    // Integer with possible enum resolution
    if (std::holds_alternative<int64_t>(value)) {
        auto int_val = std::get<int64_t>(value);
        if (auto attr_it = options.enum_labels.find(column_name); attr_it != options.enum_labels.end()) {
            if (auto val_it = attr_it->second.find(int_val); val_it != attr_it->second.end()) {
                return csv_escape(val_it->second);
            }
        }
        return std::to_string(int_val);
    }

    // Float: use snprintf with %g for clean output (no trailing zeros)
    if (std::holds_alternative<double>(value)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", std::get<double>(value));
        return std::string(buf);
    }

    // String (may be DateTime)
    if (std::holds_alternative<std::string>(value)) {
        const auto& str_val = std::get<std::string>(value);
        if (data_type == DataType::DateTime && !options.date_time_format.empty()) {
            return csv_escape(format_datetime(str_val, options.date_time_format));
        }
        return csv_escape(str_val);
    }

    return "";
}

// Join fields with commas and append LF.
static void write_csv_row(std::ofstream& file, const std::vector<std::string>& fields) {
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            file << ',';
        }
        file << fields[i];
    }
    file << '\n';
}

void Database::export_csv(const std::string& collection,
                          const std::string& group,
                          const std::string& path,
                          const CSVExportOptions& options) {
    namespace fs = std::filesystem;

    // Create parent directories (mkdir -p)
    auto parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }

    // Open in binary mode to prevent Windows text-mode CRLF conversion
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to export_csv: could not open file: " + path);
    }

    if (group.empty()) {
        // Scalar export
        impl_->require_collection(collection, "export_csv");

        // Get columns in schema definition order via SELECT * LIMIT 0
        auto schema_result = execute("SELECT * FROM " + collection + " LIMIT 0");
        const auto& all_columns = schema_result.columns();

        // Filter out "id", keep remaining columns in schema order
        std::vector<std::string> csv_columns;
        for (const auto& col : all_columns) {
            if (col != "id") {
                csv_columns.push_back(col);
            }
        }

        // Build DataType map from scalar metadata for DateTime detection and enum resolution
        auto scalar_attrs = list_scalar_attributes(collection);
        std::unordered_map<std::string, DataType> type_map;
        for (const auto& attr : scalar_attrs) {
            type_map[attr.name] = attr.data_type;
        }

        // Write header row
        std::vector<std::string> header_fields;
        header_fields.reserve(csv_columns.size());
        for (const auto& col : csv_columns) {
            header_fields.push_back(csv_escape(col));
        }
        write_csv_row(file, header_fields);

        // Build SELECT query with columns in schema order
        std::string select_cols;
        for (size_t i = 0; i < csv_columns.size(); ++i) {
            if (i > 0)
                select_cols += ", ";
            select_cols += csv_columns[i];
        }

        auto data_result = execute("SELECT " + select_cols + " FROM " + collection + " ORDER BY rowid");

        // Write data rows
        for (const auto& row : data_result) {
            std::vector<std::string> fields;
            fields.reserve(csv_columns.size());
            for (size_t i = 0; i < csv_columns.size(); ++i) {
                DataType dt = DataType::Text;
                if (auto it = type_map.find(csv_columns[i]); it != type_map.end()) {
                    dt = it->second;
                }
                fields.push_back(value_to_csv_string(row[i], csv_columns[i], dt, options));
            }
            write_csv_row(file, fields);
        }
    } else {
        // Group export
        impl_->require_collection(collection, "export_csv");

        // Determine group type by checking schema for matching table names
        std::string table_name;
        enum class GroupType { Vector, Set, TimeSeries };
        GroupType group_type{};

        std::string vec_table = Schema::vector_table_name(collection, group);
        std::string set_table = Schema::set_table_name(collection, group);
        std::string ts_table = Schema::time_series_table_name(collection, group);

        if (impl_->schema->has_table(vec_table)) {
            table_name = vec_table;
            group_type = GroupType::Vector;
        } else if (impl_->schema->has_table(set_table)) {
            table_name = set_table;
            group_type = GroupType::Set;
        } else if (impl_->schema->has_table(ts_table)) {
            table_name = ts_table;
            group_type = GroupType::TimeSeries;
        } else {
            throw std::runtime_error("Cannot export_csv: group not found: '" + group + "' in collection '" +
                                     collection + "'");
        }

        // Get group table columns in schema definition order
        auto schema_result = execute("SELECT * FROM " + table_name + " LIMIT 0");
        const auto& all_group_columns = schema_result.columns();

        // Determine which columns to include (skip id, skip vector_index for vectors)
        std::vector<std::string> group_data_columns;
        for (const auto& col : all_group_columns) {
            if (col == "id")
                continue;
            if (group_type == GroupType::Vector && col == "vector_index")
                continue;
            group_data_columns.push_back(col);
        }

        // CSV headers: label first, then group data columns
        std::vector<std::string> csv_columns;
        csv_columns.push_back("label");
        csv_columns.insert(csv_columns.end(), group_data_columns.begin(), group_data_columns.end());

        // Build DataType map from group metadata
        std::unordered_map<std::string, DataType> type_map;
        GroupMetadata group_meta{};
        if (group_type == GroupType::Vector) {
            group_meta = get_vector_metadata(collection, group);
        } else if (group_type == GroupType::Set) {
            group_meta = get_set_metadata(collection, group);
        } else {
            group_meta = get_time_series_metadata(collection, group);
        }
        for (const auto& vc : group_meta.value_columns) {
            type_map[vc.name] = vc.data_type;
        }
        // label is always Text
        type_map["label"] = DataType::Text;
        // Dimension column (if time series) is DateTime if its name starts with "date_"
        if (!group_meta.dimension_column.empty()) {
            if (is_date_time_column(group_meta.dimension_column)) {
                type_map[group_meta.dimension_column] = DataType::DateTime;
            } else {
                type_map[group_meta.dimension_column] = DataType::Text;
            }
        }

        // Write header row
        std::vector<std::string> header_fields;
        header_fields.reserve(csv_columns.size());
        for (const auto& col : csv_columns) {
            header_fields.push_back(csv_escape(col));
        }
        write_csv_row(file, header_fields);

        // Build SELECT query: C.label + group data columns with JOIN
        std::string select_cols = "C.label";
        for (const auto& col : group_data_columns) {
            select_cols += ", G." + col;
        }

        // Determine ordering
        std::string order_clause;
        if (group_type == GroupType::Vector) {
            order_clause = "ORDER BY G.id, G.vector_index";
        } else if (group_type == GroupType::Set) {
            order_clause = "ORDER BY G.id";
        } else {
            order_clause = "ORDER BY G.id, G." + group_meta.dimension_column;
        }

        std::string query = "SELECT " + select_cols + " FROM " + table_name + " G JOIN " + collection +
                            " C ON C.id = G.id " + order_clause;

        auto data_result = execute(query);

        // Write data rows
        for (const auto& row : data_result) {
            std::vector<std::string> fields;
            fields.reserve(csv_columns.size());
            for (size_t i = 0; i < csv_columns.size(); ++i) {
                DataType dt = DataType::Text;
                if (auto it = type_map.find(csv_columns[i]); it != type_map.end()) {
                    dt = it->second;
                }
                fields.push_back(value_to_csv_string(row[i], csv_columns[i], dt, options));
            }
            write_csv_row(file, fields);
        }
    }
}

}  // namespace quiver
