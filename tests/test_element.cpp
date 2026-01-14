#include <gtest/gtest.h>
#include <psr/element.h>

TEST(Element, DefaultEmpty) {
    psr::Element element;
    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_arrays());
    EXPECT_TRUE(element.scalars().empty());
    EXPECT_TRUE(element.arrays().empty());
}

TEST(Element, SetInt) {
    psr::Element element;
    element.set("count", int64_t{42});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(std::get<int64_t>(element.scalars().at("count")), 42);
}

TEST(Element, SetDouble) {
    psr::Element element;
    element.set("value", 3.14);

    EXPECT_TRUE(element.has_scalars());
    EXPECT_EQ(std::get<double>(element.scalars().at("value")), 3.14);
}

TEST(Element, SetString) {
    psr::Element element;
    element.set("label", std::string{"Plant 1"});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_EQ(std::get<std::string>(element.scalars().at("label")), "Plant 1");
}

TEST(Element, SetNull) {
    psr::Element element;
    element.set_null("empty");

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(element.scalars().at("empty")));
}

TEST(Element, SetArrayInt) {
    psr::Element element;
    element.set("counts", std::vector<int64_t>{10, 20, 30});

    EXPECT_TRUE(element.has_arrays());
    const auto& arrays = element.arrays();
    EXPECT_EQ(arrays.size(), 1);
    EXPECT_EQ(arrays.at("counts").size(), 3);
    EXPECT_EQ(std::get<int64_t>(arrays.at("counts")[0]), 10);
    EXPECT_EQ(std::get<int64_t>(arrays.at("counts")[1]), 20);
    EXPECT_EQ(std::get<int64_t>(arrays.at("counts")[2]), 30);
}

TEST(Element, SetArrayDouble) {
    psr::Element element;
    element.set("values", std::vector<double>{1.5, 2.5, 3.5});

    EXPECT_TRUE(element.has_arrays());
    const auto& arrays = element.arrays();
    EXPECT_EQ(arrays.size(), 1);
    EXPECT_EQ(arrays.at("values").size(), 3);
    EXPECT_EQ(std::get<double>(arrays.at("values")[0]), 1.5);
    EXPECT_EQ(std::get<double>(arrays.at("values")[2]), 3.5);
}

TEST(Element, SetArrayString) {
    psr::Element element;
    element.set("tags", std::vector<std::string>{"important", "urgent"});

    EXPECT_TRUE(element.has_arrays());
    const auto& arrays = element.arrays();
    EXPECT_EQ(arrays.size(), 1);
    EXPECT_EQ(arrays.at("tags").size(), 2);
    EXPECT_EQ(std::get<std::string>(arrays.at("tags")[0]), "important");
    EXPECT_EQ(std::get<std::string>(arrays.at("tags")[1]), "urgent");
}

TEST(Element, FluentChaining) {
    psr::Element element;

    element.set("label", std::string{"Plant 1"})
        .set("capacity", 50.0)
        .set("id", int64_t{1})
        .set("costs", std::vector<double>{1.0, 2.0, 3.0});

    EXPECT_EQ(element.scalars().size(), 3);
    EXPECT_EQ(element.arrays().size(), 1);
}

TEST(Element, Clear) {
    psr::Element element;
    element.set("label", std::string{"test"}).set("data", std::vector<double>{1.0});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(element.has_arrays());

    element.clear();

    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_arrays());
}

TEST(Element, OverwriteValue) {
    psr::Element element;
    element.set("value", 1.0);
    element.set("value", 2.0);

    EXPECT_EQ(element.scalars().size(), 1);
    EXPECT_EQ(std::get<double>(element.scalars().at("value")), 2.0);
}

TEST(Element, ToString) {
    psr::Element element;
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
    psr::Element element;
    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_EQ(str.find("scalars:"), std::string::npos);
    EXPECT_EQ(str.find("vectors:"), std::string::npos);
}
