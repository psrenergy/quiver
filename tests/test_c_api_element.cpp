#include <cstring>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>

namespace {

// quiver_element_to_string is the only way to read an element back through the C API.
std::string element_string(quiver_element_t* element) {
    char* out = nullptr;
    EXPECT_EQ(quiver_element_to_string(element, &out), QUIVER_OK);
    std::string result = out ? out : "";
    quiver_database_free_string(out);
    return result;
}

}  // namespace

TEST(ElementCApi, CreateAndDestroy) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, DestroyNull) {
    EXPECT_EQ(quiver_element_destroy(nullptr), QUIVER_ERROR);
}

TEST(ElementCApi, EmptyElement) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element_string(element), "Element {\n}");

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetInt) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_integer(element, "count", 42), QUIVER_OK);
    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  scalars:\n"
        "    count: 42\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetFloat) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_float(element, "value", 3.14), QUIVER_OK);
    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  scalars:\n"
        "    value: 3.140000\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetString) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_string(element, "label", "Plant 1"), QUIVER_OK);
    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  scalars:\n"
        "    label: \"Plant 1\"\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetNull) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_null(element, "empty"), QUIVER_OK);
    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  scalars:\n"
        "    empty: null\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetArrayInt) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    int64_t values[] = {10, 20, 30};
    EXPECT_EQ(quiver_element_set_array_integer(element, "counts", values, 3, nullptr), QUIVER_OK);
    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  arrays:\n"
        "    counts: [10, 20, 30]\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetArrayFloat) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    double values[] = {1.5, 2.5, 3.5};
    EXPECT_EQ(quiver_element_set_array_float(element, "costs", values, 3, nullptr), QUIVER_OK);
    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  arrays:\n"
        "    costs: [1.500000, 2.500000, 3.500000]\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetArrayString) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    const char* values[] = {"important", "urgent", "review"};
    EXPECT_EQ(quiver_element_set_array_string(element, "tags", values, 3, nullptr), QUIVER_OK);
    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  arrays:\n"
        "    tags: [\"important\", \"urgent\", \"review\"]\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, SetArrayWithNullMask) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    // has_value[i] == 0 stores SQL NULL for that cell; the data slot is a placeholder
    int64_t values[] = {10, 0, 30};
    uint8_t has_value[] = {1, 0, 1};
    EXPECT_EQ(quiver_element_set_array_integer(element, "refs", values, 3, has_value), QUIVER_OK);

    char* out_string = nullptr;
    EXPECT_EQ(quiver_element_to_string(element, &out_string), QUIVER_OK);
    EXPECT_NE(strstr(out_string, "[10, null, 30]"), nullptr);
    quiver_database_free_string(out_string);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, Clear) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    quiver_element_set_integer(element, "id", 1);
    double values[] = {1.0, 2.0};
    quiver_element_set_array_float(element, "data", values, 2, nullptr);

    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  scalars:\n"
        "    id: 1\n"
        "  arrays:\n"
        "    data: [1.000000, 2.000000]\n"
        "}"
    );

    EXPECT_EQ(quiver_element_clear(element), QUIVER_OK);

    EXPECT_EQ(element_string(element), "Element {\n}");

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, ClearNull) {
    EXPECT_EQ(quiver_element_clear(nullptr), QUIVER_ERROR);
}

TEST(ElementCApi, NullElementErrors) {
    EXPECT_EQ(quiver_element_set_integer(nullptr, "x", 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_float(nullptr, "x", 1.0), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_string(nullptr, "x", "y"), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_null(nullptr, "x"), QUIVER_ERROR);
}

TEST(ElementCApi, NullNameErrors) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(quiver_element_set_integer(element, nullptr, 1), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_float(element, nullptr, 1.0), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_string(element, nullptr, "y"), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_null(element, nullptr), QUIVER_ERROR);

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, MultipleScalars) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    quiver_element_set_string(element, "label", "Plant 1");
    quiver_element_set_float(element, "capacity", 50.0);
    quiver_element_set_integer(element, "id", 1);

    EXPECT_EQ(
        element_string(element),
        "Element {\n"
        "  scalars:\n"
        "    capacity: 50.000000\n"
        "    id: 1\n"
        "    label: \"Plant 1\"\n"
        "}"
    );

    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, ToString) {
    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    ASSERT_NE(element, nullptr);

    quiver_element_set_string(element, "label", "Plant 1");
    quiver_element_set_float(element, "capacity", 50.0);

    double costs[] = {1.5, 2.5};
    quiver_element_set_array_float(element, "costs", costs, 2, nullptr);

    char* str = nullptr;
    ASSERT_EQ(quiver_element_to_string(element, &str), QUIVER_OK);
    ASSERT_NE(str, nullptr);

    std::string result(str);
    EXPECT_NE(result.find("Element {"), std::string::npos);
    EXPECT_NE(result.find("scalars:"), std::string::npos);
    EXPECT_NE(result.find("arrays:"), std::string::npos);
    EXPECT_NE(result.find("label: \"Plant 1\""), std::string::npos);

    quiver_database_free_string(str);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}

TEST(ElementCApi, ToStringNull) {
    char* str = nullptr;
    EXPECT_EQ(quiver_element_to_string(nullptr, &str), QUIVER_ERROR);
}

TEST(ElementCApi, StringFreeNull) {
    EXPECT_EQ(quiver_database_free_string(nullptr), QUIVER_OK);
}

TEST(ElementCApi, ArrayNullErrors) {
    int64_t integer_values[] = {1, 2, 3};
    double float_values[] = {1.0, 2.0, 3.0};
    const char* string_values[] = {"a", "b", "c"};

    EXPECT_EQ(quiver_element_set_array_integer(nullptr, "x", integer_values, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_array_float(nullptr, "x", float_values, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_array_string(nullptr, "x", string_values, 3, nullptr), QUIVER_ERROR);

    quiver_element_t* element = nullptr;
    ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
    EXPECT_EQ(quiver_element_set_array_integer(element, nullptr, integer_values, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_array_float(element, nullptr, float_values, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_array_string(element, nullptr, string_values, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_array_integer(element, "x", nullptr, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_array_float(element, "x", nullptr, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_set_array_string(element, "x", nullptr, 3, nullptr), QUIVER_ERROR);
    EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
}
