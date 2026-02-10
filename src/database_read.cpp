#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

std::vector<int64_t> Database::read_scalar_integers(const std::string& collection, const std::string& attribute) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<int64_t> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_integer(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::vector<double> Database::read_scalar_floats(const std::string& collection, const std::string& attribute) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<double> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_float(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::vector<std::string> Database::read_scalar_strings(const std::string& collection, const std::string& attribute) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<std::string> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_string(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::optional<int64_t>
Database::read_scalar_integer_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    auto result = execute(sql, {id});

    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_integer(0);
}

std::optional<double>
Database::read_scalar_float_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    auto result = execute(sql, {id});

    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_float(0);
}

std::optional<std::string>
Database::read_scalar_string_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    auto result = execute(sql, {id});

    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_string(0);
}

std::vector<std::vector<int64_t>> Database::read_vector_integers(const std::string& collection,
                                                                 const std::string& attribute) {
    impl_->require_schema("read vector");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return internal::read_grouped_values_all<int64_t>(execute(sql));
}

std::vector<std::vector<double>> Database::read_vector_floats(const std::string& collection,
                                                              const std::string& attribute) {
    impl_->require_schema("read vector");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return internal::read_grouped_values_all<double>(execute(sql));
}

std::vector<std::vector<std::string>> Database::read_vector_strings(const std::string& collection,
                                                                    const std::string& attribute) {
    impl_->require_schema("read vector");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return internal::read_grouped_values_all<std::string>(execute(sql));
}

std::vector<int64_t>
Database::read_vector_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_schema("read vector");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return internal::read_grouped_values_by_id<int64_t>(execute(sql, {id}));
}

std::vector<double>
Database::read_vector_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_schema("read vector");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return internal::read_grouped_values_by_id<double>(execute(sql, {id}));
}

std::vector<std::string>
Database::read_vector_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_schema("read vector");
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return internal::read_grouped_values_by_id<std::string>(execute(sql, {id}));
}

std::vector<std::vector<int64_t>> Database::read_set_integers(const std::string& collection,
                                                              const std::string& attribute) {
    impl_->require_schema("read set");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return internal::read_grouped_values_all<int64_t>(execute(sql));
}

std::vector<std::vector<double>> Database::read_set_floats(const std::string& collection,
                                                           const std::string& attribute) {
    impl_->require_schema("read set");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return internal::read_grouped_values_all<double>(execute(sql));
}

std::vector<std::vector<std::string>> Database::read_set_strings(const std::string& collection,
                                                                 const std::string& attribute) {
    impl_->require_schema("read set");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return internal::read_grouped_values_all<std::string>(execute(sql));
}

std::vector<int64_t>
Database::read_set_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_schema("read set");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return internal::read_grouped_values_by_id<int64_t>(execute(sql, {id}));
}

std::vector<double>
Database::read_set_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_schema("read set");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return internal::read_grouped_values_by_id<double>(execute(sql, {id}));
}

std::vector<std::string>
Database::read_set_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    impl_->require_schema("read set");
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return internal::read_grouped_values_by_id<std::string>(execute(sql, {id}));
}

std::vector<int64_t> Database::read_element_ids(const std::string& collection) {
    impl_->require_collection(collection, "read element ids");
    auto sql = "SELECT id FROM " + collection + " ORDER BY rowid";
    return internal::read_grouped_values_by_id<int64_t>(execute(sql));
}

}  // namespace quiver
