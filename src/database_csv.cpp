#include "database_impl.h"
#include "quiver/options.h"
#include "quiver/schema.h"

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

// Cross-platform ISO 8601 parser using std::get_time (not strptime).
// Tries "YYYY-MM-DDTHH:MM:SS" first, then "YYYY-MM-DD HH:MM:SS".
static bool parse_iso8601(const std::string& datetime_str, std::tm& tm) {
    std::memset(&tm, 0, sizeof(tm));
    std::istringstream ss(datetime_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (!ss.fail() && tm.tm_mday >= 1) {
        return true;
    }
    std::memset(&tm, 0, sizeof(tm));
    ss.clear();
    ss.str(datetime_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return !ss.fail() && tm.tm_mday >= 1;
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
            return format_datetime(str_val, options.date_time_format);
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

// ============================================================================
// import_csv helpers
// ============================================================================

// Trim leading and trailing whitespace.
static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    return s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
}

// Read a trimmed cell from a rapidcsv Document.
static std::string read_cell(const rapidcsv::Document& doc, size_t col, size_t row) {
    return trim(doc.GetCell<std::string>(col, row));
}

// Lowercase a string for case-insensitive comparison.
static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

// Read a CSV file, handling sep= header and semicolon-delimited files.
// Returns a rapidcsv Document with column headers (LabelParams row 0).
static rapidcsv::Document read_csv_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot import_csv: could not open file: " + path);
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Detect and strip sep= header line
    char separator = ',';
    if (content.starts_with("sep=")) {
        auto nl = content.find('\n');
        if (nl != std::string::npos) {
            // Extract the separator character (e.g., "sep=," -> ',', "sep=;" -> ';')
            if (nl >= 5) {
                separator = content[4];
            }
            content = content.substr(nl + 1);
        }
    }

    // If semicolon separator, replace all ; with , for rapidcsv parsing
    if (separator == ';') {
        std::replace(content.begin(), content.end(), ';', ',');
    } else if (separator == ',' && content.find(';') != std::string::npos && content.find(',') == std::string::npos) {
        // No sep= header but semicolons present and no commas -> semicolon-delimited
        std::replace(content.begin(), content.end(), ';', ',');
    }

    std::istringstream ss(content);
    auto doc = rapidcsv::Document(
        ss, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(',', false, false, true, true, '"'));

    if (doc.GetColumnCount() == 0) {
        throw std::runtime_error("Cannot import_csv: CSV file is empty.");
    }

    return doc;
}

// Parse a datetime string from CSV back to ISO 8601 storage format.
// If format is empty, validates the input is already ISO 8601.
// If format is non-empty, parses with that format and reformats to ISO 8601.
static std::string parse_datetime_import(const std::string& raw_value, const std::string& format) {
    if (format.empty()) {
        // Assume input is already ISO 8601; validate
        std::tm tm{};
        if (!parse_iso8601(raw_value, tm)) {
            throw std::runtime_error("Cannot import_csv: Timestamp " + raw_value +
                                     " is not valid. Please provide a valid timestamp with format %Y-%m-%dT%H:%M:%S.");
        }
        // Reformat to canonical form
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
        return std::string(buffer);
    }

    // Parse with custom format, reformat to ISO 8601
    std::tm tm{};
    std::memset(&tm, 0, sizeof(tm));
    std::istringstream ss(raw_value);
    ss >> std::get_time(&tm, format.c_str());
    if (ss.fail() || tm.tm_mday < 1) {
        throw std::runtime_error("Cannot import_csv: Timestamp " + raw_value +
                                 " is not valid. Please provide a valid timestamp with format " + format + ".");
    }

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buffer);
}

// Resolve an enum text label back to its integer value.
// Searches all locales in the enum_labels map with case-insensitive matching.
static int64_t resolve_enum_value(const std::string& cell, const std::string& column, const CSVOptions& options) {
    auto attr_it = options.enum_labels.find(column);
    if (attr_it == options.enum_labels.end()) {
        throw std::runtime_error("Cannot import_csv: Invalid enum value '" + cell + "' for column '" + column + "'.");
    }

    std::string cell_lower = to_lower(cell);
    for (const auto& [locale, labels] : attr_it->second) {
        for (const auto& [label, value] : labels) {
            if (to_lower(label) == cell_lower) {
                return value;
            }
        }
    }

    throw std::runtime_error("Cannot import_csv: Invalid enum value '" + cell + "' for column '" + column + "'.");
}

// Validate that CSV columns match expected database columns (count and names).
static void validate_columns_match(const std::vector<std::string>& csv_cols, const std::vector<std::string>& db_cols) {
    if (csv_cols.size() != db_cols.size()) {
        throw std::runtime_error(
            "Cannot import_csv: The number of columns in the CSV file does not match the number of columns in the "
            "database.");
    }

    std::set<std::string> csv_set(csv_cols.begin(), csv_cols.end());
    std::set<std::string> db_set(db_cols.begin(), db_cols.end());
    if (csv_set != db_set) {
        throw std::runtime_error(
            "Cannot import_csv: The columns in the CSV file do not match the columns in the database.");
    }
}

// Get CSV column names from a rapidcsv Document.
static std::vector<std::string> get_csv_columns(const rapidcsv::Document& doc) {
    const auto size = doc.GetColumnCount();

    std::vector<std::string> cols;
    cols.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        cols.push_back(doc.GetColumnName(i));
    }
    return cols;
}

// Get DB columns in schema order from a query result, optionally excluding a column.
static std::vector<std::string> get_db_columns(const Result& schema_result, const std::string& exclude = "") {
    std::vector<std::string> cols;
    for (const auto& col : schema_result.columns()) {
        if (col != exclude) {
            cols.push_back(col);
        }
    }
    return cols;
}

// Build a label → id map from the parent collection.
static std::unordered_map<std::string, int64_t> build_label_to_id_map(Database& db, const std::string& collection) {
    std::unordered_map<std::string, int64_t> label_to_id;
    auto ids = db.read_scalar_integers(collection, "id");
    auto labels = db.read_scalar_strings(collection, "label");
    for (size_t i = 0; i < ids.size(); ++i) {
        label_to_id[labels[i]] = ids[i];
    }
    return label_to_id;
}

void Database::import_csv(const std::string& collection,
                          const std::string& group,
                          const std::string& path,
                          const CSVOptions& options) {
    impl_->require_collection(collection, "import_csv");

    // Resolve target table name
    auto table_name = collection;
    GroupTableType group_type;

    if (!group.empty()) {
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
            throw std::runtime_error("Cannot import_csv: group not found: '" + group + "' in collection '" +
                                     collection + "'");
        }
    }

    // Read CSV, validate columns against DB schema, handle empty CSV
    auto doc = read_csv_file(path);
    auto csv_cols = get_csv_columns(doc);
    auto db_cols = get_db_columns(execute("SELECT * FROM " + table_name + " LIMIT 0"), group.empty() ? "id" : "");

    // Scalar path: require label column before general column validation
    if (group.empty()) {
        if (std::find(csv_cols.begin(), csv_cols.end(), "label") == csv_cols.end()) {
            throw std::runtime_error("Cannot import_csv: CSV file does not contain a 'label' column.");
        }
    }

    validate_columns_match(csv_cols, db_cols);

    // Validate per-row column count
    for (size_t row = 0; row < doc.GetRowCount(); ++row) {
        auto row_data = doc.GetRow<std::string>(row);
        if (row_data.size() != csv_cols.size()) {
            throw std::runtime_error("Cannot import_csv: The number of columns of row " + std::to_string(row + 1) +
                                     " in the CSV file does not match the number of columns in the database.");
        }
    }

    auto row_count = doc.GetRowCount();
    if (row_count == 0) {
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            execute_raw("DELETE FROM " + table_name);
            execute_raw("PRAGMA foreign_keys = ON");
        } catch (...) {
            execute_raw("PRAGMA foreign_keys = ON");
            throw;
        }
        return;
    }

    // Find the CSV column index for each db column
    std::unordered_map<std::string, size_t> csv_col_index;
    for (size_t i = 0; i < csv_cols.size(); ++i) {
        csv_col_index[csv_cols[i]] = i;
    }

    if (group.empty()) {
        // ================================================================
        // Scalar import path
        // ================================================================

        const auto* table_def = impl_->schema->get_table(collection);

        // Build FK map: column_name -> ForeignKey
        std::unordered_map<std::string, ForeignKey> fk_map;
        for (const auto& fk : table_def->foreign_keys) {
            fk_map[fk.from_column] = fk;
        }

        // Build FK label→id lookup maps for non-self foreign keys
        std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> fk_label_maps;
        for (const auto& [col_name, fk] : fk_map) {
            if (fk.to_table != collection) {
                fk_label_maps[col_name] = build_label_to_id_map(*this, fk.to_table);
            }
        }

        // Validation pass: check all cells before mutating
        for (size_t row = 0; row < row_count; ++row) {
            for (const auto& col_name : db_cols) {
                auto cell = read_cell(doc, csv_col_index[col_name], row);
                const auto* col_def = table_def->get_column(col_name);
                auto fk_it = fk_map.find(col_name);
                auto is_fk = fk_it != fk_map.end();
                auto is_self_fk = is_fk && fk_it->second.to_table == collection;

                if (cell.empty()) {
                    if (col_def && col_def->not_null) {
                        throw std::runtime_error("Cannot import_csv: Column " + col_name + " cannot be NULL.");
                    }
                    continue;
                }

                if (is_fk && !is_self_fk) {
                    if (fk_label_maps[col_name].find(cell) == fk_label_maps[col_name].end()) {
                        throw std::runtime_error(
                            "Cannot import_csv: Could not find an existing element from collection " +
                            fk_it->second.to_table + " with label " + cell +
                            ".\nCreate the element before referencing it.");
                    }
                }

                auto type = col_def ? col_def->type : DataType::Text;

                if (type == DataType::DateTime || is_date_time_column(col_name)) {
                    parse_datetime_import(cell, options.date_time_format);
                }

                if (type == DataType::Integer && !is_fk) {
                    try {
                        (void)std::stoll(cell);
                    } catch (...) {
                        if (options.enum_labels.count(col_name) > 0) {
                            resolve_enum_value(cell, col_name, options);
                        } else {
                            throw std::runtime_error("Cannot import_csv: Invalid integer value '" + cell +
                                                     "' for column '" + col_name + "'.");
                        }
                    }
                }

                if (type == DataType::Real) {
                    try {
                        (void)std::stod(cell);
                    } catch (...) {
                        throw std::runtime_error("Cannot import_csv: Invalid float value '" + cell + "' for column '" +
                                                 col_name + "'.");
                    }
                }
            }
        }

        // Data import: disable FK → DELETE → INSERT → enable FK
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            impl_->begin_transaction();

            execute_raw("DELETE FROM " + collection);

            // Build INSERT statement
            std::string insert_cols;
            std::string insert_placeholders;
            for (size_t i = 0; i < db_cols.size(); ++i) {
                if (i > 0) {
                    insert_cols += ", ";
                    insert_placeholders += ", ";
                }
                insert_cols += db_cols[i];
                insert_placeholders += "?";
            }
            auto insert_sql =
                "INSERT INTO " + collection + " (" + insert_cols + ") VALUES (" + insert_placeholders + ")";

            for (size_t row = 0; row < row_count; ++row) {
                std::vector<Value> params;
                for (const auto& col_name : db_cols) {
                    std::string cell = read_cell(doc, csv_col_index[col_name], row);
                    const auto* col_def = table_def->get_column(col_name);
                    auto fk_it = fk_map.find(col_name);
                    auto is_fk = fk_it != fk_map.end();
                    auto is_self_fk = is_fk && fk_it->second.to_table == collection;

                    if (cell.empty()) {
                        params.emplace_back(nullptr);
                        continue;
                    }

                    if (is_self_fk) {
                        params.emplace_back(nullptr);
                        continue;
                    }

                    if (is_fk) {
                        params.emplace_back(fk_label_maps[col_name].at(cell));
                        continue;
                    }

                    auto type = col_def ? col_def->type : DataType::Text;

                    if (type == DataType::DateTime || is_date_time_column(col_name)) {
                        params.emplace_back(parse_datetime_import(cell, options.date_time_format));
                        continue;
                    }

                    if (type == DataType::Integer) {
                        try {
                            params.emplace_back(std::stoll(cell));
                        } catch (...) {
                            params.emplace_back(resolve_enum_value(cell, col_name, options));
                        }
                        continue;
                    }

                    if (type == DataType::Real) {
                        params.emplace_back(std::stod(cell));
                        continue;
                    }

                    params.emplace_back(cell);
                }

                execute(insert_sql, params);
            }

            // Second pass: resolve self-referencing FKs
            std::vector<std::string> self_fk_cols;
            for (const auto& [col_name, fk] : fk_map) {
                if (fk.to_table == collection) {
                    self_fk_cols.push_back(col_name);
                }
            }

            if (!self_fk_cols.empty()) {
                auto self_label_to_id = build_label_to_id_map(*this, collection);

                for (const auto& col_name : self_fk_cols) {
                    for (size_t row = 0; row < row_count; ++row) {
                        auto cell = read_cell(doc, csv_col_index[col_name], row);
                        if (cell.empty())
                            continue;

                        auto label = read_cell(doc, csv_col_index["label"], row);

                        if (self_label_to_id.find(cell) == self_label_to_id.end()) {
                            throw std::runtime_error(
                                "Cannot import_csv: Could not find an existing element from collection " + collection +
                                " with label " + cell + ".\nCreate the element before referencing it.");
                        }

                        execute("UPDATE " + collection + " SET " + col_name + " = ? WHERE id = ?",
                                {self_label_to_id.at(cell), self_label_to_id.at(label)});
                    }
                }
            }

            impl_->commit();
            execute_raw("PRAGMA foreign_keys = ON");
        } catch (const std::exception& e) {
            impl_->rollback();
            execute_raw("PRAGMA foreign_keys = ON");

            std::string msg = e.what();
            if (msg.find("UNIQUE constraint") != std::string::npos) {
                throw std::runtime_error("Cannot import_csv: There are duplicate entries in the CSV file.");
            }
            throw;
        }
    } else {
        // ================================================================
        // Group import path
        // ================================================================

        auto label_to_id = build_label_to_id_map(*this, collection);

        const auto* table_def = impl_->schema->get_table(table_name);

        // Build FK map (excluding the parent id FK)
        std::unordered_map<std::string, ForeignKey> fk_map;
        for (const auto& fk : table_def->foreign_keys) {
            if (fk.from_column != "id") {
                fk_map[fk.from_column] = fk;
            }
        }

        // Build type map from group metadata for DateTime detection
        GroupMetadata group_meta{};
        if (group_type == GroupTableType::Vector) {
            group_meta = get_vector_metadata(collection, group);
        } else if (group_type == GroupTableType::Set) {
            group_meta = get_set_metadata(collection, group);
        } else {
            group_meta = get_time_series_metadata(collection, group);
        }

        std::unordered_map<std::string, DataType> type_map;
        for (const auto& vc : group_meta.value_columns) {
            type_map[vc.name] = vc.data_type;
        }
        if (!group_meta.dimension_column.empty() && is_date_time_column(group_meta.dimension_column)) {
            type_map[group_meta.dimension_column] = DataType::DateTime;
        }

        // Build FK label→id lookup maps for non-id foreign keys
        std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> fk_label_maps;
        for (const auto& [col_name, fk] : fk_map) {
            fk_label_maps[col_name] = build_label_to_id_map(*this, fk.to_table);
        }

        // Helper: resolve effective DataType for a group column
        auto get_type = [&](const std::string& col_name) -> DataType {
            if (col_name == "id" || col_name == "vector_index")
                return DataType::Integer;
            if (auto it = type_map.find(col_name); it != type_map.end())
                return it->second;
            const auto* col_def = table_def->get_column(col_name);
            return col_def ? col_def->type : DataType::Text;
        };

        // Validation pass: vector_index consecutive check
        if (group_type == GroupTableType::Vector) {
            std::unordered_map<std::string, std::vector<int64_t>> element_vector_indices;
            auto id_csv_idx = csv_col_index["id"];
            auto vi_csv_idx = csv_col_index["vector_index"];

            for (size_t row = 0; row < row_count; ++row) {
                auto id_label = read_cell(doc, id_csv_idx, row);
                auto vi_str = read_cell(doc, vi_csv_idx, row);
                try {
                    element_vector_indices[id_label].push_back(std::stoll(vi_str));
                } catch (...) {
                    throw std::runtime_error(
                        "Cannot import_csv: Column vector_index must be consecutive, unique and start at 1.");
                }
            }

            for (const auto& [label, indices] : element_vector_indices) {
                for (size_t i = 0; i < indices.size(); ++i) {
                    if (indices[i] != static_cast<int64_t>(i + 1)) {
                        throw std::runtime_error(
                            "Cannot import_csv: Column vector_index must be consecutive, unique and start at 1.");
                    }
                }
            }
        }

        // Validation pass: per-cell checks
        for (size_t row = 0; row < row_count; ++row) {
            for (const auto& col_name : db_cols) {
                auto cell = read_cell(doc, csv_col_index[col_name], row);

                if (col_name == "id") {
                    if (label_to_id.find(cell) == label_to_id.end()) {
                        throw std::runtime_error("Cannot import_csv: Element with id " + cell +
                                                 " does not exist in collection " + collection + ".");
                    }
                    continue;
                }

                const auto* col_def = table_def->get_column(col_name);

                if (cell.empty()) {
                    if (col_def && col_def->not_null) {
                        throw std::runtime_error("Cannot import_csv: Column " + col_name + " cannot be NULL.");
                    }
                    continue;
                }

                if (auto fk_it = fk_map.find(col_name); fk_it != fk_map.end()) {
                    if (fk_label_maps[col_name].find(cell) == fk_label_maps[col_name].end()) {
                        throw std::runtime_error(
                            "Cannot import_csv: Could not find an existing element from collection " +
                            fk_it->second.to_table + " with label " + cell + ".");
                    }
                }

                auto type = get_type(col_name);
                auto is_datetime = (type == DataType::DateTime || is_date_time_column(col_name));

                if (is_datetime && col_name != "id" && col_name != "vector_index") {
                    parse_datetime_import(cell, options.date_time_format);
                }
            }
        }

        // Data import: disable FK → DELETE → INSERT → enable FK
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            impl_->begin_transaction();

            execute_raw("DELETE FROM " + table_name);

            // Build INSERT statement
            std::string insert_cols;
            std::string insert_placeholders;
            for (size_t i = 0; i < db_cols.size(); ++i) {
                if (i > 0) {
                    insert_cols += ", ";
                    insert_placeholders += ", ";
                }
                insert_cols += db_cols[i];
                insert_placeholders += "?";
            }
            auto insert_sql =
                "INSERT INTO " + table_name + " (" + insert_cols + ") VALUES (" + insert_placeholders + ")";

            for (size_t row = 0; row < row_count; ++row) {
                std::vector<Value> params;
                for (const auto& col_name : db_cols) {
                    auto cell = read_cell(doc, csv_col_index[col_name], row);

                    if (col_name == "id") {
                        params.emplace_back(label_to_id.at(cell));
                        continue;
                    }

                    if (cell.empty()) {
                        params.emplace_back(nullptr);
                        continue;
                    }

                    if (col_name == "vector_index") {
                        params.emplace_back(std::stoll(cell));
                        continue;
                    }

                    if (fk_map.count(col_name) > 0) {
                        params.emplace_back(fk_label_maps[col_name].at(cell));
                        continue;
                    }

                    auto type = get_type(col_name);
                    auto is_datetime = (type == DataType::DateTime || is_date_time_column(col_name));

                    if (is_datetime) {
                        params.emplace_back(parse_datetime_import(cell, options.date_time_format));
                        continue;
                    }

                    if (type == DataType::Integer) {
                        try {
                            params.emplace_back(std::stoll(cell));
                        } catch (...) {
                            params.emplace_back(resolve_enum_value(cell, col_name, options));
                        }
                        continue;
                    }

                    if (type == DataType::Real) {
                        params.emplace_back(std::stod(cell));
                        continue;
                    }

                    params.emplace_back(cell);
                }

                execute(insert_sql, params);
            }

            impl_->commit();
            execute_raw("PRAGMA foreign_keys = ON");
        } catch (const std::exception& e) {
            impl_->rollback();
            execute_raw("PRAGMA foreign_keys = ON");

            std::string msg = e.what();
            if (msg.find("UNIQUE constraint") != std::string::npos) {
                throw std::runtime_error("Cannot import_csv: There are duplicate entries in the CSV file.");
            }
            throw;
        }
    }
}

}  // namespace quiver
