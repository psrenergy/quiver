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
    QUIVER_EXPRESSION_OPERATION_ADD = 0,
    QUIVER_EXPRESSION_OPERATION_SUBTRACT = 1,
    QUIVER_EXPRESSION_OPERATION_MULTIPLY = 2,
    QUIVER_EXPRESSION_OPERATION_DIVIDE = 3,
} quiver_expression_operation_t;

// Unary operation kind
typedef enum {
    QUIVER_EXPRESSION_UNARY_OPERATION_NEGATE = 0,
    QUIVER_EXPRESSION_UNARY_OPERATION_ABS = 1,
    QUIVER_EXPRESSION_UNARY_OPERATION_SQRT = 2,
    QUIVER_EXPRESSION_UNARY_OPERATION_LOG = 3,
    QUIVER_EXPRESSION_UNARY_OPERATION_EXP = 4,
} quiver_expression_unary_operation_t;

// Ternary operation kind
typedef enum {
    QUIVER_EXPRESSION_TERNARY_OPERATION_IFELSE = 0,
} quiver_expression_ternary_operation_t;

// Construction
QUIVER_C_API quiver_error_t quiver_expression_from_file(quiver_binary_file_t* file, quiver_expression_t** out);

// Lifecycle
QUIVER_C_API quiver_error_t quiver_expression_close(quiver_expression_t* expression);

// Operations
QUIVER_C_API quiver_error_t quiver_expression_apply(quiver_expression_operation_t operation,
                                                    quiver_expression_t* lhs,
                                                    quiver_expression_t* rhs,
                                                    quiver_expression_t** out);
QUIVER_C_API quiver_error_t quiver_expression_apply_scalar_right(quiver_expression_operation_t operation,
                                                                 quiver_expression_t* lhs,
                                                                 double rhs,
                                                                 quiver_expression_t** out);
QUIVER_C_API quiver_error_t quiver_expression_apply_scalar_left(quiver_expression_operation_t operation,
                                                                double lhs,
                                                                quiver_expression_t* rhs,
                                                                quiver_expression_t** out);
QUIVER_C_API quiver_error_t quiver_expression_apply_unary(quiver_expression_unary_operation_t operation,
                                                          quiver_expression_t* operand,
                                                          quiver_expression_t** out);
QUIVER_C_API quiver_error_t quiver_expression_apply_ternary(quiver_expression_ternary_operation_t operation,
                                                            quiver_expression_t* condition,
                                                            quiver_expression_t* then_value,
                                                            quiver_expression_t* else_value,
                                                            quiver_expression_t** out);

// Materialization
QUIVER_C_API quiver_error_t quiver_expression_save(quiver_expression_t* expression, const char* path);

// Metadata access
QUIVER_C_API quiver_error_t quiver_expression_get_metadata(quiver_expression_t* expression,
                                                           quiver_binary_metadata_t** out);

// Aggregation. parameter == NULL means "no parameter" (required for sum/mean/min/max).
// Non-null pointer supplies the value (required for percentile, in [0, 1]).
QUIVER_C_API quiver_error_t quiver_expression_aggregate(quiver_expression_t* expression,
                                                        const char* dimension,
                                                        const char* operation,
                                                        const double* parameter,
                                                        quiver_expression_t** out);

QUIVER_C_API quiver_error_t quiver_expression_aggregate_agents(quiver_expression_t* expression,
                                                               const char* operation,
                                                               const double* parameter,
                                                               quiver_expression_t** out);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_EXPRESSION_H
