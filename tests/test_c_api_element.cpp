#include <gtest/gtest.h>
#include <margaux/c/element.h>
#include <string>

TEST(ElementCApi, CreateAndDestroy) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);
    element_destroy(element);
}

TEST(ElementCApi, DestroyNull) {
    element_destroy(nullptr);
}

TEST(ElementCApi, EmptyElement) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element_has_scalars(element), 0);
    EXPECT_EQ(element_has_arrays(element), 0);
    EXPECT_EQ(element_scalar_count(element), 0);
    EXPECT_EQ(element_array_count(element), 0);

    element_destroy(element);
}

TEST(ElementCApi, SetInt) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element_set_integer(element, "count", 42), DECK_DATABASE_OK);
    EXPECT_EQ(element_has_scalars(element), 1);
    EXPECT_EQ(element_scalar_count(element), 1);

    element_destroy(element);
}

TEST(ElementCApi, SetFloat) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element_set_float(element, "value", 3.14), DECK_DATABASE_OK);
    EXPECT_EQ(element_has_scalars(element), 1);

    element_destroy(element);
}

TEST(ElementCApi, SetString) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element_set_string(element, "label", "Plant 1"), DECK_DATABASE_OK);
    EXPECT_EQ(element_has_scalars(element), 1);

    element_destroy(element);
}

TEST(ElementCApi, SetNull) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element_set_null(element, "empty"), DECK_DATABASE_OK);
    EXPECT_EQ(element_has_scalars(element), 1);

    element_destroy(element);
}

TEST(ElementCApi, SetArrayInt) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    int64_t values[] = {10, 20, 30};
    EXPECT_EQ(element_set_array_integer(element, "counts", values, 3), DECK_DATABASE_OK);
    EXPECT_EQ(element_has_arrays(element), 1);
    EXPECT_EQ(element_array_count(element), 1);

    element_destroy(element);
}

TEST(ElementCApi, SetArrayFloat) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    double values[] = {1.5, 2.5, 3.5};
    EXPECT_EQ(element_set_array_float(element, "costs", values, 3), DECK_DATABASE_OK);
    EXPECT_EQ(element_has_arrays(element), 1);
    EXPECT_EQ(element_array_count(element), 1);

    element_destroy(element);
}

TEST(ElementCApi, SetArrayString) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    const char* values[] = {"important", "urgent", "review"};
    EXPECT_EQ(element_set_array_string(element, "tags", values, 3), DECK_DATABASE_OK);
    EXPECT_EQ(element_has_arrays(element), 1);
    EXPECT_EQ(element_array_count(element), 1);

    element_destroy(element);
}

TEST(ElementCApi, Clear) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    element_set_integer(element, "id", 1);
    double values[] = {1.0, 2.0};
    element_set_array_float(element, "data", values, 2);

    EXPECT_EQ(element_has_scalars(element), 1);
    EXPECT_EQ(element_has_arrays(element), 1);

    element_clear(element);

    EXPECT_EQ(element_has_scalars(element), 0);
    EXPECT_EQ(element_has_arrays(element), 0);

    element_destroy(element);
}

TEST(ElementCApi, NullElementErrors) {
    EXPECT_EQ(element_set_integer(nullptr, "x", 1), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_float(nullptr, "x", 1.0), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_string(nullptr, "x", "y"), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_null(nullptr, "x"), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
}

TEST(ElementCApi, NullNameErrors) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element_set_integer(element, nullptr, 1), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_float(element, nullptr, 1.0), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_string(element, nullptr, "y"), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_null(element, nullptr), DECK_DATABASE_ERROR_INVALID_ARGUMENT);

    element_destroy(element);
}

TEST(ElementCApi, NullAccessors) {
    EXPECT_EQ(element_has_scalars(nullptr), 0);
    EXPECT_EQ(element_has_arrays(nullptr), 0);
    EXPECT_EQ(element_scalar_count(nullptr), 0);
    EXPECT_EQ(element_array_count(nullptr), 0);
}

TEST(ElementCApi, MultipleScalars) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    element_set_string(element, "label", "Plant 1");
    element_set_float(element, "capacity", 50.0);
    element_set_integer(element, "id", 1);

    EXPECT_EQ(element_scalar_count(element), 3);

    element_destroy(element);
}

TEST(ElementCApi, ToString) {
    auto element = element_create();
    ASSERT_NE(element, nullptr);

    element_set_string(element, "label", "Plant 1");
    element_set_float(element, "capacity", 50.0);

    double costs[] = {1.5, 2.5};
    element_set_array_float(element, "costs", costs, 2);

    char* str = element_to_string(element);
    ASSERT_NE(str, nullptr);

    std::string result(str);
    EXPECT_NE(result.find("Element {"), std::string::npos);
    EXPECT_NE(result.find("scalars:"), std::string::npos);
    EXPECT_NE(result.find("arrays:"), std::string::npos);
    EXPECT_NE(result.find("label: \"Plant 1\""), std::string::npos);

    margaux_string_free(str);
    element_destroy(element);
}

TEST(ElementCApi, ToStringNull) {
    EXPECT_EQ(element_to_string(nullptr), nullptr);
}

TEST(ElementCApi, StringFreeNull) {
    margaux_string_free(nullptr);
}

TEST(ElementCApi, ArrayNullErrors) {
    int64_t integer_values[] = {1, 2, 3};
    double float_values[] = {1.0, 2.0, 3.0};
    const char* string_values[] = {"a", "b", "c"};

    EXPECT_EQ(element_set_array_integer(nullptr, "x", integer_values, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_array_float(nullptr, "x", float_values, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_array_string(nullptr, "x", string_values, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);

    auto element = element_create();
    EXPECT_EQ(element_set_array_integer(element, nullptr, integer_values, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_array_float(element, nullptr, float_values, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_array_string(element, nullptr, string_values, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_array_integer(element, "x", nullptr, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_array_float(element, "x", nullptr, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(element_set_array_string(element, "x", nullptr, 3), DECK_DATABASE_ERROR_INVALID_ARGUMENT);
    element_destroy(element);
}
