#include "quiver/csv.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
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
    rapidcsv::Document doc("", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(',', false, false));

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
                    cell = fk_it->second.at(*id);
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
                    cell = format_datetime(*str, fmt);
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
                    cell = *label;
                    doc.SetCell<std::string>(j, i, cell);
                    continue;
                }
            }

            // 5. String or numeric value
            if (auto* str = std::get_if<std::string>(&val)) {
                cell = *str;
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
}

CsvData read_csv(const std::string& path,
                 const ColumnTypeMap& column_types,
                 const FkLabelMap& fk_labels,
                 const DateFormatMap& date_format_map,
                 const EnumMap& enum_map) {
    // Detect separator
    std::ifstream csv_file(path);
    std::string first_line;
    std::getline(csv_file, first_line);
    char separator = ',';
    bool has_sep_line = false;

    if (first_line.starts_with("sep=") && first_line.size() > 4) {
        separator = first_line[4];
        has_sep_line = true;
    } else {
        // Sniff header line: pick the most frequent delimiter candidate
        const char candidates[] = {',', ';', '\t', '|'};
        size_t best_count = 0;
        for (char c : candidates) {
            auto count = static_cast<size_t>(std::count(first_line.begin(), first_line.end(), c));
            if (count > best_count) {
                best_count = count;
                separator = c;
            }
        }
    }
    csv_file.close();

    // If sep= line exists, skip it (row 1 becomes header)
    auto label_params = has_sep_line ? rapidcsv::LabelParams(1, -1) : rapidcsv::LabelParams(0, -1);
    rapidcsv::Document doc(path, label_params, rapidcsv::SeparatorParams(separator, false, false));

    // Process each column: resolve FKs, enums, date formats
    for (size_t j = 0; j < doc.GetColumnCount(); ++j) {
        const auto& col = doc.GetColumnName(j);

        // Reverse FK resolution: label -> id string
        auto fk_it = fk_labels.find(col);
        if (fk_it != fk_labels.end()) {
            auto& lookup = fk_it->second;
            for (size_t i = 0; i < doc.GetRowCount(); ++i) {
                auto cell = doc.GetCell<std::string>(j, i);
                if (cell.empty())
                    continue;
                int64_t id = -1;
                for (const auto& [fk_id, label] : lookup) {
                    if (label == cell) {
                        id = fk_id;
                        break;
                    }
                }
                if (id == -1) {
                    throw std::runtime_error("Label '" + cell + "' not found in FK mapping for column " + col);
                }
                doc.SetCell<std::string>(j, i, std::to_string(id));
            }
            continue;
        }

        // Enum resolution: label -> id string (cross-locale)
        if (enum_map.contains(col)) {
            for (size_t i = 0; i < doc.GetRowCount(); ++i) {
                auto cell = doc.GetCell<std::string>(j, i);
                if (cell.empty())
                    continue;
                auto id_opt = enum_map.find_enum_id(col, cell);
                if (!id_opt) {
                    throw std::runtime_error("Enum label '" + cell + "' not found in mapping for column " + col);
                }
                doc.SetCell<std::string>(j, i, std::to_string(*id_opt));
            }
            continue;
        }

        // DateTime: parse to ISO 8601
        if (col.starts_with("date_")) {
            for (size_t i = 0; i < doc.GetRowCount(); ++i) {
                auto cell = doc.GetCell<std::string>(j, i);
                if (cell.empty())
                    continue;
                auto iso_value = format_datetime(cell, "%Y-%m-%dT%H:%M:%S");
                doc.SetCell<std::string>(j, i, iso_value);
            }
            continue;
        }
    }

    // Build CsvData with type coercion
    CsvData result;
    for (size_t j = 0; j < doc.GetColumnCount(); ++j) {
        result.columns.push_back(doc.GetColumnName(j));
    }
    for (size_t i = 0; i < doc.GetRowCount(); ++i) {
        std::vector<Value> row;
        row.reserve(doc.GetColumnCount());
        for (size_t j = 0; j < doc.GetColumnCount(); ++j) {
            auto cell = doc.GetCell<std::string>(j, i);
            if (cell.empty()) {
                row.push_back(nullptr);
                continue;
            }
            auto type_it = column_types.find(result.columns[j]);
            if (type_it != column_types.end()) {
                switch (type_it->second) {
                case DataType::Integer:
                    row.push_back(std::stoll(cell));
                    break;
                case DataType::Real:
                    row.push_back(std::stod(cell));
                    break;
                default:
                    row.push_back(cell);
                    break;
                }
            } else {
                row.push_back(cell);
            }
        }
        result.rows.push_back(std::move(row));
    }
    return result;
}

}  // namespace quiver
