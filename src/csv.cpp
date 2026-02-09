#include "quiver/csv.h"

#include <fstream>
#include <iomanip>
#include <rapidcsv.h>
#include <simdutf.h>
#include <sstream>
#include <stdexcept>

namespace quiver {

static std::string utf8_to_latin1(const std::string& utf8) {
    std::string out(utf8.size(), '\0');
    size_t written = simdutf::convert_utf8_to_latin1(utf8.data(), utf8.size(), out.data());
    if (written == 0)
        return utf8;
    out.resize(written);
    return out;
}

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
                    cell = utf8_to_latin1(fk_it->second.at(*id));
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
                    cell = utf8_to_latin1(format_datetime(*str, fmt));
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
                    cell = utf8_to_latin1(*label);
                    doc.SetCell<std::string>(j, i, cell);
                    continue;
                }
            }

            // 5. String or numeric value
            if (auto* str = std::get_if<std::string>(&val)) {
                cell = utf8_to_latin1(*str);
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

}  // namespace quiver
