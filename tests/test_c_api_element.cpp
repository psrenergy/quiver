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
    EXPECT_EQ(psr_element_has_vectors(element), 0);
    EXPECT_EQ(psr_element_scalar_count(element), 0);
    EXPECT_EQ(psr_element_vector_count(element), 0);

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

TEST(ElementCApi, SetVectorInt) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    int64_t values[] = {1, 2, 3};
    EXPECT_EQ(psr_element_set_vector_int(element, "ids", values, 3), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);
    EXPECT_EQ(psr_element_vector_count(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetVectorDouble) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    double values[] = {1.5, 2.5, 3.5};
    EXPECT_EQ(psr_element_set_vector_double(element, "costs", values, 3), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, SetVectorString) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    const char* values[] = {"a", "b", "c"};
    EXPECT_EQ(psr_element_set_vector_string(element, "names", values, 3), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, Clear) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    psr_element_set_int(element, "id", 1);
    double values[] = {1.0, 2.0};
    psr_element_set_vector_double(element, "data", values, 2);

    EXPECT_EQ(psr_element_has_scalars(element), 1);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_clear(element);

    EXPECT_EQ(psr_element_has_scalars(element), 0);
    EXPECT_EQ(psr_element_has_vectors(element), 0);

    psr_element_destroy(element);
}

TEST(ElementCApi, NullElementErrors) {
    EXPECT_EQ(psr_element_set_int(nullptr, "x", 1), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_double(nullptr, "x", 1.0), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_string(nullptr, "x", "y"), PSR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(psr_element_set_null(nullptr, "x"), PSR_ERROR_INVALID_ARGUMENT);

    int64_t ival[] = {1};
    EXPECT_EQ(psr_element_set_vector_int(nullptr, "x", ival, 1), PSR_ERROR_INVALID_ARGUMENT);

    double dval[] = {1.0};
    EXPECT_EQ(psr_element_set_vector_double(nullptr, "x", dval, 1), PSR_ERROR_INVALID_ARGUMENT);

    const char* sval[] = {"a"};
    EXPECT_EQ(psr_element_set_vector_string(nullptr, "x", sval, 1), PSR_ERROR_INVALID_ARGUMENT);
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
    EXPECT_EQ(psr_element_has_vectors(nullptr), 0);
    EXPECT_EQ(psr_element_scalar_count(nullptr), 0);
    EXPECT_EQ(psr_element_vector_count(nullptr), 0);
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

TEST(ElementCApi, EmptyVector) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(psr_element_set_vector_double(element, "empty", nullptr, 0), PSR_OK);
    EXPECT_EQ(psr_element_has_vectors(element), 1);

    psr_element_destroy(element);
}

TEST(ElementCApi, ToString) {
    auto element = psr_element_create();
    ASSERT_NE(element, nullptr);

    psr_element_set_string(element, "label", "Plant 1");
    psr_element_set_double(element, "capacity", 50.0);
    double costs[] = {1.5, 2.5};
    psr_element_set_vector_double(element, "costs", costs, 2);

    char* str = psr_element_to_string(element);
    ASSERT_NE(str, nullptr);

    std::string result(str);
    EXPECT_NE(result.find("Element {"), std::string::npos);
    EXPECT_NE(result.find("scalars:"), std::string::npos);
    EXPECT_NE(result.find("vectors:"), std::string::npos);
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
