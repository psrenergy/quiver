#ifndef QUIVER_C_EXPRESSION_H
#define QUIVER_C_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct quiver_expression quiver_expression_t;

// Binary operation kind
typedef enum {
    QUIVER_EXPRESSION_OP_ADD = 0,
    QUIVER_EXPRESSION_OP_SUBTRACT = 1,
    QUIVER_EXPRESSION_OP_MULTIPLY = 2,
    QUIVER_EXPRESSION_OP_DIVIDE = 3,
} quiver_expression_op_t;

// Construction
QUIVER_C_API quiver_error_t quiver_expression_from_file(quiver_binary_file_t* file, quiver_expression_t** out);

// Lifecycle
QUIVER_C_API quiver_error_t quiver_expression_close(quiver_expression_t* expression);

// Operations
QUIVER_C_API quiver_error_t quiver_expression_apply(quiver_expression_op_t op,
                                                    quiver_expression_t* lhs,
                                                    quiver_expression_t* rhs,
                                                    quiver_expression_t** out);
QUIVER_C_API quiver_error_t quiver_expression_apply_scalar_right(quiver_expression_op_t op,
                                                                 quiver_expression_t* lhs,
                                                                 double rhs,
                                                                 quiver_expression_t** out);
QUIVER_C_API quiver_error_t quiver_expression_apply_scalar_left(quiver_expression_op_t op,
                                                                double lhs,
                                                                quiver_expression_t* rhs,
                                                                quiver_expression_t** out);

// Materialization
QUIVER_C_API quiver_error_t quiver_expression_save(quiver_expression_t* expression, const char* path);

// Metadata access
QUIVER_C_API quiver_error_t quiver_expression_get_metadata(quiver_expression_t* expression,
                                                           quiver_binary_metadata_t** out);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_EXPRESSION_H
