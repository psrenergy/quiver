#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"

extern "C" {

// Read scalar attributes

QUIVER_C_API quiver_error_t quiver_database_read_scalar_integers(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t** out_values,
                                                                 size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        return read_scalars_impl(db->db.read_scalar_integers(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_floats(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               double** out_values,
                                                               size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        return read_scalars_impl(db->db.read_scalar_floats(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_strings(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                char*** out_values,
                                                                size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        return copy_strings_to_c(db->db.read_scalar_strings(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Free scalar arrays

QUIVER_C_API quiver_error_t quiver_database_free_integer_array(int64_t* values) {
    QUIVER_REQUIRE(values);

    delete[] values;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_free_float_array(double* values) {
    QUIVER_REQUIRE(values);

    delete[] values;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_free_string_array(char** values, size_t count) {
    QUIVER_REQUIRE(values);

    for (size_t i = 0; i < count; ++i) {
        delete[] values[i];
    }
    delete[] values;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}

// Read vector attributes

QUIVER_C_API quiver_error_t quiver_database_read_vector_integers(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t*** out_vectors,
                                                                 size_t** out_sizes,
                                                                 size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_vectors, out_sizes, out_count);

    try {
        return read_vectors_impl(db->db.read_vector_integers(collection, attribute), out_vectors, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_floats(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               double*** out_vectors,
                                                               size_t** out_sizes,
                                                               size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_vectors, out_sizes, out_count);

    try {
        return read_vectors_impl(db->db.read_vector_floats(collection, attribute), out_vectors, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_strings(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                char**** out_vectors,
                                                                size_t** out_sizes,
                                                                size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_vectors, out_sizes, out_count);

    try {
        auto vectors = db->db.read_vector_strings(collection, attribute);
        *out_count = vectors.size();
        if (vectors.empty()) {
            *out_vectors = nullptr;
            *out_sizes = nullptr;
            return QUIVER_OK;
        }
        *out_vectors = new char**[vectors.size()];
        *out_sizes = new size_t[vectors.size()];
        for (size_t i = 0; i < vectors.size(); ++i) {
            (*out_sizes)[i] = vectors[i].size();
            if (vectors[i].empty()) {
                (*out_vectors)[i] = nullptr;
            } else {
                (*out_vectors)[i] = new char*[vectors[i].size()];
                for (size_t j = 0; j < vectors[i].size(); ++j) {
                    (*out_vectors)[i][j] = new_c_str(vectors[i][j]);
                }
            }
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Free vector arrays

QUIVER_C_API quiver_error_t quiver_database_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count) {
    QUIVER_REQUIRE(vectors, sizes);

    return free_vectors_impl(vectors, sizes, count);
}

QUIVER_C_API quiver_error_t quiver_database_free_float_vectors(double** vectors, size_t* sizes, size_t count) {
    QUIVER_REQUIRE(vectors, sizes);

    return free_vectors_impl(vectors, sizes, count);
}

QUIVER_C_API quiver_error_t quiver_database_free_string_vectors(char*** vectors, size_t* sizes, size_t count) {
    QUIVER_REQUIRE(vectors, sizes);

    for (size_t i = 0; i < count; ++i) {
        if (vectors[i]) {
            for (size_t j = 0; j < sizes[i]; ++j) {
                delete[] vectors[i][j];
            }
            delete[] vectors[i];
        }
    }
    delete[] vectors;
    delete[] sizes;
    return QUIVER_OK;
}

// Set read functions (reuse vector helpers since sets have same return structure)

QUIVER_C_API quiver_error_t quiver_database_read_set_integers(quiver_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t*** out_sets,
                                                              size_t** out_sizes,
                                                              size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_sets, out_sizes, out_count);

    try {
        return read_vectors_impl(db->db.read_set_integers(collection, attribute), out_sets, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_floats(quiver_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            double*** out_sets,
                                                            size_t** out_sizes,
                                                            size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_sets, out_sizes, out_count);

    try {
        return read_vectors_impl(db->db.read_set_floats(collection, attribute), out_sets, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_strings(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             char**** out_sets,
                                                             size_t** out_sizes,
                                                             size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_sets, out_sizes, out_count);

    try {
        auto sets = db->db.read_set_strings(collection, attribute);
        *out_count = sets.size();
        if (sets.empty()) {
            *out_sets = nullptr;
            *out_sizes = nullptr;
            return QUIVER_OK;
        }
        *out_sets = new char**[sets.size()];
        *out_sizes = new size_t[sets.size()];
        for (size_t i = 0; i < sets.size(); ++i) {
            (*out_sizes)[i] = sets[i].size();
            if (sets[i].empty()) {
                (*out_sets)[i] = nullptr;
            } else {
                (*out_sets)[i] = new char*[sets[i].size()];
                for (size_t j = 0; j < sets[i].size(); ++j) {
                    (*out_sets)[i][j] = new_c_str(sets[i][j]);
                }
            }
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Read scalar by ID functions

QUIVER_C_API quiver_error_t quiver_database_read_scalar_integer_by_id(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* attribute,
                                                                      int64_t id,
                                                                      int64_t* out_value,
                                                                      int* out_has_value) {
    QUIVER_REQUIRE(db, collection, attribute, out_value, out_has_value);

    try {
        auto result = db->db.read_scalar_integer_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_float_by_id(quiver_database_t* db,
                                                                    const char* collection,
                                                                    const char* attribute,
                                                                    int64_t id,
                                                                    double* out_value,
                                                                    int* out_has_value) {
    QUIVER_REQUIRE(db, collection, attribute, out_value, out_has_value);

    try {
        auto result = db->db.read_scalar_float_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_string_by_id(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* attribute,
                                                                     int64_t id,
                                                                     char** out_value,
                                                                     int* out_has_value) {
    QUIVER_REQUIRE(db, collection, attribute, out_value, out_has_value);

    try {
        auto result = db->db.read_scalar_string_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = new_c_str(*result);
            *out_has_value = 1;
        } else {
            *out_value = nullptr;
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Read vector by ID functions

QUIVER_C_API quiver_error_t quiver_database_read_vector_integers_by_id(quiver_database_t* db,
                                                                       const char* collection,
                                                                       const char* attribute,
                                                                       int64_t id,
                                                                       int64_t** out_values,
                                                                       size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        auto values = db->db.read_vector_integers_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_floats_by_id(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* attribute,
                                                                     int64_t id,
                                                                     double** out_values,
                                                                     size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        auto values = db->db.read_vector_floats_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_strings_by_id(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* attribute,
                                                                      int64_t id,
                                                                      char*** out_values,
                                                                      size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        return copy_strings_to_c(db->db.read_vector_strings_by_id(collection, attribute, id), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Read set by ID functions

QUIVER_C_API quiver_error_t quiver_database_read_set_integers_by_id(quiver_database_t* db,
                                                                    const char* collection,
                                                                    const char* attribute,
                                                                    int64_t id,
                                                                    int64_t** out_values,
                                                                    size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        auto values = db->db.read_set_integers_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_floats_by_id(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  double** out_values,
                                                                  size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        auto values = db->db.read_set_floats_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_strings_by_id(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   int64_t id,
                                                                   char*** out_values,
                                                                   size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        return copy_strings_to_c(db->db.read_set_strings_by_id(collection, attribute, id), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Read element Ids

QUIVER_C_API quiver_error_t quiver_database_read_element_ids(quiver_database_t* db,
                                                             const char* collection,
                                                             int64_t** out_ids,
                                                             size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_ids, out_count);

    try {
        return read_scalars_impl(db->db.read_element_ids(collection), out_ids, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
