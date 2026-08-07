#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

std::vector<std::optional<int64_t>> Database::read_scalar_integers(const std::string& collection,
                                                                   const std::string& attribute) {
    impl_->require_collection(collection, "read_scalar_integers");
    impl_->require_column(collection, attribute, "read_scalar_integers");
    auto sql = "SELECT " + attribute + " FROM " + collection + " ORDER BY rowid";
    return internal::read_column_values_nullable<int64_t>(execute(sql));
}

std::vector<std::optional<double>> Database::read_scalar_floats(const std::string& collection,
                                                                const std::string& attribute) {
    impl_->require_collection(collection, "read_scalar_floats");
    impl_->require_column(collection, attribute, "read_scalar_floats");
    auto sql = "SELECT " + attribute + " FROM " + collection + " ORDER BY rowid";
    return internal::read_column_values_nullable<double>(execute(sql));
}

std::vector<std::optional<std::string>> Database::read_scalar_strings(const std::string& collection,
                                                                      const std::string& attribute) {
    impl_->require_collection(collection, "read_scalar_strings");
    impl_->require_column(collection, attribute, "read_scalar_strings");
    auto sql = "SELECT " + attribute + " FROM " + collection + " ORDER BY rowid";
    return internal::read_column_values_nullable<std::string>(execute(sql));
}

std::optional<int64_t>
Database::read_scalar_integer_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_scalar_integer_by_id");
    impl_->require_column(collection, attribute, "read_scalar_integer_by_id");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    return internal::read_single_value<int64_t>(execute(sql, {id}));
}

std::optional<double>
Database::read_scalar_float_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_scalar_float_by_id");
    impl_->require_column(collection, attribute, "read_scalar_float_by_id");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    return internal::read_single_value<double>(execute(sql, {id}));
}

std::optional<std::string>
Database::read_scalar_string_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_scalar_string_by_id");
    impl_->require_column(collection, attribute, "read_scalar_string_by_id");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    return internal::read_single_value<std::string>(execute(sql, {id}));
}

std::vector<std::vector<int64_t>> Database::read_vector_integers(const std::string& collection,
                                                                 const std::string& attribute) {
    impl_->require_collection(collection, "read_vector_integers");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    impl_->require_column(vector_table, attribute, "read_vector_integers");
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return internal::read_grouped_values_all<int64_t>(execute(sql));
}

std::vector<std::vector<double>> Database::read_vector_floats(const std::string& collection,
                                                              const std::string& attribute) {
    impl_->require_collection(collection, "read_vector_floats");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    impl_->require_column(vector_table, attribute, "read_vector_floats");
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return internal::read_grouped_values_all<double>(execute(sql));
}

std::vector<std::vector<std::string>> Database::read_vector_strings(const std::string& collection,
                                                                    const std::string& attribute) {
    impl_->require_collection(collection, "read_vector_strings");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    impl_->require_column(vector_table, attribute, "read_vector_strings");
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return internal::read_grouped_values_all<std::string>(execute(sql));
}

std::vector<int64_t>
Database::read_vector_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_vector_integers_by_id");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    impl_->require_column(vector_table, attribute, "read_vector_integers_by_id");
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return internal::read_column_values<int64_t>(execute(sql, {id}));
}

std::vector<double>
Database::read_vector_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_vector_floats_by_id");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    impl_->require_column(vector_table, attribute, "read_vector_floats_by_id");
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return internal::read_column_values<double>(execute(sql, {id}));
}

std::vector<std::string>
Database::read_vector_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_vector_strings_by_id");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    impl_->require_column(vector_table, attribute, "read_vector_strings_by_id");
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return internal::read_column_values<std::string>(execute(sql, {id}));
}

std::vector<std::vector<int64_t>> Database::read_set_integers(const std::string& collection,
                                                              const std::string& attribute) {
    impl_->require_collection(collection, "read_set_integers");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    impl_->require_column(set_table, attribute, "read_set_integers");
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return internal::read_grouped_values_all<int64_t>(execute(sql));
}

std::vector<std::vector<double>> Database::read_set_floats(const std::string& collection,
                                                           const std::string& attribute) {
    impl_->require_collection(collection, "read_set_floats");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    impl_->require_column(set_table, attribute, "read_set_floats");
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return internal::read_grouped_values_all<double>(execute(sql));
}

std::vector<std::vector<std::string>> Database::read_set_strings(const std::string& collection,
                                                                 const std::string& attribute) {
    impl_->require_collection(collection, "read_set_strings");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    impl_->require_column(set_table, attribute, "read_set_strings");
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return internal::read_grouped_values_all<std::string>(execute(sql));
}

std::vector<int64_t>
Database::read_set_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_set_integers_by_id");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    impl_->require_column(set_table, attribute, "read_set_integers_by_id");
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return internal::read_column_values<int64_t>(execute(sql, {id}));
}

std::vector<double>
Database::read_set_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_set_floats_by_id");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    impl_->require_column(set_table, attribute, "read_set_floats_by_id");
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return internal::read_column_values<double>(execute(sql, {id}));
}

std::vector<std::string>
Database::read_set_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read_set_strings_by_id");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    impl_->require_column(set_table, attribute, "read_set_strings_by_id");
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return internal::read_column_values<std::string>(execute(sql, {id}));
}

namespace {

std::string group_select_sql(const GroupMetadata& metadata, const std::string& table, const std::string& order_by) {
    std::string sql = "SELECT ";
    for (size_t i = 0; i < metadata.value_columns.size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += metadata.value_columns[i].name;
    }
    sql += " FROM " + table + " WHERE id = ? ORDER BY " + order_by;
    return sql;
}

std::vector<std::map<std::string, Value>> group_rows_from_result(const GroupMetadata& metadata, const Result& result) {
    std::vector<std::map<std::string, Value>> rows;
    rows.reserve(result.row_count());
    for (size_t row_idx = 0; row_idx < result.row_count(); ++row_idx) {
        std::map<std::string, Value> row;
        for (size_t col_idx = 0; col_idx < metadata.value_columns.size(); ++col_idx) {
            row[metadata.value_columns[col_idx].name] = result[row_idx][col_idx];
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

}  // namespace

std::vector<std::map<std::string, Value>>
Database::read_vector_group_by_id(const std::string& collection, const std::string& group, int64_t id) {
    impl_->require_collection(collection, "read_vector_group_by_id");
    auto metadata = get_vector_metadata(collection, group);
    if (metadata.value_columns.empty()) {
        return {};
    }
    auto sql = group_select_sql(metadata, Schema::vector_table_name(collection, group), "vector_index");
    return group_rows_from_result(metadata, execute(sql, {id}));
}

std::vector<std::map<std::string, Value>>
Database::read_set_group_by_id(const std::string& collection, const std::string& group, int64_t id) {
    impl_->require_collection(collection, "read_set_group_by_id");
    auto metadata = get_set_metadata(collection, group);
    if (metadata.value_columns.empty()) {
        return {};
    }
    auto sql = group_select_sql(metadata, Schema::set_table_name(collection, group), "rowid");
    return group_rows_from_result(metadata, execute(sql, {id}));
}

std::vector<int64_t> Database::read_element_ids(const std::string& collection) {
    impl_->require_collection(collection, "read_element_ids");
    auto sql = "SELECT id FROM " + collection + " ORDER BY rowid";
    return internal::read_column_values<int64_t>(execute(sql));
}

int64_t Database::read_element_count(const std::string& collection) const {
    impl_->require_collection(collection, "read_element_count");
    return query_int_rows(impl_->db, "SELECT COUNT(*) FROM \"" + collection + "\"")[0][0];
}

}  // namespace quiver
