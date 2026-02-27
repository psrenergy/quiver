#include "database_impl.h"
#include "quiver/options.h"
#include "quiver/schema.h"
#include "utils/datetime.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <rapidcsv.h>
#include <set>
#include <sstream>

namespace quiver {

// Convert a Value to its CSV string representation.
// NULL -> empty string, integers check enum_labels,
// floats use %g for clean output, strings apply DateTime formatting.
// rapidcsv handles CSV escaping/quoting via auto-quote.
static std::string
value_to_csv_string(const Value& value, const std::string& column_name, DataType data_type, const CSVOptions& options) {
    // NULL -> empty field
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return "";
    }

    // Integer with possible enum resolution (reverse search: find label whose value matches)
    if (std::holds_alternative<int64_t>(value)) {
        auto int_val = std::get<int64_t>(value);
        if (auto attr_it = options.enum_labels.find(column_name); attr_it != options.enum_labels.end()) {
            for (const auto& [locale, labels] : attr_it->second) {
                for (const auto& [label, val] : labels) {
                    if (val == int_val)
                        return label;
                }
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
            return datetime::format_datetime(str_val, options.date_time_format);
        }
        return str_val;
    }

    return "";
}

// Build a rapidcsv Document with column headers enabled and LF-only line endings.
// SeparatorParams: comma separator, no trim, no CR (LF only), quoted linebreaks, auto-quote, double-quote char.
static rapidcsv::Document make_csv_document() {
    return rapidcsv::Document(
        "", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(',', false, false, true, true, '"'));
}

// Save a rapidcsv Document to a file path via stringstream intermediary.
// Uses binary mode to prevent Windows CRLF conversion.
static void save_csv_document(rapidcsv::Document& doc, const std::string& path) {
    std::ostringstream oss;
    oss << "sep=,\n";
    doc.Save(oss);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to export_csv: could not open file: " + path);
    }
    file << oss.str();
}

// Render query results to a CSV file: create document, set headers, populate cells, save.
// Column types are resolved once from type_map (invariant across rows).
static void write_csv(const Result& data_result,
                      const std::vector<std::string>& csv_columns,
                      const std::unordered_map<std::string, DataType>& type_map,
                      const CSVOptions& options,
                      const std::string& path) {
    auto doc = make_csv_document();

    for (size_t i = 0; i < csv_columns.size(); ++i) {
        doc.SetColumnName(i, csv_columns[i]);
    }

    // Resolve column types once (invariant across rows)
    std::vector<DataType> col_types(csv_columns.size(), DataType::Text);
    for (size_t i = 0; i < csv_columns.size(); ++i) {
        if (auto it = type_map.find(csv_columns[i]); it != type_map.end()) {
            col_types[i] = it->second;
        }
    }

    size_t row_idx = 0;
    for (const auto& row : data_result) {
        for (size_t i = 0; i < csv_columns.size(); ++i) {
            doc.SetCell<std::string>(i, row_idx, value_to_csv_string(row[i], csv_columns[i], col_types[i], options));
        }
        ++row_idx;
    }

    save_csv_document(doc, path);
}

void Database::export_csv(const std::string& collection,
                          const std::string& group,
                          const std::string& path,
                          const CSVOptions& options) {
    namespace fs = std::filesystem;

    // Create parent directories (mkdir -p)
    auto parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
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

        // Build SELECT query with columns in schema order
        std::string select_cols;
        for (size_t i = 0; i < csv_columns.size(); ++i) {
            if (i > 0)
                select_cols += ", ";
            select_cols += csv_columns[i];
        }

        auto data_result = execute("SELECT " + select_cols + " FROM " + collection + " ORDER BY rowid");
        write_csv(data_result, csv_columns, type_map, options, path);
    } else {
        // Group export
        impl_->require_collection(collection, "export_csv");

        // Determine group type by checking schema for matching table names
        std::string table_name;
        GroupTableType group_type{};

        auto vec_table = Schema::vector_table_name(collection, group);
        auto set_table = Schema::set_table_name(collection, group);
        auto ts_table = Schema::time_series_table_name(collection, group);

        if (impl_->schema->has_table(vec_table)) {
            table_name = vec_table;
            group_type = GroupTableType::Vector;
        } else if (impl_->schema->has_table(set_table)) {
            table_name = set_table;
            group_type = GroupTableType::Set;
        } else if (impl_->schema->has_table(ts_table)) {
            table_name = ts_table;
            group_type = GroupTableType::TimeSeries;
        } else {
            throw std::runtime_error("Cannot export_csv: group not found: '" + group + "' in collection '" +
                                     collection + "'");
        }

        // Get group table columns in schema definition order
        auto schema_result = execute("SELECT * FROM " + table_name + " LIMIT 0");
        const auto& all_group_columns = schema_result.columns();

        // All group table columns become CSV columns
        std::vector<std::string> group_data_columns;
        group_data_columns.reserve(all_group_columns.size());
        for (const auto& col : all_group_columns) {
            group_data_columns.push_back(col);
        }

        // CSV headers: label first, then group data columns
        std::vector<std::string> csv_columns;
        csv_columns.insert(csv_columns.end(), group_data_columns.begin(), group_data_columns.end());

        // Build DataType map from group metadata
        std::unordered_map<std::string, DataType> type_map;
        GroupMetadata group_meta{};
        if (group_type == GroupTableType::Vector) {
            group_meta = get_vector_metadata(collection, group);
        } else if (group_type == GroupTableType::Set) {
            group_meta = get_set_metadata(collection, group);
        } else {
            group_meta = get_time_series_metadata(collection, group);
        }
        for (const auto& vc : group_meta.value_columns) {
            type_map[vc.name] = vc.data_type;
        }
        // id is always Text as it takes the label value from the parent collection
        type_map["id"] = DataType::Text;
        // Dimension column (if time series) is DateTime if its name starts with "date_"
        if (!group_meta.dimension_column.empty()) {
            if (is_date_time_column(group_meta.dimension_column)) {
                type_map[group_meta.dimension_column] = DataType::DateTime;
            } else {
                type_map[group_meta.dimension_column] = DataType::Text;
            }
        }

        // Build SELECT query: C.label + group data columns with JOIN
        std::string select_cols = "C.label";
        for (const auto& col : group_data_columns) {
            if (col == "id")
                continue;
            select_cols += ", G." + col;
        }

        // Determine ordering
        std::string order_clause;
        if (group_type == GroupTableType::Vector) {
            order_clause = "ORDER BY G.id, G.vector_index";
        } else if (group_type == GroupTableType::Set) {
            order_clause = "ORDER BY G.id";
        } else {
            order_clause = "ORDER BY G.id, G." + group_meta.dimension_column;
        }

        std::string query = "SELECT " + select_cols + " FROM " + table_name + " G JOIN " + collection +
                            " C ON C.id = G.id " + order_clause;

        auto data_result = execute(query);
        write_csv(data_result, csv_columns, type_map, options, path);
    }
}

}  // namespace quiver
