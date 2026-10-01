#include "csv/csv_read.h"
#include "database_impl.h"
#include "database_internal.h"
#include "quiver/options.h"
#include "schema.h"
#include "utils/datetime.h"
#include "utils/number.h"
#include "utils/string.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
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

// Whole-cell integer parse: nullopt unless every character is consumed, so "1.5" is not an integer
// (stoll alone accepts any valid prefix); REAL cells go through its float counterpart,
// utils::parse_float. Import writes through a raw INSERT, so this is where the core typing policy --
// a float into an INTEGER column is rejected -- reaches CSV text.
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

// Lowercase a string for case-insensitive comparison.
static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

// csv-parser keeps a closing quote that is followed by ordinary text as a literal and stays in
// quoted mode, where separators and line breaks are plain text -- so `"a" ,b\n"c",d` parses as ONE
// row whose cell count can still match the header, and an unterminated quote swallows the rest of
// the file. db:read_csv tolerates that, but import deletes before it writes, so a silently
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
    // Import writes through a raw INSERT and never reaches validate_value, so the canonical form is
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

// The one CSV cell -> Value conversion, for the scalar and the group path alike. Import runs it over
// every row before its first write, so converting a row is what validates it. `col` is null for a
// column SELECT * lists but the schema does not (PRAGMA table_info omits generated columns): it
// converts as nullable TEXT, and the INSERT then reports it. `fk` and `fk_labels` are set for a
// foreign key resolved by label here; a self-reference is the caller's (NULL now, resolved in a
// second pass once every row exists).
static Value convert_cell(const std::string& cell,
                          const std::string& col_name,
                          const ColumnDefinition* col,
                          const ForeignKey* fk,
                          const std::unordered_map<std::string, int64_t>* fk_labels,
                          const CSVOptions& options) {
    if (cell.empty()) {
        if (col && col->not_null) {
            throw std::runtime_error("Cannot import_csv: Column " + col_name + " cannot be NULL.");
        }
        return nullptr;
    }
    if (fk) {
        const auto it = fk_labels->find(cell);
        if (it == fk_labels->end()) {
            throw std::runtime_error("Cannot import_csv: Could not find an existing element from collection " +
                                     fk->to_table + " with label " + cell +
                                     ".\nCreate the element before referencing it.");
        }
        return it->second;
    }
    switch (col ? col->type : DataType::Text) {
    case DataType::DateTime:
        return parse_datetime_import(cell, options.date_time_format);
    case DataType::Integer:
        if (const auto value = parse_integer(cell)) {
            return *value;
        }
        if (options.enum_labels.count(col_name) > 0) {
            return resolve_enum_value(cell, col_name, options);
        }
        throw std::runtime_error("Cannot import_csv: Invalid integer value '" + cell + "' for column '" + col_name +
                                 "'.");
    case DataType::Real:
        if (const auto value = utils::parse_float(cell)) {
            return *value;
        }
        throw std::runtime_error("Cannot import_csv: Invalid float value '" + cell + "' for column '" + col_name +
                                 "'.");
    default:
        return cell;
    }
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

    // Import manages its own transaction (a raw BEGIN, and a ROLLBACK on any error), so inside a
    // caller's transaction its BEGIN would fail and that ROLLBACK would discard the caller's work.
    if (in_transaction()) {
        throw std::runtime_error("Cannot import_csv: transaction already active");
    }

    // Resolve target table name
    auto table_name = collection;
    GroupTableType group_type{};
    if (!group.empty()) {
        const auto match = impl_->schema->find_group_table(collection, group);
        if (!match) {
            throw std::runtime_error("Cannot import_csv: group not found: '" + group + "' in collection '" +
                                     collection + "'");
        }
        table_name = match->table_name;
        group_type = match->type;
    }
    const auto& table_def = *impl_->schema->get_table(table_name);

    // Read CSV, validate columns against DB schema, handle empty CSV
    auto csv = read_csv_file(path);
    const auto& csv_cols = csv.header;
    auto db_cols =
        get_db_columns(impl_->execute("SELECT * FROM " + table_name + " LIMIT 0"), group.empty() ? "id" : "");

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

    // Find the CSV column index for each db column
    std::unordered_map<std::string, size_t> csv_col_index;
    for (size_t i = 0; i < csv_cols.size(); ++i) {
        csv_col_index[csv_cols[i]] = i;
    }

    // Every row, converted by convert_cell before the first write and in its INSERT's column order: a
    // bad cell throws before anything changes.
    std::vector<std::vector<Value>> rows;
    rows.reserve(row_count);

    // Scalar path only, read by its write steps below.
    std::unordered_map<std::string, int64_t> existing_label_to_id;
    std::set<std::string> csv_labels;
    std::vector<std::string> self_fk_cols;

    if (group.empty()) {
        // ================================================================
        // Scalar import path
        // ================================================================

        // Capture label -> id before any write: it decides which elements are updated in place (keeping
        // their ids, so relations to them hold) and which ones the CSV omits and are deleted.
        existing_label_to_id = build_label_to_id_map(*this, collection);

        // Build FK map: column_name -> ForeignKey
        std::unordered_map<std::string, ForeignKey> fk_map;
        for (const auto& fk : table_def.foreign_keys) {
            fk_map[fk.from_column] = fk;
        }

        // Build FK label→id lookup maps for non-self foreign keys; self-references wait for the second pass
        std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> fk_label_maps;
        for (const auto& [col_name, fk] : fk_map) {
            if (fk.to_table != collection) {
                fk_label_maps[col_name] = build_label_to_id_map(*this, fk.to_table);
            } else {
                self_fk_cols.push_back(col_name);
            }
        }

        for (const auto& cells : csv.rows) {
            // A label may appear only once: an existing label is written in place by its id below, so a
            // repeat would silently overwrite the first row.
            const auto& label = cells[csv_col_index.at("label")];
            if (!csv_labels.insert(label).second) {
                throw std::runtime_error("Cannot import_csv: There are duplicate entries in the CSV file.");
            }

            auto& row = rows.emplace_back();
            // Existing label -> preserved id; new label -> NULL -> fresh id.
            if (auto it = existing_label_to_id.find(label); it != existing_label_to_id.end()) {
                row.emplace_back(it->second);
            } else {
                row.emplace_back(nullptr);
            }

            for (const auto& col_name : db_cols) {
                const auto& cell = cells[csv_col_index.at(col_name)];
                const auto* col_def = table_def.get_column(col_name);
                const auto fk_it = fk_map.find(col_name);
                const auto is_fk = fk_it != fk_map.end();
                const auto is_self_fk = is_fk && fk_it->second.to_table == collection;

                if (is_self_fk && !cell.empty()) {
                    // NULL for now: the second pass sets it by label, once every CSV row exists.
                    row.emplace_back(nullptr);
                } else if (is_fk && !is_self_fk) {
                    row.push_back(
                        convert_cell(cell, col_name, col_def, &fk_it->second, &fk_label_maps.at(col_name), options));
                } else {
                    row.push_back(convert_cell(cell, col_name, col_def, nullptr, nullptr, options));
                }
            }
        }
    } else {
        // ================================================================
        // Group import path
        // ================================================================

        auto label_to_id = build_label_to_id_map(*this, collection);

        // A time series group needs its date dimension, as export and the readers do. This also refuses
        // the <collection>_time_series_files table, which group "files" names: it has no key, and a
        // header-only CSV would otherwise clear it.
        if (group_type == GroupTableType::TimeSeries) {
            internal::find_dimension_column(table_def);
        }

        // Build FK map (excluding the parent id FK)
        std::unordered_map<std::string, ForeignKey> fk_map;
        for (const auto& fk : table_def.foreign_keys) {
            if (fk.from_column != "id") {
                fk_map[fk.from_column] = fk;
            }
        }

        // Build FK label→id lookup maps for non-id foreign keys
        std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> fk_label_maps;
        for (const auto& [col_name, fk] : fk_map) {
            fk_label_maps[col_name] = build_label_to_id_map(*this, fk.to_table);
        }

        // vector_index consecutive check
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

        for (const auto& cells : csv.rows) {
            auto& row = rows.emplace_back();
            for (const auto& col_name : db_cols) {
                const auto& cell = cells[csv_col_index.at(col_name)];

                if (col_name == "id") {
                    const auto it = label_to_id.find(cell);
                    if (it == label_to_id.end()) {
                        throw std::runtime_error("Cannot import_csv: Element with id " + cell +
                                                 " does not exist in collection " + collection + ".");
                    }
                    row.emplace_back(it->second);
                    continue;
                }

                const auto fk_it = fk_map.find(col_name);
                const auto is_fk = fk_it != fk_map.end();
                row.push_back(convert_cell(cell,
                                           col_name,
                                           table_def.get_column(col_name),
                                           is_fk ? &fk_it->second : nullptr,
                                           is_fk ? &fk_label_maps.at(col_name) : nullptr,
                                           options));
            }
        }
    }

    // Data import, in one transaction and with foreign keys on throughout, so every ON DELETE action
    // fires exactly as it does for delete_element.
    try {
        impl_->begin_transaction();

        if (group.empty()) {
            // Clear self-references first: an ON DELETE CASCADE self-reference from a kept element to
            // an omitted one would otherwise delete the kept element along with it. The second pass
            // below restores them from the CSV's labels.
            for (const auto& col_name : self_fk_cols) {
                impl_->execute("UPDATE " + collection + " SET " + col_name + " = NULL");
            }

            // Delete the elements the CSV omits: their group rows cascade away and every relation to
            // them follows its ON DELETE action (SET NULL clears it, CASCADE deletes the row).
            for (const auto& [label, id] : existing_label_to_id) {
                if (!csv_labels.contains(label)) {
                    impl_->execute("DELETE FROM " + collection + " WHERE id = ?", {id});
                }
            }

            // A cascade can still reach a kept element through another collection (a cycle of ON DELETE
            // CASCADE relations), and the upsert below would bring it back without its group rows.
            // Refuse instead; the rollback restores everything.
            auto remaining = read_element_ids(collection);
            std::set<int64_t> remaining_ids(remaining.begin(), remaining.end());
            for (const auto& [label, id] : existing_label_to_id) {
                if (csv_labels.contains(label) && !remaining_ids.contains(id)) {
                    throw std::runtime_error("Cannot import_csv: Deleting the elements the CSV omits would also delete "
                                             "element '" +
                                             label + "' through an ON DELETE CASCADE chain.");
                }
            }

            // Write every row by id (id is not one of db_cols): an existing label's id updates its row
            // in place, keeping its group rows and every relation pointing at it; a new label's NULL id
            // gets a fresh one. Never INSERT OR REPLACE: its implicit delete would cascade as well.
            std::string insert_cols = "id";
            std::string insert_placeholders = "?";
            std::string update_assignments;
            for (const auto& col : db_cols) {
                insert_cols += ", " + col;
                insert_placeholders += ", ?";
                if (!update_assignments.empty()) {
                    update_assignments += ", ";
                }
                update_assignments += col + " = excluded." + col;
            }
            auto insert_sql = "INSERT INTO " + collection + " (" + insert_cols + ") VALUES (" + insert_placeholders +
                              ") ON CONFLICT(id) DO UPDATE SET " + update_assignments;

            for (const auto& row : rows) {
                impl_->execute(insert_sql, row);
            }

            // Second pass: resolve self-referencing FKs, now that every CSV row exists
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

                        impl_->execute("UPDATE " + collection + " SET " + col_name + " = ? WHERE id = ?",
                                       {self_label_to_id.at(cell), self_label_to_id.at(label)});
                    }
                }
            }
        } else {
            // DELETE → INSERT. Every id and FK cell was resolved to an existing element above, so the
            // inserts need no foreign-key relaxation.
            impl_->execute_raw("DELETE FROM " + table_name);

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

            for (const auto& row : rows) {
                impl_->execute(insert_sql, row);
            }
        }

        impl_->commit();
    } catch (const std::exception& e) {
        impl_->rollback();

        std::string msg = e.what();
        if (msg.find("UNIQUE constraint") != std::string::npos) {
            throw std::runtime_error("Cannot import_csv: There are duplicate entries in the CSV file.");
        }
        throw;
    }
}

}  // namespace quiver
