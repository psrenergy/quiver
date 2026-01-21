#include <gtest/gtest.h>
#include <margaux/element.h>

TEST(Element, DefaultEmpty) {
    margaux::Element element;
    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_arrays());
    EXPECT_TRUE(element.scalars().empty());
    EXPECT_TRUE(element.arrays().empty());
}

TEST(Element, SetInt) {
    margaux::Element element;
    element.set("count", int64_t{42});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(std::get<int64_t>(element.scalars().at("count")), 42);
}

TEST(Element, SetFloat) {
    margaux::Element element;
    element.set("value", 3.14);

    EXPECT_TRUE(element.has_scalars());
    EXPECT_EQ(std::get<double>(element.scalars().at("value")), 3.14);
}

TEST(Element, SetString) {
    margaux::Element element;
    element.set("label", std::string{"Plant 1"});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_EQ(std::get<std::string>(element.scalars().at("label")), "Plant 1");
}

TEST(Element, SetNull) {
    margaux::Element element;
    element.set_null("empty");

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(element.scalars().at("empty")));
}

TEST(Element, SetArrayInt) {
    margaux::Element element;
    element.set("counts", std::vector<int64_t>{10, 20, 30});

    EXPECT_TRUE(element.has_arrays());
    const auto& arrays = element.arrays();
    EXPECT_EQ(arrays.size(), 1);
    EXPECT_EQ(arrays.at("counts").size(), 3);
    EXPECT_EQ(std::get<int64_t>(arrays.at("counts")[0]), 10);
    EXPECT_EQ(std::get<int64_t>(arrays.at("counts")[1]), 20);
    EXPECT_EQ(std::get<int64_t>(arrays.at("counts")[2]), 30);
}

TEST(Element, SetArrayFloat) {
    margaux::Element element;
    element.set("values", std::vector<double>{1.5, 2.5, 3.5});

    EXPECT_TRUE(element.has_arrays());
    const auto& arrays = element.arrays();
    EXPECT_EQ(arrays.size(), 1);
    EXPECT_EQ(arrays.at("values").size(), 3);
    EXPECT_EQ(std::get<double>(arrays.at("values")[0]), 1.5);
    EXPECT_EQ(std::get<double>(arrays.at("values")[2]), 3.5);
}

TEST(Element, SetArrayString) {
    margaux::Element element;
    element.set("tags", std::vector<std::string>{"important", "urgent"});

    EXPECT_TRUE(element.has_arrays());
    const auto& arrays = element.arrays();
    EXPECT_EQ(arrays.size(), 1);
    EXPECT_EQ(arrays.at("tags").size(), 2);
    EXPECT_EQ(std::get<std::string>(arrays.at("tags")[0]), "important");
    EXPECT_EQ(std::get<std::string>(arrays.at("tags")[1]), "urgent");
}

TEST(Element, FluentChaining) {
    margaux::Element element;

    element.set("label", std::string{"Plant 1"})
        .set("capacity", 50.0)
        .set("id", int64_t{1})
        .set("costs", std::vector<double>{1.0, 2.0, 3.0});

    EXPECT_EQ(element.scalars().size(), 3);
    EXPECT_EQ(element.arrays().size(), 1);
}

TEST(Element, Clear) {
    margaux::Element element;
    element.set("label", std::string{"test"}).set("data", std::vector<double>{1.0});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(element.has_arrays());

    element.clear();

    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_arrays());
}

TEST(Element, OverwriteValue) {
    margaux::Element element;
    element.set("value", 1.0);
    element.set("value", 2.0);

    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(std::get<double>(element.scalars().at("value")), 2.0);
}

TEST(Element, ToString) {
    margaux::Element element;
    element.set("label", std::string{"Plant 1"}).set("capacity", 50.0).set("costs", std::vector<double>{1.5, 2.5});

    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_NE(str.find("scalars:"), std::string::npos);
    EXPECT_NE(str.find("arrays:"), std::string::npos);
    EXPECT_NE(str.find("label: \"Plant 1\""), std::string::npos);
    EXPECT_NE(str.find("capacity:"), std::string::npos);
    EXPECT_NE(str.find("costs:"), std::string::npos);
}

TEST(Element, ToStringEmpty) {
    margaux::Element element;
    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_EQ(str.find("scalars:"), std::string::npos);
    EXPECT_EQ(str.find("vectors:"), std::string::npos);
}

// ============================================================================
// to_string() formatting edge cases
// ============================================================================

TEST(Element, ToStringWithSpecialCharacters) {
    margaux::Element element;
    element.set("label", std::string("Test \"with\" special\nchars"));

    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_NE(str.find("scalars:"), std::string::npos);
    EXPECT_NE(str.find("label:"), std::string::npos);
}

TEST(Element, ToStringWithEmptyString) {
    margaux::Element element;
    element.set("empty_value", std::string(""));

    std::string str = element.to_string();

    EXPECT_NE(str.find("empty_value:"), std::string::npos);
    EXPECT_NE(str.find("\"\""), std::string::npos);
}

TEST(Element, ToStringWithLargeArray) {
    margaux::Element element;
    std::vector<int64_t> large_array;
    for (int i = 0; i < 100; ++i) {
        large_array.push_back(i);
    }
    element.set("large_array", large_array);

    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_NE(str.find("arrays:"), std::string::npos);
    EXPECT_NE(str.find("large_array:"), std::string::npos);
    // Verify first and last elements are in the string
    EXPECT_NE(str.find("0"), std::string::npos);
    EXPECT_NE(str.find("99"), std::string::npos);
}

TEST(Element, ToStringWithNullValue) {
    margaux::Element element;
    element.set_null("nullable_field");

    std::string str = element.to_string();

    EXPECT_NE(str.find("nullable_field:"), std::string::npos);
    EXPECT_NE(str.find("null"), std::string::npos);
}

// ============================================================================
// Element builder edge cases
// ============================================================================

TEST(Element, SetOverwriteWithDifferentType) {
    margaux::Element element;
    element.set("value", int64_t{42});

    // Overwrite with different type (double)
    element.set("value", 3.14);

    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(std::get<double>(element.scalars().at("value")), 3.14);
}

TEST(Element, ClearAndReuse) {
    margaux::Element element;
    element.set("label", std::string("Original")).set("data", std::vector<double>{1.0, 2.0});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(element.has_arrays());

    element.clear();

    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_arrays());

    // Reuse after clear
    element.set("new_label", std::string("Reused")).set("new_data", std::vector<int64_t>{3, 4, 5});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(element.has_arrays());
    EXPECT_EQ(std::get<std::string>(element.scalars().at("new_label")), "Reused");
    EXPECT_EQ(element.arrays().at("new_data").size(), 3);
}

TEST(Element, SetMultipleSameNameArrays) {
    margaux::Element element;
    element.set("values", std::vector<int64_t>{1, 2, 3});

    // Overwrite with different array
    element.set("values", std::vector<int64_t>{10, 20});

    EXPECT_EQ(element.arrays().size(), 1);
    EXPECT_EQ(element.arrays().at("values").size(), 2);
    EXPECT_EQ(std::get<int64_t>(element.arrays().at("values")[0]), 10);
}

TEST(Element, SetMixedScalarsAndArrays) {
    margaux::Element element;
    element.set("label", std::string("Test"))
        .set("integer_value", int64_t{42})
        .set("float_value", 3.14)
        .set_null("null_value")
        .set("integer_array", std::vector<int64_t>{1, 2, 3})
        .set("float_array", std::vector<double>{1.1, 2.2})
        .set("string_array", std::vector<std::string>{"a", "b", "c"});

    EXPECT_EQ(element.scalars().size(), 4);
    EXPECT_EQ(element.arrays().size(), 3);

    std::string str = element.to_string();
    EXPECT_NE(str.find("scalars:"), std::string::npos);
    EXPECT_NE(str.find("arrays:"), std::string::npos);
}

TEST(Element, ToStringWithAllTypes) {
    margaux::Element element;
    element.set("text", std::string("hello")).set("integer", int64_t{123}).set("real", 45.67).set_null("empty");

    std::string str = element.to_string();

    EXPECT_NE(str.find("\"hello\""), std::string::npos);
    EXPECT_NE(str.find("123"), std::string::npos);
    EXPECT_NE(str.find("45.67"), std::string::npos);
    EXPECT_NE(str.find("null"), std::string::npos);
}
