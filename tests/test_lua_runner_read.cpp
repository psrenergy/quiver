#include "test_lua_runner.h"

TEST_F(LuaRunnerTest, ReadScalarStrings) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Test Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));

    quiver::LuaRunner lua(db);

    // Read from Lua and verify count
    lua.run(R"(
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels")
        assert(labels[1] == "Item 1", "First label mismatch")
        assert(labels[2] == "Item 2", "Second label mismatch")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarIntegers) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{20}));
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_integer", int64_t{30}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local integers = db:read_scalar_integers("Collection", "some_integer")
        assert(#integers == 3, "Expected 3 integers, got " .. #integers)
        assert(integers[1] == 10, "First integer should be 10")
        assert(integers[2] == 20, "Second integer should be 20")
        assert(integers[3] == 30, "Third integer should be 30")

        -- Test sum
        local sum = 0
        for i, v in ipairs(integers) do
            sum = sum + v
        end
        assert(sum == 60, "Sum should be 60, got " .. sum)
    )");
}

TEST_F(LuaRunnerTest, ReadScalarFloats) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 1.5));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_float", 2.5));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local floats = db:read_scalar_floats("Collection", "some_float")
        assert(#floats == 2, "Expected 2 floats")
        assert(floats[1] == 1.5, "First float should be 1.5")
        assert(floats[2] == 2.5, "Second float should be 2.5")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarFloatsPreservesNulls) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    // some_float has no default, so an unset value is SQL NULL.
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 1.5));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));  // NULL float
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_float", 3.5));

    quiver::LuaRunner lua(db);

    // One entry per element; the NULL must occupy a slot (nil), not be dropped.
    // (# is unreliable on a table with nil holes, so assert by explicit index.)
    lua.run(R"(
        local floats = db:read_scalar_floats("Collection", "some_float")
        assert(floats[1] == 1.5, "floats[1] should be 1.5")
        assert(floats[2] == nil, "floats[2] should be nil for the NULL element")
        assert(floats[3] == 3.5, "floats[3] should be 3.5")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarIntegersPreservesNulls) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));  // NULL integer
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_integer", int64_t{30}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local ints = db:read_scalar_integers("Collection", "some_integer")
        assert(ints[1] == 10, "ints[1] should be 10")
        assert(ints[2] == nil, "ints[2] should be nil for the NULL element")
        assert(ints[3] == 30, "ints[3] should be 30")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarStringsPreservesNulls) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("all_types.sql"));

    db.create_element("AllTypes", quiver::Element().set("label", "a").set("some_text", "hello"));
    db.create_element("AllTypes", quiver::Element().set("label", "b"));  // NULL string
    db.create_element("AllTypes", quiver::Element().set("label", "c").set("some_text", "world"));
    db.create_element("AllTypes", quiver::Element().set("label", "d").set("some_text", ""));  // empty, not NULL

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local texts = db:read_scalar_strings("AllTypes", "some_text")
        assert(texts[1] == "hello", "texts[1] should be hello")
        assert(texts[2] == nil, "texts[2] should be nil for the NULL element")
        assert(texts[3] == "world", "texts[3] should be world")
        assert(texts[4] == "", "texts[4] should be an empty string, not nil")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarFloatsAllNull) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));

    quiver::LuaRunner lua(db);

    // All-NULL column: every slot is nil. read_element_ids is the count authority
    // (# is unreliable over an all-nil table).
    lua.run(R"(
        local ids = db:read_element_ids("Collection")
        assert(#ids == 2, "expected 2 element ids")
        local floats = db:read_scalar_floats("Collection", "some_float")
        assert(floats[1] == nil, "floats[1] should be nil")
        assert(floats[2] == nil, "floats[2] should be nil")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorIntegers) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{10, 20}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 2, "Expected 2 vectors")

        -- Check first vector
        assert(#vectors[1] == 3, "First vector should have 3 elements")
        assert(vectors[1][1] == 1, "First vector[1] should be 1")
        assert(vectors[1][2] == 2, "First vector[2] should be 2")
        assert(vectors[1][3] == 3, "First vector[3] should be 3")

        -- Check second vector
        assert(#vectors[2] == 2, "Second vector should have 2 elements")
        assert(vectors[2][1] == 10, "Second vector[1] should be 10")
        assert(vectors[2][2] == 20, "Second vector[2] should be 20")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorFloats) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_float", std::vector<double>{1.1, 2.2, 3.3}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_floats("Collection", "value_float")
        assert(#vectors == 1, "Expected 1 vector")
        assert(#vectors[1] == 3, "Vector should have 3 elements")
        assert(vectors[1][1] == 1.1, "vector[1] should be 1.1")
        assert(vectors[1][2] == 2.2, "vector[2] should be 2.2")
        assert(vectors[1][3] == 3.3, "vector[3] should be 3.3")
    )");
}

TEST_F(LuaRunnerTest, ReadEmptyVector) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    // Create element without vector data
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    // Reading vectors from elements that don't have vector data should return empty
    lua.run(R"(
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 0, "Expected 0 vectors for elements without vector data")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarStringsEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // No Collection elements created, should return empty table
    lua.run(R"(
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 0, "Expected empty table, got " .. #labels .. " items")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarIntegersEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local integers = db:read_scalar_integers("Collection", "some_integer")
        assert(#integers == 0, "Expected empty table, got " .. #integers .. " items")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorIntegersEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 0, "Expected empty table, got " .. #vectors .. " items")
    )");
}

TEST_F(LuaRunnerTest, ReadSetStringsAll) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("tag", std::vector<std::string>{"a", "b"}));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 2").set("tag", std::vector<std::string>{"c", "d", "e"}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local sets = db:read_set_strings("Collection", "tag")
        assert(#sets == 2, "Expected 2 outer elements, got " .. #sets)
        assert(#sets[1] == 2, "First set should have 2 tags, got " .. #sets[1])
        assert(#sets[2] == 3, "Second set should have 3 tags, got " .. #sets[2])
    )");
}

TEST_F(LuaRunnerTest, ReadElementIds) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    int64_t id2 = db.create_element("Collection", quiver::Element().set("label", "Item 2"));
    int64_t id3 = db.create_element("Collection", quiver::Element().set("label", "Item 3"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local ids = db:read_element_ids("Collection")
        assert(#ids == 3, "Expected 3 Ids, got " .. #ids)
        assert(ids[1] == )" +
                         std::to_string(id1) + R"(, "First Id mismatch")
        assert(ids[2] == )" +
                         std::to_string(id2) + R"(, "Second Id mismatch")
        assert(ids[3] == )" +
                         std::to_string(id3) + R"(, "Third Id mismatch")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadElementIdsEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local ids = db:read_element_ids("Collection")
        assert(#ids == 0, "Expected 0 Ids for empty collection, got " .. #ids)
    )");
}

TEST_F(LuaRunnerTest, ReadScalarsById) {
    auto db = quiver::Database::from_schema(
        ":memory:", collections_schema, {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}).set("some_float", 3.14));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local scalars = db:read_scalars_by_id("Collection", )" +
                         std::to_string(id) + R"()

        -- Verify label (TEXT)
        assert(scalars.label == "Item 1", "Expected label 'Item 1', got " .. tostring(scalars.label))
        assert(type(scalars.label) == "string", "label should be string type")

        -- Verify some_integer (INTEGER)
        assert(scalars.some_integer == 42, "Expected some_integer 42, got " .. tostring(scalars.some_integer))
        assert(type(scalars.some_integer) == "number", "some_integer should be number type")

        -- Verify some_float (REAL)
        assert(scalars.some_float == 3.14, "Expected some_float 3.14, got " .. tostring(scalars.some_float))
        assert(type(scalars.some_float) == "number", "some_float should be number type")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadVectorsById) {
    // Use basic.sql which has no vector groups -- verifies the binding is callable
    // and returns an empty table. Note: collections.sql has multi-column vector groups
    // where group_name != column_name, which is a known limitation of the composite helper.
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
    int64_t id = db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local vectors = db:read_vectors_by_id("Configuration", )" +
                         std::to_string(id) + R"()

        -- basic.sql has no vector groups, so result should be an empty table
        assert(type(vectors) == "table", "Expected table type, got " .. type(vectors))

        -- Verify no keys in the result
        local count = 0
        for _ in pairs(vectors) do count = count + 1 end
        assert(count == 0, "Expected empty table for schema with no vector groups, got " .. count .. " entries")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadSetsById) {
    // Use basic.sql which has no set groups -- verifies the binding is callable
    // and returns an empty table. Note: collections.sql has set groups where
    // group_name != column_name, which is a known limitation of the composite helper.
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
    int64_t id = db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local sets = db:read_sets_by_id("Configuration", )" +
                         std::to_string(id) + R"()

        -- basic.sql has no set groups, so result should be an empty table
        assert(type(sets) == "table", "Expected table type, got " .. type(sets))

        -- Verify no keys in the result
        local count = 0
        for _ in pairs(sets) do count = count + 1 end
        assert(count == 0, "Expected empty table for schema with no set groups, got " .. count .. " entries")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadVectorsByIdWithData) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("composite_helpers.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    int64_t id = db.create_element("Items",
                                   quiver::Element()
                                       .set("label", "Item 1")
                                       .set("amount", std::vector<int64_t>{10, 20, 30})
                                       .set("score", std::vector<double>{1.1, 2.2})
                                       .set("note", std::vector<std::string>{"hello", "world"}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local vectors = db:read_vectors_by_id("Items", )" +
                         std::to_string(id) + R"()

        -- Verify 3 vector groups
        local count = 0
        for _ in pairs(vectors) do count = count + 1 end
        assert(count == 3, "Expected 3 vector groups, got " .. count)

        -- Verify integer vector
        assert(vectors.amount ~= nil, "Missing 'amount' vector group")
        assert(#vectors.amount == 3, "Expected 3 amount values, got " .. #vectors.amount)
        assert(vectors.amount[1] == 10, "amount[1] expected 10, got " .. tostring(vectors.amount[1]))
        assert(vectors.amount[2] == 20, "amount[2] expected 20, got " .. tostring(vectors.amount[2]))
        assert(vectors.amount[3] == 30, "amount[3] expected 30, got " .. tostring(vectors.amount[3]))

        -- Verify float vector
        assert(vectors.score ~= nil, "Missing 'score' vector group")
        assert(#vectors.score == 2, "Expected 2 score values, got " .. #vectors.score)
        local d1 = vectors.score[1] - 1.1
        assert(d1 > -1e-9 and d1 < 1e-9, "score[1] expected 1.1")
        local d2 = vectors.score[2] - 2.2
        assert(d2 > -1e-9 and d2 < 1e-9, "score[2] expected 2.2")

        -- Verify string vector
        assert(vectors.note ~= nil, "Missing 'note' vector group")
        assert(#vectors.note == 2, "Expected 2 note values, got " .. #vectors.note)
        assert(vectors.note[1] == "hello", "note[1] expected 'hello', got " .. tostring(vectors.note[1]))
        assert(vectors.note[2] == "world", "note[2] expected 'world', got " .. tostring(vectors.note[2]))
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadSetsByIdWithData) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("composite_helpers.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    int64_t id = db.create_element("Items",
                                   quiver::Element()
                                       .set("label", "Item 1")
                                       .set("code", std::vector<int64_t>{10, 20, 30})
                                       .set("weight", std::vector<double>{1.1, 2.2})
                                       .set("tag", std::vector<std::string>{"alpha", "beta"}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local sets = db:read_sets_by_id("Items", )" +
                         std::to_string(id) + R"()

        -- Verify 3 set groups
        local count = 0
        for _ in pairs(sets) do count = count + 1 end
        assert(count == 3, "Expected 3 set groups, got " .. count)

        -- Verify integer set
        assert(sets.code ~= nil, "Missing 'code' set group")
        assert(#sets.code == 3, "Expected 3 code values, got " .. #sets.code)

        -- Verify float set
        assert(sets.weight ~= nil, "Missing 'weight' set group")
        assert(#sets.weight == 2, "Expected 2 weight values, got " .. #sets.weight)

        -- Verify string set
        assert(sets.tag ~= nil, "Missing 'tag' set group")
        assert(#sets.tag == 2, "Expected 2 tag values, got " .. #sets.tag)
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadElementById) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("composite_helpers.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    int64_t id = db.create_element("Items",
                                   quiver::Element()
                                       .set("label", "Item 1")
                                       .set("amount", std::vector<int64_t>{10, 20, 30})
                                       .set("score", std::vector<double>{1.1, 2.2})
                                       .set("note", std::vector<std::string>{"hello", "world"})
                                       .set("code", std::vector<int64_t>{5, 6})
                                       .set("weight", std::vector<double>{9.9})
                                       .set("tag", std::vector<std::string>{"alpha", "beta"}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local elem = db:read_element_by_id("Items", )" +
                         std::to_string(id) + R"()

        -- Verify scalars
        assert(elem.label == "Item 1", "Expected label 'Item 1', got " .. tostring(elem.label))

        -- Verify vectors
        assert(elem.amount ~= nil, "Missing vector 'amount'")
        assert(#elem.amount == 3, "Expected 3 amount values, got " .. #elem.amount)
        assert(elem.amount[1] == 10, "Expected amount[1] == 10")

        -- Verify sets
        assert(elem.tag ~= nil, "Missing set 'tag'")
        assert(#elem.tag == 2, "Expected 2 tag values, got " .. #elem.tag)

        -- Verify all keys present (label + id + vectors + sets = 1 scalar + 3 vectors + 3 sets = 7+)
        local count = 0
        for _ in pairs(elem) do count = count + 1 end
        assert(count >= 7, "Expected at least 7 keys, got " .. count)
    )";
    lua.run(script);
}
