#include <gtest/gtest.h>
#include <psr/element.h>

TEST(Element, DefaultEmpty) {
    psr::Element element;
    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_vector_groups());
    EXPECT_FALSE(element.has_set_groups());
    EXPECT_TRUE(element.scalars().empty());
    EXPECT_TRUE(element.vector_groups().empty());
    EXPECT_TRUE(element.set_groups().empty());
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

TEST(Element, AddVectorGroup) {
    psr::Element element;
    psr::VectorGroup group = {{{"value", 1.5}, {"count", int64_t{10}}},
                              {{"value", 2.5}, {"count", int64_t{20}}},
                              {{"value", 3.5}, {"count", int64_t{30}}}};
    element.add_vector_group("test_group", group);

    EXPECT_TRUE(element.has_vector_groups());
    const auto& groups = element.vector_groups();
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups.at("test_group").size(), 3);
    EXPECT_EQ(std::get<double>(groups.at("test_group")[0].at("value")), 1.5);
    EXPECT_EQ(std::get<int64_t>(groups.at("test_group")[1].at("count")), 20);
}

TEST(Element, AddSetGroup) {
    psr::Element element;
    psr::SetGroup group = {{{"tag", std::string{"important"}}, {"priority", int64_t{1}}},
                           {{"tag", std::string{"urgent"}}, {"priority", int64_t{2}}}};
    element.add_set_group("tags", group);

    EXPECT_TRUE(element.has_set_groups());
    const auto& groups = element.set_groups();
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups.at("tags").size(), 2);
    EXPECT_EQ(std::get<std::string>(groups.at("tags")[0].at("tag")), "important");
}

TEST(Element, FluentChaining) {
    psr::Element element;
    psr::VectorGroup vec_group = {{{"cost", 1.0}}, {{"cost", 2.0}}, {{"cost", 3.0}}};
    psr::SetGroup set_group = {{{"name", std::string{"a"}}}};

    element.set("label", std::string{"Plant 1"})
        .set("capacity", 50.0)
        .set("id", int64_t{1})
        .add_vector_group("costs", vec_group)
        .add_set_group("names", set_group);

    EXPECT_EQ(element.scalars().size(), 3);
    EXPECT_EQ(element.vector_groups().size(), 1);
    EXPECT_EQ(element.set_groups().size(), 1);
}

TEST(Element, Clear) {
    psr::Element element;
    psr::VectorGroup group = {{{"data", 1.0}}};
    element.set("label", std::string{"test"}).add_vector_group("data", group);

    EXPECT_TRUE(element.has_scalars());
    EXPECT_TRUE(element.has_vector_groups());

    element.clear();

    EXPECT_FALSE(element.has_scalars());
    EXPECT_FALSE(element.has_vector_groups());
    EXPECT_FALSE(element.has_set_groups());
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
    psr::VectorGroup vec_group = {{{"costs", 1.5}}, {{"costs", 2.5}}};
    element.set("label", std::string{"Plant 1"}).set("capacity", 50.0).add_vector_group("costs", vec_group);

    std::string str = element.to_string();

    EXPECT_NE(str.find("Element {"), std::string::npos);
    EXPECT_NE(str.find("scalars:"), std::string::npos);
    EXPECT_NE(str.find("vector_groups:"), std::string::npos);
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
