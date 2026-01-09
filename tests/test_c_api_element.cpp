#include <gtest/gtest.h>
#include <psr/c/element.h>
#include <string>

TEST(ElementCApi, CreateAndDestroy) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);
    psr_element_destroy(element);
}

TEST(ElementCApi, DestroyNull) {
    psr_element_destroy(nullptr);
}

TEST(ElementCApi, EmptyElement) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_has_scalars(element), 0);
    EXPECT_EQ(psr_element_has_vector_groups(element), 0);
    EXPECT_EQ(psr_element_has_set_groups(element), 0);
    EXPECT_EQ(psr_element_scalar_count(element), 0);
    EXPECT_EQ(psr_element_vector_group_count(element), 0);
    EXPECT_EQ(psr_element_set_group_count(element), 0);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetInt) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_int(element, "count", 42), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);
    EXPECT_EQ(psr_element_scalar_count(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetDouble) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_double(element, "value", 3.14), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetString) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_string(element, "label", "Plant 1"), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetNull) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_null(element, "empty"), PSR_OK);
    EXPECT_EQ(psr_element_has_scalars(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, VectorGroup) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    auto group = psr_vector_group_create();
    ASSERT_NE(group, nullptr);

    // Add first row
    EXPECT_EQ(psr_vector_group_add_row(group), PSR_OK);
    EXPECT_EQ(psr_vector_group_set_double(group, "value", 1.5), PSR_OK);
    EXPECT_EQ(psr_vector_group_set_int(group, "count", 10), PSR_OK);

    // Add second row
    EXPECT_EQ(psr_vector_group_add_row(group), PSR_OK);
    EXPECT_EQ(psr_vector_group_set_double(group, "value", 2.5), PSR_OK);
    EXPECT_EQ(psr_vector_group_set_int(group, "count", 20), PSR_OK);

    EXPECT_EQ(psr_element_add_vector_group(element, "test_group", group), PSR_OK);
    EXPECT_EQ(psr_element_has_vector_groups(element), 1);
    EXPECT_EQ(psr_element_vector_group_count(element), 1);

    psr_vector_group_destroy(group);
    psr_element_destroy(element);
}

TEST(ElementCApi, SetGroup) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    auto group = psr_set_group_create();
    ASSERT_NE(group, nullptr);

    // Add first row
    EXPECT_EQ(psr_set_group_add_row(group), PSR_OK);
    EXPECT_EQ(psr_set_group_set_string(group, "tag", "important"), PSR_OK);
    EXPECT_EQ(psr_set_group_set_int(group, "priority", 1), PSR_OK);

    // Add second row
    EXPECT_EQ(psr_set_group_add_row(group), PSR_OK);
    EXPECT_EQ(psr_set_group_set_string(group, "tag", "urgent"), PSR_OK);
    EXPECT_EQ(psr_set_group_set_int(group, "priority", 2), PSR_OK);

    EXPECT_EQ(psr_element_add_set_group(element, "tags", group), PSR_OK);
    EXPECT_EQ(psr_element_has_set_groups(element), 1);
    EXPECT_EQ(psr_element_set_group_count(element), 1);

    psr_set_group_destroy(group);
    psr_element_destroy(element);
}

TEST(ElementCApi, Clear) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    psr_element_set_int(element, "id", 1);

    auto group = psr_vector_group_create();
    psr_vector_group_add_row(group);
    psr_vector_group_set_double(group, "data", 1.0);
    psr_element_add_vector_group(element, "data", group);
    psr_vector_group_destroy(group);

    EXPECT_EQ(psr_element_has_scalars(element), 1);
    EXPECT_EQ(psr_element_has_vector_groups(element), 1);

    psr_element_clear(element);

    EXPECT_EQ(psr_element_has_scalars(element), 0);
    EXPECT_EQ(psr_element_has_vector_groups(element), 0);

    psr_element_destroy(element);
}

TEST(ElementCApi, NullElementErrors) {
    EXPECT_EQ(psr_element_set_int(nullptr, "x", 1), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_double(nullptr, "x", 1.0), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_string(nullptr, "x", "y"), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_null(nullptr, "x"), PSR_ERROR_INVALID_ARGUMENT);
}

TEST(ElementCApi, NullNameErrors) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_int(element, nullptr, 1), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_double(element, nullptr, 1.0), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_string(element, nullptr, "y"), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_null(element, nullptr), PSR_ERROR_INVALID_ARGUMENT);

    psr_element_destroy(element);
}

TEST(ElementCApi, NullAccessors) {
    EXPECT_EQ(psr_element_has_scalars(nullptr), 0);
    EXPECT_EQ(psr_element_has_vector_groups(nullptr), 0);
    EXPECT_EQ(psr_element_has_set_groups(nullptr), 0);
    EXPECT_EQ(psr_element_scalar_count(nullptr), 0);
    EXPECT_EQ(psr_element_vector_group_count(nullptr), 0);
    EXPECT_EQ(psr_element_set_group_count(nullptr), 0);
}

TEST(ElementCApi, MultipleScalars) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    psr_element_set_string(element, "label", "Plant 1");
    psr_element_set_double(element, "capacity", 50.0);
    psr_element_set_int(element, "id", 1);

    EXPECT_EQ(psr_element_scalar_count(element), 3);

    psr_element_destroy(element);
}

TEST(ElementCApi, ToString) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    psr_element_set_string(element, "label", "Plant 1");
    psr_element_set_double(element, "capacity", 50.0);

    auto group = psr_vector_group_create();
    psr_vector_group_add_row(group);
    psr_vector_group_set_double(group, "costs", 1.5);
    psr_vector_group_add_row(group);
    psr_vector_group_set_double(group, "costs", 2.5);
    psr_element_add_vector_group(element, "costs", group);
    psr_vector_group_destroy(group);

    char* str = psr_element_to_string(element);
    ASSERT_NE(str, nullptr);

    std::string result(str);
    EXPECT_NE(result.find("Element {"), std::string::npos);
    EXPECT_NE(result.find("scalars:"), std::string::npos);
    EXPECT_NE(result.find("vector_groups:"), std::string::npos);
    EXPECT_NE(result.find("label: \"Plant 1\""), std::string::npos);

    psr_string_free(str);
    psr_element_destroy(element);
}

TEST(ElementCApi, ToStringNull) {
    EXPECT_EQ(psr_element_to_string(nullptr), nullptr);
}

TEST(ElementCApi, StringFreeNull) {
    psr_string_free(nullptr);
}

TEST(ElementCApi, VectorGroupNullErrors) {
    EXPECT_EQ(psr_vector_group_add_row(nullptr), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_vector_group_set_int(nullptr, "x", 1), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_vector_group_set_double(nullptr, "x", 1.0), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_vector_group_set_string(nullptr, "x", "y"), PSR_ERROR_INVALID_ARGUMENT);

    auto element = psr_element_create();
    EXPECT_EQ(psr_element_add_vector_group(element, "x", nullptr), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_add_vector_group(nullptr, "x", nullptr), PSR_ERROR_INVALID_ARGUMENT);
    psr_element_destroy(element);
}

TEST(ElementCApi, SetGroupNullErrors) {
    EXPECT_EQ(psr_set_group_add_row(nullptr), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_set_group_set_int(nullptr, "x", 1), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_set_group_set_double(nullptr, "x", 1.0), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_set_group_set_string(nullptr, "x", "y"), PSR_ERROR_INVALID_ARGUMENT);

    auto element = psr_element_create();
    EXPECT_EQ(psr_element_add_set_group(element, "x", nullptr), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_add_set_group(nullptr, "x", nullptr), PSR_ERROR_INVALID_ARGUMENT);
    psr_element_destroy(element);
}
