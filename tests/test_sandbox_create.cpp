#include "test_sandbox.h"

#include <algorithm>
#include <utility>

TEST_F(SandboxTest, CreateElement) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
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
    quiver::Sandbox sandbox(db);

    // Note: vector columns in the same table must have the same length
    sandbox.run(R"(
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

TEST_F(SandboxTest, CreateElementRealArraysPreserveCellTypes) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Collection", { label = "Integer first", value_float = {1, 2.5} })
        db:create_element("Collection", { label = "Float first", value_float = {2.5, 1} })
        db:create_element("Collection", { label = "Boolean first", value_float = {true, 2.5, false} })
        db:create_element("Collection", { label = "Float and boolean", value_float = {2.5, true, false} })
    )");

    EXPECT_EQ(
        db.read_vector_floats("Collection", "value_float"),
        (std::vector<std::vector<std::optional<double>>>{{1.0, 2.5}, {2.5, 1.0}, {1.0, 2.5, 0.0}, {2.5, 1.0, 0.0}})
    );
}

TEST_F(SandboxTest, CreateElementRejectsArrayHoleMaskedByExtraKey) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(
            local values = {1, 2, 3, 4, 5, 6, 7, 8}
            values[2] = nil
            values.extra = 3
            db:create_element("Collection", { label = "Item", value_float = values })
        )",
        "Cannot create_element: array 'value_float' has a nil hole or a non-integer key"
    );
    EXPECT_EQ(db.number_of_elements("Collection"), 0);
}

// On create the core skips an empty array before looking up its table, so a misspelled one passes.
TEST_F(SandboxTest, CreateElementSkipsEmptyArray) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "x", value_int = {}, typo = {} })
    )");

    EXPECT_EQ(db.read_scalar_strings("Collection", "label"), (std::vector<std::optional<std::string>>{"x"}));
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Collection_vector_values"), 0);
}

TEST_F(SandboxTest, CreateElementWithOnlyLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1" })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");
}

TEST_F(SandboxTest, CreateElementMixedTypes) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
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

    quiver::Sandbox sandbox(db);

    // Attempting to create element without required label should fail
    expect_sandbox_error(
        sandbox,
        R"(db:create_element("Collection", { some_integer = 42 }))",
        "NOT NULL constraint failed"
    );
}

TEST_F(SandboxTest, CreateElementTrimsWhitespace) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
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
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
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

    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:create_element("NonexistentCollection", { label = "Test" }))",
        "Cannot create_element: collection not found"
    );
}

TEST_F(SandboxTest, CreateElementUnsupportedAttributeTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox sandbox(db);

    // A function, not a boolean: a boolean is INTEGER 1/0 on every write path now (see the boolean
    // tests below). What must still throw is a value with no SQL counterpart at all.
    try {
        sandbox.run(R"(db:create_element("Configuration", { label = "Item", enabled = print }))");
        FAIL() << "expected unsupported attribute type to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot create_element: attribute 'enabled'"), std::string::npos)
            << e.what();
    }
}

TEST_F(SandboxTest, CreateElementUnsupportedArrayElementTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox sandbox(db);

    try {
        sandbox.run(R"(db:create_element("Configuration", { label = "Item", tags = { print, print } }))");
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

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
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

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(db:create_element("Collection", { label = "Item", value_int = { true, false, true } }))");

    auto id = db.read_element_ids("Collection")[0];
    EXPECT_EQ(
        db.read_vector_integers_by_id("Collection", "value_int", id),
        (std::vector<std::optional<int64_t>>{1, 0, 1})
    );
}

TEST_F(SandboxTest, CreateElementMixedIntegerAndBooleanArray) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox sandbox(db);

    // Every boolean cell maps to INTEGER 1/0 alongside the existing integer cells.
    sandbox.run(R"(db:create_element("Collection", { label = "Item", value_int = { 7, true, false } }))");

    auto id = db.read_element_ids("Collection")[0];
    EXPECT_EQ(
        db.read_vector_integers_by_id("Collection", "value_int", id),
        (std::vector<std::optional<int64_t>>{7, 1, 0})
    );
}

TEST_F(SandboxTest, UpdateElementBooleanAttributeStoresInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local id = db:create_element("Collection", { label = "Item", some_integer = 42 })
        db:update_element("Collection", id, { some_integer = true })
    )");

    EXPECT_EQ(db.read_scalar_integers("Collection", "some_integer")[0], 1);
}

TEST_F(SandboxTest, UpdateVectorGroupBooleanCellsStoreIntegers) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
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

    quiver::Sandbox sandbox(db);

    // An int64 is accepted for a REAL column (int-for-REAL coercion), so a boolean is too.
    sandbox.run(R"(
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

    quiver::Sandbox sandbox(db);

    // Booleans reach the core as INTEGER 1/0, accepted through int-for-REAL coercion.
    sandbox.run(R"(db:create_element("Collection", { label = "Item", value_float = { 1.5, true, false } }))");

    auto id = db.read_element_ids("Collection")[0];
    EXPECT_EQ(
        db.read_vector_floats_by_id("Collection", "value_float", id),
        (std::vector<std::optional<double>>{1.5, 1.0, 0.0})
    );
}

TEST_F(SandboxTest, CreateElementArrayCellTypeMismatchThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox sandbox(db);

    // Supported Lua values keep their types; C++ supplies the schema/type errors.
    const std::pair<const char*, const char*> cases[] = {
        {R"(db:create_element("Collection", { label = "I", tag = { "a", true } }))",
         "Cannot create_element: type mismatch for array 'tag' index 1: expected TEXT, got INTEGER"},
        {R"(db:create_element("Collection", { label = "I", tag = { "a", 1 } }))",
         "Cannot create_element: type mismatch for array 'tag' index 1: expected TEXT, got INTEGER"},
        {R"(db:create_element("Collection", { label = "I", value_int = { 1, "zz" } }))",
         "Cannot create_element: type mismatch for column 'value_int': expected INTEGER, got TEXT"},
        {R"(db:create_element("Collection", { label = "I", value_int = { 1, 2.5 } }))",
         "Cannot create_element: type mismatch for array 'value_int' index 1: expected INTEGER, got REAL"},
        {R"(db:create_element("Collection", { label = "I", value_int = { 2.5, 1 } }))",
         "Cannot create_element: type mismatch for array 'value_int' index 0: expected INTEGER, got REAL"},
    };
    for (const auto& [script, message] : cases) {
        expect_sandbox_error(sandbox, script, message);
    }
    EXPECT_EQ(db.number_of_elements("Collection"), 0);
}

TEST_F(SandboxTest, CreateElementRejectsNonTableElement) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:create_element("Collection", 5))",
        "Cannot create_element: element_table must be a table, got number"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:create_element("Collection", db))",
        "Cannot create_element: element_table must be a table, got userdata"
    );
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
}

// A userdata attribute value used to be walked as an array and reach the script as sol2's raw
// usertype text.
TEST_F(SandboxTest, CreateElementRejectsUserdataAttribute) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:create_element("Collection", { label = "x", some_integer = db }))",
        "Cannot create_element: attribute 'some_integer' must be a value or a table, got userdata"
    );
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
}

// A number key used to be spelled as text in Release, and a boolean key had no text at all.
TEST_F(SandboxTest, CreateElementRejectsNonStringAttributeName) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:create_element("Collection", { "x" }))",
        "Cannot create_element: attribute name must be a string, got number"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:create_element("Collection", { label = "y", [true] = 1 }))",
        "Cannot create_element: attribute name must be a string, got boolean"
    );
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
}
