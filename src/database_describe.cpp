#include "database_impl.h"

#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

namespace {

// Integer value distributions are reported only for non-primary-key INTEGER columns whose distinct
// value count does not exceed this threshold (the enum/category case); higher-cardinality columns
// (and float/text/primary-key columns) report coverage counts only.
constexpr int64_t kMaxDistributionCardinality = 64;

// Print a group's value columns in declaration order; time series dimension
// columns are bracketed, vector tables hide their structural vector_index.
void print_group_columns(std::ostream& out, const TableDefinition& table, GroupTableType type) {
    bool first = true;
    for (const auto& col_name : table.column_order) {
        if (col_name == "id" || (type == GroupTableType::Vector && col_name == "vector_index"))
            continue;
        const auto& col = table.columns.at(col_name);
        if (!first)
            out << ", ";
        if (type == GroupTableType::TimeSeries && is_date_time_column(col_name)) {
            out << "[" << col_name << "]";
        } else {
            out << col_name << "(" << data_type_to_string(col.type) << ")";
        }
        first = false;
    }
    out << "\n";
}

std::string group_table_name(const std::string& collection, const std::string& group, GroupTableType type) {
    switch (type) {
    case GroupTableType::Vector:
        return Schema::vector_table_name(collection, group);
    case GroupTableType::Set:
        return Schema::set_table_name(collection, group);
    case GroupTableType::TimeSeries:
        return Schema::time_series_table_name(collection, group);
    default:
        return "";
    }
}

const char* plural(int64_t n) {
    return n == 1 ? "" : "s";
}

// Whitespace/control-byte normalization for UI sidecar text (D-03, and the T-01-03 mitigation):
// maps every byte below 0x20 or equal to 0x7F to a space -- a deliberate superset of D-03's named
// \r/\n/\t that also neutralizes ESC (0x1B) and every other C0 control, so no sidecar string can
// emit an ANSI escape sequence into a terminal rendering the report. Then collapses runs of
// spaces to one and trims both ends. Testing the byte as unsigned char is what keeps UTF-8
// intact: every byte of a multibyte sequence is 0x80 or above and is never touched. An empty
// result means the value is absent and no clause is emitted -- never an empty pair of quotes.
std::string normalize_ui_text(const std::string& raw) {
    std::string collapsed;
    collapsed.reserve(raw.size());
    bool last_was_space = false;
    for (char c : raw) {
        const auto uc = static_cast<unsigned char>(c);
        const bool is_control = uc < 0x20 || uc == 0x7F;
        const char out_char = is_control ? ' ' : c;
        if (out_char == ' ') {
            if (!last_was_space && !collapsed.empty()) {
                collapsed.push_back(' ');
            }
            last_was_space = true;
        } else {
            collapsed.push_back(out_char);
            last_was_space = false;
        }
    }
    while (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }
    return collapsed;
}

// Wraps text in ASCII double quotes with backslash doubled and double quote escaped, and no other
// escaping (D-02). This is what makes every separator unambiguous: arbitrary user text only ever
// appears between an unescaped opening and closing quote.
std::string quote_ui_text(const std::string& text) {
    std::string quoted = "\"";
    quoted.reserve(text.size() + 2);
    for (char c : text) {
        if (c == '\\' || c == '"') {
            quoted.push_back('\\');
        }
        quoted.push_back(c);
    }
    quoted.push_back('"');
    return quoted;
}

// ASCII-lowercase, then keep only a-z/0-9 (D-04). Spelled as an explicit ASCII test rather than
// std::tolower(char): passing a negative char to std::tolower is undefined behavior, and a
// meaningful fraction of corpus strings are non-ASCII UTF-8. squash() therefore drops non-ASCII
// bytes, which biases toward *printing* a label rather than suppressing it -- that is the safe
// direction and is deliberate; do not "fix" it to a Unicode-aware fold.
std::string squash(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        const char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
        if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9')) {
            out.push_back(lower);
        }
    }
    return out;
}

// Appends zero to three "; keyword body" clauses for one scalar attribute (D-01), in the fixed
// order label, enum, tooltip. Each clause carries its own leading "; " so eliding one leaves no
// dangling separator. Only the tooltip clause is conditional on with_tooltip -- label and enum
// are unconditional -- which is what makes describe()'s line a strict prefix of
// describe_collection()'s line by construction (D-07/D-08).
void write_ui_clauses(std::ostream& out, const UiAttribute* meta, const std::string& name, bool with_tooltip) {
    if (!meta) {
        return;
    }

    const std::string normalized_label = normalize_ui_text(meta->label);
    const bool emit_label = !normalized_label.empty() && squash(normalized_label) != squash(name);
    if (emit_label) {
        out << "; label " << quote_ui_text(normalized_label);
    }

    // Enum clause (D-06): never suppressed for redundancy -- it is the one field that cannot be
    // re-derived from the column name. std::map<int64_t, std::string> already iterates in
    // ascending key order, so there is no sort. An entry whose label normalizes to empty is
    // dropped; when no entry survives, the whole clause is omitted (never an empty brace pair).
    std::string enum_body;
    for (const auto& [code, label] : meta->enum_labels) {
        const std::string normalized_entry = normalize_ui_text(label);
        if (normalized_entry.empty()) {
            continue;
        }
        if (!enum_body.empty()) {
            enum_body += ", ";
        }
        enum_body += std::to_string(code) + ": " + quote_ui_text(normalized_entry);
    }
    if (!enum_body.empty()) {
        out << "; enum {" << enum_body << "}";
    }

    if (with_tooltip) {
        const std::string normalized_tooltip = normalize_ui_text(meta->tooltip);
        const bool redundant_vs_name = squash(normalized_tooltip) == squash(name);
        // Compared against the raw sidecar label per D-05, even when the label clause itself was
        // suppressed by D-04.
        const bool redundant_vs_label = squash(normalized_tooltip) == squash(meta->label);
        if (!normalized_tooltip.empty() && !redundant_vs_name && !redundant_vs_label) {
            out << "; tooltip " << quote_ui_text(normalized_tooltip);
        }
    }
}

// Write one collection's structural section (scalars + vector/set/time-series groups).
void write_collection_section(std::ostream& out,
                              const Schema& schema,
                              const std::string& collection,
                              int64_t count,
                              const UiMetadata* ui,
                              bool with_tooltip) {
    out << "Collection: " << collection << " (" << count << " element" << plural(count) << ")\n";

    const auto* table_def = schema.get_table(collection);
    if (table_def && !table_def->column_order.empty()) {
        out << "  Scalars:\n";
        for (const auto& name : table_def->column_order) {
            const auto& col = table_def->columns.at(name);
            out << "    - " << name << " (" << data_type_to_string(col.type) << ")";
            if (col.primary_key) {
                out << " PRIMARY KEY";
            }
            if (col.not_null && !col.primary_key) {
                out << " NOT NULL";
            }
            write_ui_clauses(out, ui ? ui->find(collection, name) : nullptr, name, with_tooltip);
            out << "\n";
        }
    }

    const std::pair<const char*, GroupTableType> sections[] = {
        {"  Vectors:", GroupTableType::Vector},
        {"  Sets:", GroupTableType::Set},
        {"  Time Series:", GroupTableType::TimeSeries},
    };
    for (const auto& [header, type] : sections) {
        auto groups = schema.group_names(collection, type);
        if (groups.empty())
            continue;
        out << header << "\n";
        for (const auto& group_name : groups) {
            const auto* table = schema.get_table(group_table_name(collection, group_name, type));
            out << "    - " << group_name << ": ";
            print_group_columns(out, *table, type);
        }
    }
}

}  // namespace

std::string Database::describe() const {
    impl_->require_schema();

    std::ostringstream out;
    out << "Database: " << impl_->path << "\n";
    out << "Version: " << current_version() << "\n";

    for (const auto& collection : impl_->schema->collection_names()) {
        out << "\n";
        write_collection_section(
            out, *impl_->schema, collection, number_of_elements(collection), &impl_->ui_metadata, false);
    }

    return out.str();
}

std::string Database::describe_collection(const std::string& collection) const {
    impl_->require_collection(collection, "describe_collection");

    std::ostringstream out;
    write_collection_section(
        out, *impl_->schema, collection, number_of_elements(collection), &impl_->ui_metadata, true);
    return out.str();
}

std::string Database::summarize_collection(const std::string& collection) const {
    impl_->require_collection(collection, "summarize_collection");

    const int64_t element_count = number_of_elements(collection);
    const std::string quoted_collection = "\"" + collection + "\"";

    std::ostringstream out;
    out << "Collection: " << collection << " (" << element_count << " element" << plural(element_count) << ")\n";
    out << "  Scalars:\n";

    for (const auto& scalar : list_scalar_attributes(collection)) {
        const std::string quoted_col = "\"" + scalar.name + "\"";

        auto counts = query_int_rows(impl_->db,
                                     "SELECT COUNT(*) - COUNT(" + quoted_col + "), COUNT(" + quoted_col + ") FROM " +
                                         quoted_collection);
        const int64_t null_count = counts[0][0];
        const int64_t non_null_count = counts[0][1];
        out << "    - " << scalar.name << ": " << non_null_count << " non-null, " << null_count << " null";

        // Integer value distribution: only for non-primary-key INTEGER columns whose distinct
        // cardinality is bounded. The LIMIT-based pre-check keeps high-cardinality columns
        // (ids, large FKs) from materializing a huge list.
        if (scalar.data_type == DataType::Integer && !scalar.primary_key) {
            auto distinct = query_int_rows(impl_->db,
                                           "SELECT COUNT(*) FROM (SELECT DISTINCT " + quoted_col + " FROM " +
                                               quoted_collection + " WHERE " + quoted_col + " IS NOT NULL LIMIT ?)",
                                           {kMaxDistributionCardinality + 1});
            if (distinct[0][0] > 0 && distinct[0][0] <= kMaxDistributionCardinality) {
                auto rows =
                    query_int_rows(impl_->db,
                                   "SELECT " + quoted_col + ", COUNT(*) FROM " + quoted_collection + " WHERE " +
                                       quoted_col + " IS NOT NULL GROUP BY " + quoted_col + " ORDER BY " + quoted_col);
                out << "; values {";
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (i != 0) {
                        out << ", ";
                    }
                    out << rows[i][0] << ": " << rows[i][1];
                }
                out << "}";
            }
        }
        out << "\n";
    }

    // Per group: count elements that have at least one row in the group table.
    const std::pair<const char*, GroupTableType> sections[] = {
        {"  Vectors:", GroupTableType::Vector},
        {"  Sets:", GroupTableType::Set},
        {"  Time Series:", GroupTableType::TimeSeries},
    };
    for (const auto& [header, type] : sections) {
        auto groups = impl_->schema->group_names(collection, type);
        if (groups.empty())
            continue;
        out << header << "\n";
        for (const auto& group_name : groups) {
            const auto table = group_table_name(collection, group_name, type);
            const auto non_empty = query_int_rows(impl_->db, "SELECT COUNT(DISTINCT id) FROM \"" + table + "\"")[0][0];
            out << "    - " << group_name << ": " << non_empty << "/" << element_count << " non-empty\n";
        }
    }

    return out.str();
}

}  // namespace quiver
