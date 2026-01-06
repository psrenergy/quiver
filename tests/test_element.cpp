#include <gtest/gtest.h>
#include <psr/element.h>

TEST(Element, DefaultEmpty) {
    psr::Element element;
    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_vectors());
    EXPECT_TRUE(element.scalars().empty());
    EXPECT_TRUE(element.vectors().empty());
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

TEST(Element, SetVectorInt) {
    psr::Element element;
    element.set_vector("ids", std::vector<int64_t>{1, 2, 3});

    EXPECT_TRUE(element.has_vectors());
    auto& vec = std::get<std::vector<int64_t>>(element.vectors().at("ids"));
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[2], 3);
}

TEST(Element, SetVectorDouble) {
    psr::Element element;
    element.set_vector("costs", std::vector<double>{1.5, 2.5, 3.5});

    EXPECT_TRUE(element.has_vectors());
    auto& vec = std::get<std::vector<double>>(element.vectors().at("costs"));
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[1], 2.5);
}

TEST(Element, SetVectorString) {
    psr::Element element;
    element.set_vector("names", std::vector<std::string>{"a", "b", "c"});

    EXPECT_TRUE(element.has_vectors());
    auto& vec = std::get<std::vector<std::string>>(element.vectors().at("names"));
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], "a");
}

TEST(Element, FluentChaining) {
    psr::Element element;
    element.set("label", std::string{"Plant 1"})
        .set("capacity", 50.0)
        .set("id", int64_t{1})
        .set_vector("costs", std::vector<double>{1.0, 2.0, 3.0});

    EXPECT_EQ(element.scalars().size(), 3);
    EXPECT_EQ(element.vectors().size(), 1);
}

TEST(Element, Clear) {
    psr::Element element;
    element.set("label", std::string{"test"}).set_vector("data", std::vector<double>{1.0});

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(element.has_vectors());

    element.clear();

    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_vectors());
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
    element.set("label", std::string{"Plant 1"})
        .set("capacity", 50.0)
        .set_vector("costs", std::vector<double>{1.5, 2.5});

    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_NE(str.find("scalars:"), std::string::npos);
    EXPECT_NE(str.find("vectors:"), std::string::npos);
    EXPECT_NE(str.find("label: \"Plant 1\""), std::string::npos);
    EXPECT_NE(str.find("capacity:"), std::string::npos);
    EXPECT_NE(str.find("costs: ["), std::string::npos);
}

TEST(Element, ToStringEmpty) {
    psr::Element element;
    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_EQ(str.find("scalars:"), std::string::npos);
    EXPECT_EQ(str.find("vectors:"), std::string::npos);
}
