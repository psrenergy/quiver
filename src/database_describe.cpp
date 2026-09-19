#include "database_impl.h"
#include "ui_config.h"

#include <algorithm>
#include <optional>
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

// The em dash (U+2014) as explicit UTF-8 bytes -- cmake/CompilerOptions.cmake passes no /utf-8 to
// MSVC, so a literal character in this source file is a portability hazard. One named constant
// keeps every render clause and every test assertion on the same bytes.
constexpr const char* kEmDash = "\xE2\x80\x94";

// Writes the `UI config: <path> (locale: <locale>)` header line when a sidecar loaded, nothing
// otherwise -- the guard that makes DESC-05's byte-identical no-sidecar output hold by
// construction (every header/label/clause added by this file gates on `ui` this same way).
void write_ui_header(std::ostream& out, const std::optional<UIConfigSet>& ui) {
    if (!ui) {
        return;
    }
    out << "UI config: " << ui->source_directory << " (locale: " << ui->locale << ")\n";
}

// Appends the collection label clause (` <em-dash> "<label>"`) to a `Collection:` line when `ui`
// names a non-empty label for `collection`. Shared by write_collection_section (describe() /
// describe_collection()) and summarize_collection()'s own Collection: line -- one clause, two
// call sites, never two renderers (D-03).
void append_collection_label(std::ostream& out, const UIConfigSet* ui, const std::string& collection) {
    if (!ui) {
        return;
    }
    const auto it = ui->collections.find(collection);
    if (it == ui->collections.end() || it->second.meta.label.empty()) {
        return;
    }
    out << " " << kEmDash << " \"" << it->second.meta.label << "\"";
}

// Appends the four independently-guarded scalar-line clauses, in the fixed D-02 order: unit,
// hidden, label, vocabulary. `ui` may be null (no sidecar); find_attribute returns nullptr for an
// unconfigured attribute either way, so the whole block is skipped and today's line is unchanged.
void append_scalar_ui_clauses(std::ostream& out, const UIConfigSet* ui, const std::string& collection,
                              const std::string& attribute_name) {
    if (!ui) {
        return;
    }
    const auto* attribute = find_attribute(*ui, collection, attribute_name);
    if (!attribute) {
        return;
    }

    if (!attribute->unit.empty()) {
        out << " [" << attribute->unit << "]";
    }
    if (attribute->hidden) {
        out << " [hidden]";
    }
    if (!attribute->label.empty()) {
        out << " " << kEmDash << " \"" << attribute->label << "\"";
    }
    if (!attribute->vocabulary.empty()) {
        out << " enum " << attribute->vocabulary;
        const auto* vocabulary = find_vocabulary(*ui, attribute->vocabulary);
        if (vocabulary) {
            out << " {";
            for (size_t i = 0; i < vocabulary->size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << (*vocabulary)[i].code << ": " << (*vocabulary)[i].label;
            }
            out << "}";
        } else {
            out << " (undeclared vocabulary)";
        }
    }
}

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

// Write one collection's structural section (scalars + vector/set/time-series groups). `ui` is
// nullable (no sidecar loaded); the same renderer serves describe() and describe_collection() --
// it is never forked into a UI-aware and a UI-unaware variant (D-03).
void write_collection_section(std::ostream& out, const Schema& schema, const UIConfigSet* ui,
                              const std::string& collection, int64_t count) {
    out << "Collection: " << collection << " (" << count << " element" << plural(count) << ")";
    append_collection_label(out, ui, collection);
    out << "\n";

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
            append_scalar_ui_clauses(out, ui, collection, name);
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
    impl_->require_ui_config();

    const UIConfigSet* ui = impl_->ui_config ? &*impl_->ui_config : nullptr;

    std::ostringstream out;
    out << "Database: " << impl_->path << "\n";
    out << "Version: " << current_version() << "\n";
    write_ui_header(out, impl_->ui_config);

    for (const auto& collection : impl_->schema->collection_names()) {
        out << "\n";
        write_collection_section(out, *impl_->schema, ui, collection, number_of_elements(collection));
    }

    return out.str();
}

std::string Database::describe_collection(const std::string& collection) const {
    impl_->require_collection(collection, "describe_collection");
    impl_->require_ui_config();

    const UIConfigSet* ui = impl_->ui_config ? &*impl_->ui_config : nullptr;

    std::ostringstream out;
    write_ui_header(out, impl_->ui_config);
    write_collection_section(out, *impl_->schema, ui, collection, number_of_elements(collection));
    return out.str();
}

std::string Database::summarize_collection(const std::string& collection) const {
    impl_->require_collection(collection, "summarize_collection");
    impl_->require_ui_config();

    const UIConfigSet* ui = impl_->ui_config ? &*impl_->ui_config : nullptr;
    const int64_t element_count = number_of_elements(collection);
    const std::string quoted_collection = "\"" + collection + "\"";

    std::ostringstream out;
    write_ui_header(out, impl_->ui_config);
    out << "Collection: " << collection << " (" << element_count << " element" << plural(element_count) << ")";
    append_collection_label(out, ui, collection);
    out << "\n";
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

                // Enum label lookup (DESC-04): resolved only when a sidecar loaded, the attribute
                // binds a non-empty vocabulary name, and that vocabulary is itself known. Any of
                // those being false leaves `vocabulary` null and the loop below emits nothing
                // extra -- the no-sidecar output is unchanged by construction, not by care.
                const std::vector<UIEnumEntry>* vocabulary = nullptr;
                if (impl_->ui_config) {
                    if (const auto* attribute = find_attribute(*impl_->ui_config, collection, scalar.name);
                        attribute && !attribute->vocabulary.empty()) {
                        vocabulary = find_vocabulary(*impl_->ui_config, attribute->vocabulary);
                    }
                }

                out << "; values {";
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (i != 0) {
                        out << ", ";
                    }
                    const auto code = rows[i][0];
                    out << code << ": " << rows[i][1];
                    if (vocabulary) {
                        auto entry = std::find_if(vocabulary->begin(), vocabulary->end(), [code](const UIEnumEntry& e) {
                            return e.code == code;
                        });
                        if (entry != vocabulary->end()) {
                            out << " (" << entry->label << ")";
                        } else {
                            out << " (undeclared)";
                        }
                    }
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
