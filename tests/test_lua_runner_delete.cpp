#include "test_lua_runner.h"

#include <algorithm>

TEST_F(LuaRunnerTest, DeleteElementById) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));
    db.create_element("Collection", quiver::Element().set("label", "Item 3"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local ids = db:read_element_ids("Collection")
        assert(#ids == 3, "Expected 3 elements before delete")

        db:delete_element("Collection", 2)

        ids = db:read_element_ids("Collection")
        assert(#ids == 2, "Expected 2 elements after delete")
    )");

    // Verify from C++ side
    auto ids = db.read_element_ids("Collection");
    EXPECT_EQ(ids.size(), 2);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 3);
}

TEST_F(LuaRunnerTest, DeleteElementByIdWithVectorData) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{4, 5, 6}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:delete_element("Collection", 1)

        local ids = db:read_element_ids("Collection")
        assert(#ids == 1, "Expected 1 element after delete")
        assert(ids[1] == 2, "Remaining element should have Id 2")
    )");

    // Verify vector data of remaining element is intact
    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{4, 5, 6}));
}

TEST_F(LuaRunnerTest, DeleteElementByIdNonExistent) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    // Deleting non-existent element should succeed silently (idempotent)
    lua.run(R"(
        db:delete_element("Collection", 999)

        local ids = db:read_element_ids("Collection")
        assert(#ids == 1, "Original element should still exist")
    )");
}

TEST_F(LuaRunnerTest, DeleteElementByIdOtherElementsUnchanged) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_integer", int64_t{300}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:delete_element("Collection", 2)

        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels after delete")

        local integers = db:read_scalar_integers("Collection", "some_integer")
        assert(#integers == 2, "Expected 2 integers after delete")

        local sum = integers[1] + integers[2]
        assert(sum == 400, "Sum should be 400 (100 + 300), got " .. sum)
    )");

    // Verify from C++ side
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), "Item 1") != labels.end());
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), "Item 3") != labels.end());
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), "Item 2") == labels.end());
}
