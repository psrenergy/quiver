#include "test_sandbox.h"

#include <algorithm>

TEST_F(SandboxTest, CreateElement) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 42, some_float = 3.14 })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers[0], 42);
}

TEST_F(SandboxTest, CreateElementWithArrays) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    // Note: vector columns in the same table must have the same length
    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", {
            label = "Item 1",
            value_int = {1, 2, 3},
            value_float = {1.5, 2.5, 3.5}
        })
    )");

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0], (std::vector<std::optional<int64_t>>{1, 2, 3}));

    auto floats = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(floats.size(), 1);
    EXPECT_EQ(floats[0], (std::vector<std::optional<double>>{1.5, 2.5, 3.5}));
}

// On create the core skips an empty array before looking up its table, so a misspelled one passes.
TEST_F(SandboxTest, CreateElementSkipsEmptyArray) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "x", value_int = {}, typo = {} })
    )");

    EXPECT_EQ(db.read_scalar_strings("Collection", "label"), (std::vector<std::optional<std::string>>{"x"}));
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Collection_vector_values"), 0);
}

TEST_F(SandboxTest, CreateElementWithOnlyLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1" })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");
}

TEST_F(SandboxTest, CreateElementMixedTypes) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", {
            label = "Item 1",
            some_integer = 42,
            some_float = 3.14
        })
    )");

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers.size(), 1);
    EXPECT_EQ(integers[0], 42);

    auto floats = db.read_scalar_floats("Collection", "some_float");
    EXPECT_EQ(floats.size(), 1);
    EXPECT_DOUBLE_EQ(*floats[0], 3.14);
}

TEST_F(SandboxTest, CreateElementMissingLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    // Attempting to create element without required label should fail
    expect_sandbox_error(lua, R"(db:create_element("Collection", { some_integer = 42 }))", "NOT NULL constraint failed");
}

TEST_F(SandboxTest, CreateElementTrimsWhitespace) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", {
            label = "  Item 1  ",
            tag = {"  important  ", "	urgent\n", " review "}
        })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 1);
    auto tags = sets[0];
    std::sort(tags.begin(), tags.end());
    EXPECT_EQ(tags, (std::vector<std::optional<std::string>>{"important", "review", "urgent"}));
}

TEST_F(SandboxTest, CreateElementWithSpecialCharactersInLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Test's \"special\" chars: <>&" })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Test's \"special\" chars: <>&");
}

TEST_F(SandboxTest, CreateElementInvalidCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    expect_sandbox_error(
        lua,
        R"(db:create_element("NonexistentCollection", { label = "Test" }))",
        "Cannot create_element: collection not found"
    );
}

TEST_F(SandboxTest, CreateElementUnsupportedAttributeTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    // A function, not a boolean: a boolean is INTEGER 1/0 on every write path now (see the boolean
    // tests below). What must still throw is a value with no SQL counterpart at all.
    try {
        lua.run(R"(db:create_element("Configuration", { label = "Item", enabled = print }))");
        FAIL() << "expected unsupported attribute type to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot create_element: attribute 'enabled'"), std::string::npos)
            << e.what();
    }
}

TEST_F(SandboxTest, CreateElementUnsupportedArrayElementTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    try {
        lua.run(R"(db:create_element("Configuration", { label = "Item", tags = { print, print } }))");
        FAIL() << "expected unsupported array element type to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot create_element: array 'tags'"), std::string::npos) << e.what();
    }
}

// ============================================================================
// Boolean input. SQLite has no boolean type, so a Lua boolean is INTEGER 1/0 wherever an integer
// is accepted. Lua has no boolean *readers* (deliberate — root AGENTS.md), so these read back
// through the integer readers.
// ============================================================================

TEST_F(SandboxTest, CreateElementBooleanAttributeStoresInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    lua.run(R"(
        db:create_element("Collection", { label = "True", some_integer = true })
        db:create_element("Collection", { label = "False", some_integer = false })
    )");

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    ASSERT_EQ(integers.size(), 2);
    EXPECT_EQ(integers[0], 1);
    EXPECT_EQ(integers[1], 0);
}

TEST_F(SandboxTest, CreateElementBooleanArrayStoresIntegers) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    lua.run(R"(db:create_element("Collection", { label = "Item", value_int = { true, false, true } }))");

    auto id = db.read_element_ids("Collection")[0];
    EXPECT_EQ(
        db.read_vector_integers_by_id("Collection", "value_int", id),
        (std::vector<std::optional<int64_t>>{1, 0, 1})
    );
}

TEST_F(SandboxTest, CreateElementMixedIntegerAndBooleanArray) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    // Dispatch picks the integer helper from cell 1; every later boolean cell must still coerce.
    // Before lua_cell_to_int64 this silently stored 0 for the boolean in release builds, where
    // SOL_SAFE_GETTER is off and the unchecked get<int64_t> returned 0 instead of throwing.
    lua.run(R"(db:create_element("Collection", { label = "Item", value_int = { 7, true, false } }))");

    auto id = db.read_element_ids("Collection")[0];
    EXPECT_EQ(
        db.read_vector_integers_by_id("Collection", "value_int", id),
        (std::vector<std::optional<int64_t>>{7, 1, 0})
    );
}

TEST_F(SandboxTest, UpdateElementBooleanAttributeStoresInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    lua.run(R"(
        local id = db:create_element("Collection", { label = "Item", some_integer = 42 })
        db:update_element("Collection", id, { some_integer = true })
    )");

    EXPECT_EQ(db.read_scalar_integers("Collection", "some_integer")[0], 1);
}

TEST_F(SandboxTest, UpdateVectorGroupBooleanCellsStoreIntegers) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    lua.run(R"(
        local id = db:create_element("Collection", { label = "Item" })
        db:update_vector_group("Collection", "values", id, { value_int = { true, false } })
    )");

    auto id = db.read_element_ids("Collection")[0];
    EXPECT_EQ(
        db.read_vector_integers_by_id("Collection", "value_int", id),
        (std::vector<std::optional<int64_t>>{1, 0})
    );
}

TEST_F(SandboxTest, UpsertTimeSeriesRowBooleanStoresInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    // An int64 is accepted for a REAL column (int-for-REAL coercion), so a boolean is too.
    lua.run(R"(
        local id = db:create_element("Collection", { label = "Item" })
        db:upsert_time_series_row("Collection", "data", id, { date_time = "2024-01-01T00:00:00", value = true })
    )");

    auto id = db.read_element_ids("Collection")[0];
    auto rows = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(std::get<double>(rows[0].at("value")), 1.0);
}

TEST_F(SandboxTest, CreateElementMixedFloatAndBooleanArray) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    // The float sibling of CreateElementMixedIntegerAndBooleanArray: dispatch picks the double
    // helper from cell 1, and every later boolean cell must still coerce (int-for-REAL coercion).
    lua.run(R"(db:create_element("Collection", { label = "Item", value_float = { 1.5, true, false } }))");

    auto id = db.read_element_ids("Collection")[0];
    EXPECT_EQ(
        db.read_vector_floats_by_id("Collection", "value_float", id),
        (std::vector<std::optional<double>>{1.5, 1.0, 0.0})
    );
}

TEST_F(SandboxTest, CreateElementArrayCellTypeMismatchThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    // A cell that fits no element type is a Pattern 1 rejection naming the array and the cell —
    // not a raw sol2 message, and never a silent placeholder (the unchecked sol2 getters are only
    // checked while SOL_SAFE_GETTER is on, i.e. debug builds).
    for (
        const char* script :
        {R"(db:create_element("Collection", { label = "I", tag = { "a", true } }))",
         R"(db:create_element("Collection", { label = "I", tag = { "a", 1 } }))",
         R"(db:create_element("Collection", { label = "I", value_int = { 1, "zz" } }))"}
    ) {
        try {
            lua.run(script);
            FAIL() << "expected a mismatched array cell to throw: " << script;
        } catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find("Cannot create_element: array '"), std::string::npos) << e.what();
            EXPECT_NE(std::string(e.what()).find("cell #2 has unsupported Lua type"), std::string::npos) << e.what();
        }
    }
}

TEST_F(SandboxTest, CreateElementRejectsNonTableElement) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    expect_sandbox_error(
        lua,
        R"(db:create_element("Collection", 5))",
        "Cannot create_element: element_table must be a table, got number"
    );
    expect_sandbox_error(
        lua,
        R"(db:create_element("Collection", db))",
        "Cannot create_element: element_table must be a table, got userdata"
    );
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
}

// A userdata attribute value used to be walked as an array and reach the script as sol2's raw
// usertype text.
TEST_F(SandboxTest, CreateElementRejectsUserdataAttribute) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    expect_sandbox_error(
        lua,
        R"(db:create_element("Collection", { label = "x", some_integer = db }))",
        "Cannot create_element: attribute 'some_integer' must be a value or a table, got userdata"
    );
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
}

// A number key used to be spelled as text in Release, and a boolean key had no text at all.
TEST_F(SandboxTest, CreateElementRejectsNonStringAttributeName) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox lua(db);

    expect_sandbox_error(
        lua,
        R"(db:create_element("Collection", { "x" }))",
        "Cannot create_element: attribute name must be a string, got number"
    );
    expect_sandbox_error(
        lua,
        R"(db:create_element("Collection", { label = "y", [true] = 1 }))",
        "Cannot create_element: attribute name must be a string, got boolean"
    );
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
}
