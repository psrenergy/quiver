#include "csv/csv_read.h"
#include "database_impl.h"
#include "quiver/options.h"
#include "quiver/schema.h"
#include "utils/datetime.h"
#include "utils/string.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace quiver {

// A parsed CSV file: the header names verbatim, then every data row's trimmed cells in file order.
struct CsvTable {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
};

// Whole-cell number parses: nullopt unless every character is consumed, so "1.5" is not an integer
// and "9.99abc" or a decimal-comma "1,5" is not a float (stoll/strtod alone accept any valid prefix).
// Import writes through a raw INSERT, so this is where the core typing policy -- a float into an
// INTEGER column is rejected -- reaches CSV text.
static std::optional<int64_t> parse_integer(const std::string& cell) {
    size_t pos = 0;
    int64_t value = 0;
    try {
        value = std::stoll(cell, &pos);
    } catch (const std::logic_error&) {  // invalid_argument and out_of_range
        return std::nullopt;
    }
    return pos == cell.size() ? std::optional(value) : std::nullopt;
}

// strtod reads the C locale's decimal point, which a host process can switch to ',' (Python's
// locale.setlocale(LC_ALL, "") on a pt-BR machine), while export_csv writes '.' in every locale. So the
// cell is spelled in the active locale first, and parses exactly as it would in the "C" locale.
static std::optional<double> parse_float(std::string cell) {
    const std::string_view point = std::localeconv()->decimal_point;
    if (point != ".") {
        if (cell.find(point) != std::string::npos) {
            return std::nullopt;  // "1,5" is not a number in the "C" locale either
        }
        if (const auto dot = cell.find('.'); dot != std::string::npos) {
            cell.replace(dot, 1, point);
        }
    }
    const char* begin = cell.c_str();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(begin, &end);
    if (end == begin || end != begin + cell.size()) {
        return std::nullopt;
    }
    // ERANGE marks a literal no finite double holds -- overflow to inf, or a nonzero value flushed to
    // 0 -- but glibc and Apple libc also raise it for a representable subnormal, which export_csv
    // writes, so only the first two are rejected.
    if (errno == ERANGE && (std::isinf(value) || value == 0.0)) {
        return std::nullopt;
    }
    return value;
}

// Lowercase a string for case-insensitive comparison.
static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

// csv-parser keeps a closing quote that is followed by ordinary text as a literal and stays in
// quoted mode, where separators and line breaks are plain text -- so `"a" ,b\n"c",d` parses as ONE
// row whose cell count can still match the header, and an unterminated quote swallows the rest of
// the file. db:read_csv tolerates that, but import deletes the table before inserting, so a silently
// merged row is lost data: reject both shapes before parsing. A quote opens a field only as its
// first byte, exactly as in csv-parser; inside one, `""` is an escaped quote.
static void require_well_formed_quotes(std::string_view text, char separator) {
    bool in_quotes = false;
    bool field_start = true;
    size_t line = 1;
    size_t quote_line = 1;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        // A lone CR ends a line too, as it ends a record in csv-parser; a CRLF counts once, at its LF.
        if (c == '\n' || (c == '\r' && (i + 1 == text.size() || text[i + 1] != '\n'))) {
            ++line;
        }
        if (in_quotes) {
            if (c != '"') {
                continue;
            }
            const char next = i + 1 < text.size() ? text[i + 1] : '\n';
            if (next == '"') {
                ++i;
                continue;
            }
            if (next != separator && next != '\r' && next != '\n') {
                throw std::runtime_error("Cannot import_csv: malformed quoted field on line " + std::to_string(line));
            }
            in_quotes = false;
            field_start = false;
            continue;
        }
        if (c == '"' && field_start) {
            in_quotes = true;
            quote_line = line;
        }
        field_start = c == separator || c == '\n' || c == '\r';
    }
    if (in_quotes) {
        throw std::runtime_error("Cannot import_csv: unterminated quoted field on line " + std::to_string(quote_line));
    }
}

// The Reader options for a CSV file, after rejecting malformed quoting. An Excel `sep=X` first line
// names the separator and is skipped, with any blank lines below it; without one, a header line
// holding ';' and no ',' is read as semicolon-delimited. The separator must be known before the
// parser is built and the quote check needs every byte, so the whole file is read here -- and freed
// on return, before the Reader reads it again.
static csv_read::Options sniff_csv_file(const std::string& path) {
    // csv-parser re-reads the file by name, so a short read here would let it import bytes the quote
    // check never saw (a byte-range lock fails ReadFile but not csv-parser's mapped reads): a regular
    // file that cannot be read in full is rejected, not judged empty. A path that is not a regular
    // file (missing, a directory) is left unread for the Reader to report.
    std::string content;
    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec)) {
        const auto size = std::filesystem::file_size(path, ec);
        std::ifstream file(path, std::ios::binary);
        content.resize(ec ? 0 : static_cast<size_t>(size));
        if (ec || !file.read(content.data(), static_cast<std::streamsize>(content.size()))) {
            throw std::runtime_error("Cannot import_csv: cannot read file '" + path + "'");
        }
    }
    std::string_view text = content;
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.remove_prefix(3);
    }
    const auto first_line = text.substr(0, text.find_first_of("\r\n"));

    csv_read::Options options;
    if (first_line.starts_with("sep=")) {
        // "sep=" alone, like "sep=,", means a comma. The header is the first record after the sep line
        // and any blank lines below it, counted the way csv-parser splits records -- at an LF, a CRLF
        // or a lone CR -- so the `\r\r\n` of a doubled text-mode conversion is the sep record plus a
        // blank one. header_row is 1-based and counts the sep record.
        options.separator = first_line.size() > 4 ? first_line[4] : ',';
        auto rest = text.substr(first_line.size());
        int64_t line_ends = 0;
        while (rest.starts_with('\r') || rest.starts_with('\n')) {
            rest.remove_prefix(rest.starts_with("\r\n") ? 2 : 1);
            ++line_ends;
        }
        options.header_row = std::max<int64_t>(line_ends, 1) + 1;
    } else if (first_line.find(';') != std::string_view::npos && first_line.find(',') == std::string_view::npos) {
        options.separator = ';';
    }

    // csv-parser rejects a UTF-16/32 BOM itself, with the real reason; a byte scan of those code units
    // would misreport it as a quote error.
    const bool wide_bom = content.starts_with("\xFF\xFE") || content.starts_with("\xFE\xFF") ||
                          content.starts_with(std::string_view("\0\0\xFE\xFF", 4));
    if (!wide_bom) {
        require_well_formed_quotes(text, options.separator);
    }
    return options;
}

// Read a CSV file through csv_read::Reader, the parser behind db:read_csv, with the options
// sniff_csv_file chose. Trailing empty header columns (an Excel artifact) are dropped, along with up
// to that many trailing empty cells per row. Data cells are trimmed once here; header names stay
// verbatim, since validate_columns_match compares them as written. Every row is buffered: import
// validates all of them before mutating anything.
static CsvTable read_csv_file(const std::string& path) {
    csv_read::Reader reader(path, path, "import_csv", sniff_csv_file(path));
    CsvTable csv{reader.header(), {}};
    reader.for_each_row([&csv](std::vector<std::string>&& cells, int64_t) {
        for (auto& cell : cells) {
            cell = string::trim(cell);
        }
        csv.rows.push_back(std::move(cells));
        return true;
    });

    size_t width = csv.header.size();
    while (width > 0 && string::trim(csv.header[width - 1]).empty()) {
        --width;
    }
    const size_t trailing = csv.header.size() - width;
    csv.header.resize(width);
    for (auto& row : csv.rows) {
        for (size_t n = 0; n < trailing && row.size() > width && row.back().empty(); ++n) {
            row.pop_back();
        }
    }

    return csv;
}

// Parse a datetime string from CSV back to ISO 8601 storage format.
// If format is empty, validates the input is already ISO 8601.
// If format is non-empty, parses with that format and reformats to ISO 8601.
static std::string parse_datetime_import(const std::string& raw_value, const std::string& format) {
    std::tm tm{};
    bool parsed = false;
    if (format.empty()) {
        parsed = datetime::parse_iso8601(raw_value, tm);
    } else {
        std::istringstream ss(raw_value);
        parsed = !(ss >> std::get_time(&tm, format.c_str())).fail();
    }

    char buffer[64] = {};
    if (parsed && tm.tm_mday >= 1) {
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
    }
    // Import writes through a raw INSERT and never reaches TypeValidator, so the canonical form is
    // re-checked here to hold it to the same grammar as create_element/update_element. get_time on
    // its own does not reject an impossible calendar day, so `date_time_format = "%d/%m/%Y"` on a
    // cell "31/02/2024" otherwise stored "2024-02-31T00:00:00" - a value the core's own writers
    // refuse and no binding's date parser can read.
    if (!parsed || !datetime::is_valid_iso8601(buffer)) {
        throw std::runtime_error("Cannot import_csv: Timestamp " + raw_value +
                                 " is not valid. Please provide a valid timestamp with format " +
                                 (format.empty() ? std::string("%Y-%m-%dT%H:%M:%S") : format) + ".");
    }
    return buffer;
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
        // id (PK) and label (NOT NULL by schema convention) are always present; guard defensively.
        if (ids[i] && labels[i]) {
            label_to_id[*labels[i]] = *ids[i];
        }
    }
    return label_to_id;
}

void Database::import_csv(const std::string& collection,
                          const std::string& group,
                          const std::string& path,
                          const CSVOptions& options) {
    impl_->require_collection(collection, "import_csv");

    // Import toggles PRAGMA foreign_keys, which is a no-op inside a transaction,
    // and manages its own transaction — so it cannot run inside an explicit one.
    if (in_transaction()) {
        throw std::runtime_error("Cannot import_csv: transaction already active");
    }

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
    auto csv = read_csv_file(path);
    const auto& csv_cols = csv.header;
    auto db_cols = get_db_columns(execute("SELECT * FROM " + table_name + " LIMIT 0"), group.empty() ? "id" : "");

    // Scalar path: require label column before general column validation
    if (group.empty()) {
        if (std::find(csv_cols.begin(), csv_cols.end(), "label") == csv_cols.end()) {
            throw std::runtime_error("Cannot import_csv: CSV file does not contain a 'label' column.");
        }
    }

    validate_columns_match(csv_cols, db_cols);

    // Validate per-row column count; every csv.rows[row][i] read below relies on it to stay in range
    for (size_t row = 0; row < csv.rows.size(); ++row) {
        const auto& row_data = csv.rows[row];
        if (row_data.size() != csv_cols.size()) {
            throw std::runtime_error("Cannot import_csv: Row " + std::to_string(row + 1) + " has " +
                                     std::to_string(row_data.size()) + " columns, but the header has " +
                                     std::to_string(csv_cols.size()) + ".");
        }
    }

    auto row_count = csv.rows.size();
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

        // Capture label -> id before the delete so re-inserted elements keep their
        // ids; otherwise foreign keys in other tables would silently re-point.
        auto existing_label_to_id = build_label_to_id_map(*this, collection);

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
                const auto& cell = csv.rows[row][csv_col_index[col_name]];
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

                if (type == DataType::Integer && !is_fk && !parse_integer(cell)) {
                    if (options.enum_labels.count(col_name) > 0) {
                        resolve_enum_value(cell, col_name, options);
                    } else {
                        throw std::runtime_error("Cannot import_csv: Invalid integer value '" + cell +
                                                 "' for column '" + col_name + "'.");
                    }
                }

                if (type == DataType::Real && !parse_float(cell)) {
                    throw std::runtime_error("Cannot import_csv: Invalid float value '" + cell + "' for column '" +
                                             col_name + "'.");
                }
            }
        }

        // Data import: disable FK → DELETE → INSERT → enable FK
        execute_raw("PRAGMA foreign_keys = OFF");
        try {
            impl_->begin_transaction();

            execute_raw("DELETE FROM " + collection);

            // Build INSERT statement; id is inserted explicitly (it is not one of db_cols).
            std::string insert_cols = "id";
            std::string insert_placeholders = "?";
            for (const auto& col : db_cols) {
                insert_cols += ", " + col;
                insert_placeholders += ", ?";
            }
            auto insert_sql =
                "INSERT INTO " + collection + " (" + insert_cols + ") VALUES (" + insert_placeholders + ")";

            for (size_t row = 0; row < row_count; ++row) {
                std::vector<Value> parameters;

                // Existing label -> preserved id; new label -> NULL -> fresh id.
                const auto& label = csv.rows[row][csv_col_index.at("label")];
                if (auto it = existing_label_to_id.find(label); it != existing_label_to_id.end()) {
                    parameters.emplace_back(it->second);
                } else {
                    parameters.emplace_back(nullptr);
                }

                for (const auto& col_name : db_cols) {
                    const auto& cell = csv.rows[row][csv_col_index[col_name]];
                    const auto* col_def = table_def->get_column(col_name);
                    auto fk_it = fk_map.find(col_name);
                    auto is_fk = fk_it != fk_map.end();
                    auto is_self_fk = is_fk && fk_it->second.to_table == collection;

                    if (cell.empty()) {
                        parameters.emplace_back(nullptr);
                        continue;
                    }

                    if (is_self_fk) {
                        parameters.emplace_back(nullptr);
                        continue;
                    }

                    if (is_fk) {
                        parameters.emplace_back(fk_label_maps[col_name].at(cell));
                        continue;
                    }

                    auto type = col_def ? col_def->type : DataType::Text;

                    if (type == DataType::DateTime || is_date_time_column(col_name)) {
                        parameters.emplace_back(parse_datetime_import(cell, options.date_time_format));
                        continue;
                    }

                    if (type == DataType::Integer) {
                        if (auto value = parse_integer(cell)) {
                            parameters.emplace_back(*value);
                        } else {
                            parameters.emplace_back(resolve_enum_value(cell, col_name, options));
                        }
                        continue;
                    }

                    if (type == DataType::Real) {
                        parameters.emplace_back(*parse_float(cell));  // validated above
                        continue;
                    }

                    parameters.emplace_back(cell);
                }

                execute(insert_sql, parameters);
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
                        const auto& cell = csv.rows[row][csv_col_index[col_name]];
                        if (cell.empty())
                            continue;

                        const auto& label = csv.rows[row][csv_col_index["label"]];

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

        // Helper: resolve effective DataType for a group column. Only a vector group's vector_index is
        // structural; a set or time-series column of that name keeps its declared type.
        auto get_type = [&](const std::string& col_name) -> DataType {
            if (col_name == "id" || (group_type == GroupTableType::Vector && col_name == "vector_index"))
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
                const auto& id_label = csv.rows[row][id_csv_idx];
                auto vector_index = parse_integer(csv.rows[row][vi_csv_idx]);
                if (!vector_index) {
                    throw std::runtime_error(
                        "Cannot import_csv: Column vector_index must be consecutive, unique and start at 1.");
                }
                element_vector_indices[id_label].push_back(*vector_index);
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
                const auto& cell = csv.rows[row][csv_col_index[col_name]];

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

                // Numbers are checked here, before the DELETE, with the scalar path's messages.
                if (type == DataType::Integer && fk_map.count(col_name) == 0 && !parse_integer(cell)) {
                    if (options.enum_labels.count(col_name) > 0) {
                        resolve_enum_value(cell, col_name, options);
                    } else {
                        throw std::runtime_error("Cannot import_csv: Invalid integer value '" + cell +
                                                 "' for column '" + col_name + "'.");
                    }
                }

                if (type == DataType::Real && !parse_float(cell)) {
                    throw std::runtime_error("Cannot import_csv: Invalid float value '" + cell + "' for column '" +
                                             col_name + "'.");
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
                std::vector<Value> parameters;
                for (const auto& col_name : db_cols) {
                    const auto& cell = csv.rows[row][csv_col_index[col_name]];

                    if (col_name == "id") {
                        parameters.emplace_back(label_to_id.at(cell));
                        continue;
                    }

                    if (cell.empty()) {
                        parameters.emplace_back(nullptr);
                        continue;
                    }

                    if (fk_map.count(col_name) > 0) {
                        parameters.emplace_back(fk_label_maps[col_name].at(cell));
                        continue;
                    }

                    auto type = get_type(col_name);
                    auto is_datetime = (type == DataType::DateTime || is_date_time_column(col_name));

                    if (is_datetime) {
                        parameters.emplace_back(parse_datetime_import(cell, options.date_time_format));
                        continue;
                    }

                    if (type == DataType::Integer) {
                        if (auto value = parse_integer(cell)) {
                            parameters.emplace_back(*value);
                        } else {
                            parameters.emplace_back(resolve_enum_value(cell, col_name, options));
                        }
                        continue;
                    }

                    if (type == DataType::Real) {
                        parameters.emplace_back(*parse_float(cell));  // validated above
                        continue;
                    }

                    parameters.emplace_back(cell);
                }

                execute(insert_sql, parameters);
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