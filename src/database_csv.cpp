#include "database_impl.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <rapidcsv.h>
#include <sstream>
#include <stdexcept>

namespace quiver {

static std::string format_datetime(const std::string& iso_value, const std::string& fmt) {
    std::tm tm = {};
    std::istringstream ss(iso_value);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail())
        return iso_value;

    std::ostringstream out;
    out << std::put_time(&tm, fmt.c_str());
    return out.str();
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

static std::string value_to_string(const Value& val) {
    std::ostringstream oss;
    oss << val;
    return oss.str();
}

void write_csv(const std::string& path,
               const std::vector<std::string>& columns,
               const Result& data,
               const FkLabelMap& fk_labels,
               const DateFormatMap& date_format_map,
               const EnumMap& enum_map) {
    rapidcsv::Document doc(
        "", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(',', false, false, false, false));

    for (size_t i = 0; i < columns.size(); ++i) {
        doc.SetColumnName(i, columns[i]);
    }

    for (size_t i = 0; i < data.row_count(); ++i) {
        for (size_t j = 0; j < data.column_count(); ++j) {
            const auto& col = columns[j];
            const auto& val = data[i].at(j);

            std::string cell;

            // 1. FK resolution
            auto fk_it = fk_labels.find(col);
            if (fk_it != fk_labels.end()) {
                if (auto* id = std::get_if<int64_t>(&val)) {
                    cell = trim(fk_it->second.at(*id));
                    doc.SetCell<std::string>(j, i, cell);
                    continue;
                }
            }

            // 2. NULL -> empty
            if (std::holds_alternative<std::nullptr_t>(val)) {
                doc.SetCell<std::string>(j, i, "");
                continue;
            }

            // 3. DateTime formatting (date_ prefix)
            if (col.starts_with("date_")) {
                if (auto* str = std::get_if<std::string>(&val)) {
                    auto fmt_it = date_format_map.find(col);
                    auto fmt = fmt_it != date_format_map.end() ? fmt_it->second : "%Y-%m-%dT%H:%M:%S";
                    cell = trim(format_datetime(*str, fmt));
                    doc.SetCell<std::string>(j, i, cell);
                    continue;
                }
            }

            // 4. Enum resolution
            if (enum_map.contains(col)) {
                if (auto* id = std::get_if<int64_t>(&val)) {
                    auto locale = enum_map.get_first_locale();
                    auto label = enum_map.get_enum_label(col, *id, locale);
                    if (!label) {
                        throw std::runtime_error("Enum ID " + std::to_string(*id) +
                                                 " not found in mapping for column " + col);
                    }
                    cell = trim(*label);
                    doc.SetCell<std::string>(j, i, cell);
                    continue;
                }
            }

            // 5. String or numeric value
            if (auto* str = std::get_if<std::string>(&val)) {
                cell = trim(*str);
            } else {
                cell = value_to_string(val);
            }
            doc.SetCell<std::string>(j, i, cell);
        }
    }

    std::ofstream csv_file(path);
    csv_file << "sep=,\n";
    doc.Save(csv_file);
    csv_file.close();
    if (!csv_file.good()) {
        throw std::runtime_error("Failed to write CSV file at path: " + path);
    }
}

void Database::export_csv(const std::string& table,
                          const std::string& path,
                          const DateFormatMap& date_format_map,
                          const EnumMap& enum_map) {
    impl_->logger->debug("Exporting table {} to CSV at path '{}'", table, path);
    impl_->require_collection(table, "export to CSV");

    const auto* table_def = impl_->schema->get_table(table);

    // Build export columns (label first, skip id if table has label)
    bool has_label = table_def->has_column("label");
    std::vector<std::string> columns;
    if (has_label) {
        columns.push_back("label");
    }
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id" && has_label)
            continue;
        if (col_name == "label")
            continue;
        columns.push_back(col_name);
    }

    // Build id->label lookup for FK columns whose target table has a label
    FkLabelMap fk_labels;
    for (const auto& fk : table_def->foreign_keys) {
        const auto* target = impl_->schema->get_table(fk.to_table);
        if (!target || !target->has_column("label"))
            continue;
        auto& lookup = fk_labels[fk.from_column];
        for (auto& row : execute("SELECT id, label FROM " + fk.to_table)) {
            lookup[*row.get_integer(0)] = *row.get_string(1);
        }
    }

    // Query all data
    std::string sql = "SELECT ";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += columns[i];
    }
    sql += " FROM " + table;
    auto data = execute(sql);

    write_csv(path, columns, data, fk_labels, date_format_map, enum_map);
}

void Database::import_csv(const std::string& table, const std::string& path) {
    impl_->logger->debug("Importing table {} from CSV at path '{}'", table, path);
    impl_->require_collection(table, "import from CSV");
    // TODO: implement
}

}  // namespace quiver
