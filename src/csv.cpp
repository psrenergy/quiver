#include "quiver/csv.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace quiver {

// Decodes a UTF-8 code point starting at position i, advances i past it.
// Returns the Unicode code point, or -1 for invalid sequences.
static int decode_utf8(const std::string& s, size_t& i) {
    unsigned char b = s[i];
    if (b <= 0x7F)
        return b;
    // Count leading 1-bits to determine sequence length (2, 3, or 4 bytes)
    int len = (b < 0xE0) ? 2 : (b < 0xF0) ? 3 : 4;
    int cp = b & (0x3F >> len);  // extract payload bits from leading byte
    for (int j = 1; j < len && i + j < s.size(); ++j)
        cp = (cp << 6) | (s[i + j] & 0x3F);
    i += len - 1;
    return cp;
}

static std::string utf8_to_latin1(const std::string& utf8) {
    std::string out;
    out.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size(); ++i) {
        int cp = decode_utf8(utf8, i);
        out += (cp >= 0 && cp <= 0xFF) ? static_cast<char>(cp) : '?';
    }
    return out;
}

static void write_csv_field(std::ofstream& out, const std::string& field) {
    auto latin1 = utf8_to_latin1(field);
    if (latin1.find_first_of(",\"\n") != std::string::npos) {
        out << '"';
        for (char c : latin1) {
            if (c == '"')
                out << '"';
            out << c;
        }
        out << '"';
    } else {
        out << latin1;
    }
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

void write_csv(const std::string& path,
               const std::vector<std::string>& columns,
               const Result& data,
               const FkLabelMap& fk_labels,
               const DateFormatMap& date_format_map,
               const EnumMap& enum_map) {
    std::ofstream csv_file;
    csv_file.open(path);
    csv_file << "sep=,\n";

    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            csv_file << ",";
        csv_file << columns[i];
    }
    csv_file << "\n";

    for (size_t i = 0; i < data.row_count(); ++i) {
        for (size_t j = 0; j < data.column_count(); ++j) {
            if (j > 0)
                csv_file << ",";
            const auto& col = columns[j];
            const auto& val = data[i].at(j);

            // 1. FK resolution
            auto fk_it = fk_labels.find(col);
            if (fk_it != fk_labels.end()) {
                if (auto* id = std::get_if<int64_t>(&val)) {
                    write_csv_field(csv_file, fk_it->second.at(*id));
                    continue;
                }
            }

            // 2. NULL -> empty
            if (std::holds_alternative<std::nullptr_t>(val))
                continue;

            // 3. DateTime formatting (date_ prefix)
            if (col.starts_with("date_")) {
                if (auto* str = std::get_if<std::string>(&val)) {
                    auto fmt_it = date_format_map.find(col);
                    auto fmt = fmt_it != date_format_map.end() ? fmt_it->second : "%Y-%m-%dT%H:%M:%S";
                    write_csv_field(csv_file, format_datetime(*str, fmt));
                    continue;
                }
            }

            // 4. Enum resolution
            auto enum_it = enum_map.find(col);
            if (enum_it != enum_map.end()) {
                if (auto* id = std::get_if<int64_t>(&val)) {
                    auto label_it = enum_it->second.find(*id);
                    if (label_it == enum_it->second.end()) {
                        throw std::runtime_error("Enum ID " + std::to_string(*id) +
                                                 " not found in mapping for column " + col);
                    }
                    write_csv_field(csv_file, label_it->second);
                    continue;
                }
            }

            // 5. String with CSV quoting
            if (auto* str = std::get_if<std::string>(&val)) {
                write_csv_field(csv_file, *str);
            } else {
                csv_file << val;
            }
        }
        csv_file << "\n";
    }
    csv_file.close();
}

}  // namespace quiver
