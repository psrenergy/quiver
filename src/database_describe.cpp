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

// Run a read-only query that yields integer columns and collect the rows. describe()/
// summarize_collection() are const, but Database::execute() is not; this mirrors current_version()'s
// direct prepare/step on impl_->db so the const methods can issue their own counting/aggregate SQL.
// Only integer parameters are needed (LIMIT bounds), and every column read is an int64.
std::vector<std::vector<int64_t>>
query_int_rows(sqlite3* db, const std::string& sql, const std::vector<int64_t>& parameters = {}) {
    sqlite3_stmt* raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    StmtPtr stmt(raw_stmt, sqlite3_finalize);

    for (size_t i = 0; i < parameters.size(); ++i) {
        sqlite3_bind_int64(stmt.get(), static_cast<int>(i + 1), parameters[i]);
    }

    const int col_count = sqlite3_column_count(stmt.get());
    std::vector<std::vector<int64_t>> rows;
    int rc = 0;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        std::vector<int64_t> row;
        row.reserve(col_count);
        for (int c = 0; c < col_count; ++c) {
            row.push_back(sqlite3_column_int64(stmt.get(), c));
        }
        rows.push_back(std::move(row));
    }
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(db)));
    }
    return rows;
}

// COUNT(*) of a quoted identifier table.
int64_t element_count_of(sqlite3* db, const std::string& collection) {
    return query_int_rows(db, "SELECT COUNT(*) FROM \"" + collection + "\"")[0][0];
}

const char* plural(int64_t n) {
    return n == 1 ? "" : "s";
}

// Write one collection's structural section (scalars + vector/set/time-series groups).
void write_collection_section(std::ostream& out, const Schema& schema, const std::string& collection, int64_t count) {
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
    impl_->require_schema("describe");

    std::ostringstream out;
    out << "Database: " << impl_->path << "\n";
    out << "Version: " << current_version() << "\n";

    for (const auto& collection : impl_->schema->collection_names()) {
        out << "\n";
        write_collection_section(out, *impl_->schema, collection, element_count_of(impl_->db, collection));
    }

    return out.str();
}

std::string Database::describe_collection(const std::string& collection) const {
    impl_->require_collection(collection, "describe_collection");

    std::ostringstream out;
    write_collection_section(out, *impl_->schema, collection, element_count_of(impl_->db, collection));
    return out.str();
}

std::string Database::summarize_collection(const std::string& collection) const {
    impl_->require_collection(collection, "summarize_collection");

    const int64_t element_count = element_count_of(impl_->db, collection);
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
