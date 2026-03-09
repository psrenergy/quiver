#ifndef QUIVER_C_BINARY_METADATA_H
#define QUIVER_C_BINARY_METADATA_H

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Time frequency enum (maps to quiver::TimeFrequency)
typedef enum {
    QUIVER_TIME_FREQUENCY_YEARLY = 0,
    QUIVER_TIME_FREQUENCY_MONTHLY = 1,
    QUIVER_TIME_FREQUENCY_WEEKLY = 2,
    QUIVER_TIME_FREQUENCY_DAILY = 3,
    QUIVER_TIME_FREQUENCY_HOURLY = 4,
} quiver_time_frequency_t;

// Flat struct (maps to quiver::TimeProperties)
typedef struct {
    quiver_time_frequency_t frequency;
    int64_t initial_value;
    int64_t parent_dimension_index;
} quiver_time_properties_t;

// Flat struct (maps to quiver::Dimension)
// name is allocated via new_c_str, free with quiver_binary_metadata_free_dimension
typedef struct {
    const char* name;
    int64_t size;
    int is_time_dimension;
    quiver_time_properties_t time_properties;  // valid only when is_time_dimension != 0
} quiver_dimension_t;

// Opaque handle type
typedef struct quiver_binary_metadata quiver_binary_metadata_t;

// Lifecycle
QUIVER_C_API quiver_error_t quiver_binary_metadata_create(quiver_binary_metadata_t** out);
QUIVER_C_API quiver_error_t quiver_binary_metadata_free(quiver_binary_metadata_t* md);

// Factories
QUIVER_C_API quiver_error_t quiver_binary_metadata_from_toml(const char* toml, quiver_binary_metadata_t** out);

typedef struct quiver_element quiver_element_t;
QUIVER_C_API quiver_error_t quiver_binary_metadata_from_element(quiver_element_t* el, quiver_binary_metadata_t** out);

// Serialization
QUIVER_C_API quiver_error_t quiver_binary_metadata_to_toml(quiver_binary_metadata_t* md, char** out_toml);

// Builders
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_initial_datetime(quiver_binary_metadata_t* md, const char* iso8601);
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_unit(quiver_binary_metadata_t* md, const char* unit);
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_version(quiver_binary_metadata_t* md, const char* version);
QUIVER_C_API quiver_error_t quiver_binary_metadata_set_labels(quiver_binary_metadata_t* md,
                                                            const char* const* labels,
                                                            size_t count);
QUIVER_C_API quiver_error_t quiver_binary_metadata_add_dimension(quiver_binary_metadata_t* md,
                                                               const char* name,
                                                               int64_t size);
QUIVER_C_API quiver_error_t quiver_binary_metadata_add_time_dimension(quiver_binary_metadata_t* md,
                                                                    const char* name,
                                                                    int64_t size,
                                                                    const char* frequency);

// Getters
QUIVER_C_API quiver_error_t quiver_binary_metadata_get_unit(quiver_binary_metadata_t* md, char** out);
QUIVER_C_API quiver_error_t quiver_binary_metadata_get_version(quiver_binary_metadata_t* md, char** out);
QUIVER_C_API quiver_error_t quiver_binary_metadata_get_initial_datetime(quiver_binary_metadata_t* md, char** out);
QUIVER_C_API quiver_error_t quiver_binary_metadata_get_number_of_time_dimensions(quiver_binary_metadata_t* md,
                                                                               int64_t* out);
QUIVER_C_API quiver_error_t quiver_binary_metadata_get_labels(quiver_binary_metadata_t* md, char*** out, size_t* out_count);
QUIVER_C_API quiver_error_t quiver_binary_metadata_get_dimension_count(quiver_binary_metadata_t* md, size_t* out);
QUIVER_C_API quiver_error_t quiver_binary_metadata_get_dimension(quiver_binary_metadata_t* md,
                                                               size_t index,
                                                               quiver_dimension_t* out);

// Free helpers
QUIVER_C_API quiver_error_t quiver_binary_metadata_free_string(char* str);
QUIVER_C_API quiver_error_t quiver_binary_metadata_free_string_array(char** strs, size_t count);
QUIVER_C_API quiver_error_t quiver_binary_metadata_free_dimension(quiver_dimension_t* dim);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_BINARY_METADATA_H
