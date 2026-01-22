#include <gtest/gtest.h>
#include <quiver/c/element.h>
#include <string>

TEST(ElementCApi, CreateAndDestroy) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);
    quiver_element_destroy(element);
}

TEST(ElementCApi, DestroyNull) {
    quiver_element_destroy(nullptr);
}

TEST(ElementCApi, EmptyElement) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_has_scalars(element), 0);
    EXPECT_EQ(quiver_element_has_arrays(element), 0);
    EXPECT_EQ(quiver_element_scalar_count(element), 0);
    EXPECT_EQ(quiver_element_array_count(element), 0);

    quiver_element_destroy(element);
}

TEST(ElementCApi, SetInt) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_integer(element, "count", 42), QUIVER_OK);
    EXPECT_EQ(quiver_element_has_scalars(element), 1);
    EXPECT_EQ(quiver_element_scalar_count(element), 1);

    quiver_element_destroy(element);
}

TEST(ElementCApi, SetFloat) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_float(element, "value", 3.14), QUIVER_OK);
    EXPECT_EQ(quiver_element_has_scalars(element), 1);

    quiver_element_destroy(element);
}

TEST(ElementCApi, SetString) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_string(element, "label", "Plant 1"), QUIVER_OK);
    EXPECT_EQ(quiver_element_has_scalars(element), 1);

    quiver_element_destroy(element);
}

TEST(ElementCApi, SetNull) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_null(element, "empty"), QUIVER_OK);
    EXPECT_EQ(quiver_element_has_scalars(element), 1);

    quiver_element_destroy(element);
}

TEST(ElementCApi, SetArrayInt) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    int64_t values[] = {10, 20, 30};
    EXPECT_EQ(quiver_element_set_array_integer(element, "counts", values, 3), QUIVER_OK);
    EXPECT_EQ(quiver_element_has_arrays(element), 1);
    EXPECT_EQ(quiver_element_array_count(element), 1);

    quiver_element_destroy(element);
}

TEST(ElementCApi, SetArrayFloat) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    double values[] = {1.5, 2.5, 3.5};
    EXPECT_EQ(quiver_element_set_array_float(element, "costs", values, 3), QUIVER_OK);
    EXPECT_EQ(quiver_element_has_arrays(element), 1);
    EXPECT_EQ(quiver_element_array_count(element), 1);

    quiver_element_destroy(element);
}

TEST(ElementCApi, SetArrayString) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    const char* values[] = {"important", "urgent", "review"};
    EXPECT_EQ(quiver_element_set_array_string(element, "tags", values, 3), QUIVER_OK);
    EXPECT_EQ(quiver_element_has_arrays(element), 1);
    EXPECT_EQ(quiver_element_array_count(element), 1);

    quiver_element_destroy(element);
}

TEST(ElementCApi, Clear) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    quiver_element_set_integer(element, "id", 1);
    double values[] = {1.0, 2.0};
    quiver_element_set_array_float(element, "data", values, 2);

    EXPECT_EQ(quiver_element_has_scalars(element), 1);
    EXPECT_EQ(quiver_element_has_arrays(element), 1);

    quiver_element_clear(element);

    EXPECT_EQ(quiver_element_has_scalars(element), 0);
    EXPECT_EQ(quiver_element_has_arrays(element), 0);

    quiver_element_destroy(element);
}

TEST(ElementCApi, NullElementErrors) {
    EXPECT_EQ(quiver_element_set_integer(nullptr, "x", 1), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_float(nullptr, "x", 1.0), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_string(nullptr, "x", "y"), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_null(nullptr, "x"), QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST(ElementCApi, NullNameErrors) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_integer(element, nullptr, 1), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_float(element, nullptr, 1.0), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_string(element, nullptr, "y"), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_null(element, nullptr), QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_element_destroy(element);
}

TEST(ElementCApi, NullAccessors) {
    EXPECT_EQ(quiver_element_has_scalars(nullptr), 0);
    EXPECT_EQ(quiver_element_has_arrays(nullptr), 0);
    EXPECT_EQ(quiver_element_scalar_count(nullptr), 0);
    EXPECT_EQ(quiver_element_array_count(nullptr), 0);
}

TEST(ElementCApi, MultipleScalars) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    quiver_element_set_string(element, "label", "Plant 1");
    quiver_element_set_float(element, "capacity", 50.0);
    quiver_element_set_integer(element, "id", 1);

    EXPECT_EQ(quiver_element_scalar_count(element), 3);

    quiver_element_destroy(element);
}

TEST(ElementCApi, ToString) {
    auto element = quiver_element_create();
    ASSERT_NE(element, nullptr);

    quiver_element_set_string(element, "label", "Plant 1");
    quiver_element_set_float(element, "capacity", 50.0);

    double costs[] = {1.5, 2.5};
    quiver_element_set_array_float(element, "costs", costs, 2);

    char* str = quiver_element_to_string(element);
    ASSERT_NE(str, nullptr);

    std::string result(str);
    EXPECT_NE(result.find("Element {"), std::string::npos);
    EXPECT_NE(result.find("scalars:"), std::string::npos);
    EXPECT_NE(result.find("arrays:"), std::string::npos);
    EXPECT_NE(result.find("label: \"Plant 1\""), std::string::npos);

    quiver_string_free(str);
    quiver_element_destroy(element);
}

TEST(ElementCApi, ToStringNull) {
    EXPECT_EQ(quiver_element_to_string(nullptr), nullptr);
}

TEST(ElementCApi, StringFreeNull) {
    quiver_string_free(nullptr);
}

TEST(ElementCApi, ArrayNullErrors) {
    int64_t integer_values[] = {1, 2, 3};
    double float_values[] = {1.0, 2.0, 3.0};
    const char* string_values[] = {"a", "b", "c"};

    EXPECT_EQ(quiver_element_set_array_integer(nullptr, "x", integer_values, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_array_float(nullptr, "x", float_values, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_array_string(nullptr, "x", string_values, 3), QUIVER_ERROR_INVALID_ARGUMENT);

    auto element = quiver_element_create();
    EXPECT_EQ(quiver_element_set_array_integer(element, nullptr, integer_values, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_array_float(element, nullptr, float_values, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_array_string(element, nullptr, string_values, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_array_integer(element, "x", nullptr, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_array_float(element, "x", nullptr, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(quiver_element_set_array_string(element, "x", nullptr, 3), QUIVER_ERROR_INVALID_ARGUMENT);
    quiver_element_destroy(element);
}
