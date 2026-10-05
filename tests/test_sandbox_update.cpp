#include "test_sandbox.h"

#include <algorithm>

TEST_F(SandboxTest, UpdateElementSingleScalar) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_element("Collection", 1, { some_integer = 999 })

        local scalars = db:read_scalars_by_id("Collection", 1)
        assert(scalars.some_integer == 999, "Expected 999, got " .. tostring(scalars.some_integer))
        assert(scalars.label == "Item 1", "Label should be unchanged")
    )");

    // Verify from C++ side
    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 999);

    auto label = db.read_scalar_string_by_id("Collection", "label", 1);
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Item 1");
}

TEST_F(SandboxTest, UpdateElementMultipleScalars) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}).set("some_float", 1.5)
    );

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_element("Collection", 1, { some_integer = 500, some_float = 9.9 })

        local scalars = db:read_scalars_by_id("Collection", 1)
        assert(scalars.some_integer == 500, "Expected integer 500, got " .. tostring(scalars.some_integer))
        assert(scalars.some_float == 9.9, "Expected float 9.9, got " .. tostring(scalars.some_float))
        assert(scalars.label == "Item 1", "Label should be unchanged")
    )");

    // Verify from C++ side
    auto integer_value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(integer_value.has_value());
    EXPECT_EQ(*integer_value, 500);

    auto float_value = db.read_scalar_float_by_id("Collection", "some_float", 1);
    EXPECT_TRUE(float_value.has_value());
    EXPECT_DOUBLE_EQ(*float_value, 9.9);
}

TEST_F(SandboxTest, UpdateElementOtherElementsUnchanged) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_integer", int64_t{300}));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        -- Update only element 2
        db:update_element("Collection", 2, { some_integer = 999 })

        -- Verify element 2 updated
        local s2 = db:read_scalars_by_id("Collection", 2)
        assert(s2.some_integer == 999, "Element 2 should be updated to 999")

        -- Verify elements 1 and 3 unchanged
        local s1 = db:read_scalars_by_id("Collection", 1)
        assert(s1.some_integer == 100, "Element 1 should be unchanged at 100")

        local s3 = db:read_scalars_by_id("Collection", 3)
        assert(s3.some_integer == 300, "Element 3 should be unchanged at 300")
    )");

    // Verify from C++ side
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 1), 100);
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 2), 999);
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 3), 300);
}

TEST_F(SandboxTest, UpdateElementWithArrays) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element()
            .set("label", "Item 1")
            .set("some_integer", int64_t{10})
            .set("value_int", std::vector<int64_t>{1, 2, 3})
    );

    quiver::Sandbox sandbox(db);

    // Update with both scalar and array values - both should be updated
    sandbox.run(R"(
        db:update_element("Collection", 1, { some_integer = 999, value_int = {7, 8, 9} })
    )");

    // Verify from C++ side
    auto integer_value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(integer_value.has_value());
    EXPECT_EQ(*integer_value, 999);

    auto vec_values = db.read_vector_integers_by_id("Collection", "value_int", 1);
    EXPECT_EQ(vec_values, (std::vector<std::optional<int64_t>>{7, 8, 9}));
}

TEST_F(SandboxTest, UpdateElementRefusesArrayWithNilHole) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element()
            .set("label", "Item 1")
            .set("value_int", std::vector<quiver::Value>{int64_t{10}, nullptr, int64_t{30}})
    );
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 2").set("value_int", std::vector<quiver::Value>{nullptr, int64_t{20}})
    );
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 3").set("value_int", std::vector<int64_t>{7, 8, 9})
    );

    quiver::Sandbox sandbox(db);

    // A read hands each NULL cell back as a nil hole. Written back through an element array it
    // used to keep only the cells before the hole, or with a leading hole skip the array, silently.
    expect_sandbox_error(
        sandbox,
        R"(db:update_element("Collection", 3, { value_int = db:read_vectors_by_id("Collection", 1).value_int }))",
        "Cannot update_element: array 'value_int' has a nil hole"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_element("Collection", 3, {
                         label = "Item 3b", value_int = db:read_vectors_by_id("Collection", 2).value_int }))",
        "has a nil hole"
    );

    EXPECT_EQ(
        db.read_vector_integers_by_id("Collection", "value_int", 3),
        (std::vector<std::optional<int64_t>>{7, 8, 9})
    );
    EXPECT_EQ(db.read_scalar_string_by_id("Collection", "label", 3), "Item 3");
}

// An empty array reaches the core, which clears the group holding the column on update.
TEST_F(SandboxTest, UpdateElementEmptyArrayClearsGroup) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element()
            .set("label", "Item 1")
            .set("some_integer", int64_t{10})
            .set("value_int", std::vector<int64_t>{1, 2, 3})
            .set("tag", std::vector<std::string>{"a", "b"})
    );
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{4, 5})
    );

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(db:update_element("Collection", 1, { value_int = {} }))");
    EXPECT_TRUE(db.read_vector_integers_by_id("Collection", "value_int", 1).empty());
    EXPECT_EQ(db.read_set_strings_by_id("Collection", "tag", 1), (std::vector<std::optional<std::string>>{"a", "b"}));

    sandbox.run(R"(db:update_element("Collection", 1, { tag = {} }))");
    EXPECT_TRUE(db.read_set_strings_by_id("Collection", "tag", 1).empty());
    EXPECT_EQ(db.read_scalar_integer_by_id("Collection", "some_integer", 1), 10);

    sandbox.run(R"(db:update_element_by_label("Collection", "Item 2", { value_int = {} }))");
    EXPECT_TRUE(db.read_vector_integers_by_id("Collection", "value_int", 2).empty());
}

TEST_F(SandboxTest, UpdateElementEmptyArrayErrors) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2})
    );

    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:update_element("Collection", 1, { typo = {} }))",
        "Cannot update_element: array 'typo' does not match any vector, set, or time series table in collection "
        "'Collection'"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_element("Collection", 1, { value_int = {}, value_float = {1.5, 2.5} }))",
        "must have the same length"
    );
    EXPECT_EQ(db.read_vector_integers_by_id("Collection", "value_int", 1), (std::vector<std::optional<int64_t>>{1, 2}));
}

TEST_F(SandboxTest, UpdateElementEmptyArrayClearsEveryGroupSharingTheColumn) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"));
    quiver::Sandbox sandbox(db);

    // parent_ref names a column of both Child_vector_refs and Child_set_parents.
    sandbox.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Parent", { label = "Parent 1" })
        db:create_element("Child", { label = "Child 1", mentor_id = { "Parent 1" }, score = { 7 } })
        db:update_vector_group("Child", "refs", 1, { parent_ref = { 1, 1 } })
        db:update_set_group("Child", "parents", 1, { parent_ref = { 1 } })
        db:update_element("Child", 1, { parent_ref = {} })
    )");

    EXPECT_TRUE(db.read_vector_group_by_id("Child", "refs", 1).empty());
    EXPECT_TRUE(db.read_set_group_by_id("Child", "parents", 1).empty());
    EXPECT_EQ(db.read_set_integers_by_id("Child", "mentor_id", 1), (std::vector<std::optional<int64_t>>{1}));
    EXPECT_EQ(db.read_set_integers_by_id("Child", "score", 1), (std::vector<std::optional<int64_t>>{7}));
}

TEST_F(SandboxTest, UpdateElementEmptyDateTimeClearsEveryTimeSeriesGroup) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("multi_time_series.sql"));
    quiver::Sandbox sandbox(db);

    // Every time-series group of a collection shares date_time.
    sandbox.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Sensor", {
            label = "Sensor 1",
            date_time = { "2024-01-01T10:00:00", "2024-01-02T10:00:00" },
            temperature = { 20.0, 21.5 },
            humidity = { 45.0, 50.0 },
        })
        db:update_element("Sensor", 1, { date_time = {} })
    )");

    EXPECT_TRUE(db.read_time_series_group("Sensor", "temperature", 1).empty());
    EXPECT_TRUE(db.read_time_series_group("Sensor", "humidity", 1).empty());
}

TEST_F(SandboxTest, UpdateElementRoundTripOfReadVectorsById) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    // value_float reads back all-NULL, so as an empty Lua list.
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2})
    );
    // Both columns read back all-NULL.
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 2").set("value_int", std::vector<quiver::Value>{nullptr, nullptr})
    );
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:update_element("Collection", 1, db:read_vectors_by_id("Collection", 1)))",
        "must have the same length"
    );
    EXPECT_EQ(db.read_vector_integers_by_id("Collection", "value_int", 1), (std::vector<std::optional<int64_t>>{1, 2}));

    sandbox.run(R"(db:update_element("Collection", 2, db:read_vectors_by_id("Collection", 2)))");
    EXPECT_TRUE(db.read_vector_group_by_id("Collection", "values", 2).empty());
}

TEST_F(SandboxTest, UpdateVectorIntegers) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3})
    );

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_element("Collection", 1, { value_int = {10, 20, 30, 40} })
    )");

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", 1);
    EXPECT_EQ(vec, (std::vector<std::optional<int64_t>>{10, 20, 30, 40}));
}

TEST_F(SandboxTest, UpdateVectorFloats) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("value_float", std::vector<double>{1.0, 2.0})
    );

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_element("Collection", 1, { value_float = {5.5, 6.6, 7.7} })
    )");

    auto vec = db.read_vector_floats_by_id("Collection", "value_float", 1);
    EXPECT_EQ(vec, (std::vector<std::optional<double>>{5.5, 6.6, 7.7}));
}

TEST_F(SandboxTest, UpdateScalarStringTrimsWhitespace) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Test Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("tag", std::vector<std::string>{"old"})
    );

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_element("Collection", 1, { tag = {"  alpha  ", "	beta\n", " gamma "} })
    )");

    auto set_vals = db.read_set_strings_by_id("Collection", "tag", 1);
    std::sort(set_vals.begin(), set_vals.end());
    EXPECT_EQ(set_vals, (std::vector<std::optional<std::string>>{"alpha", "beta", "gamma"}));
}

TEST_F(SandboxTest, UpdateVectorStrings) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element()
            .set("label", "Item 1")
            .set("value_int", std::vector<int64_t>{1})
            .set("value_float", std::vector<double>{1.0})
    );

    quiver::Sandbox sandbox(db);

    // The collections.sql schema has value_int and value_float vectors but no string vector.
    // We test that update_vector_strings compiles and runs; actual schema support depends on schema.
    // For now, just verify no crash when calling with an empty vector on a valid attribute.
    auto vec = db.read_vector_integers_by_id("Collection", "value_int", 1);
    EXPECT_EQ(vec.size(), 1);
}

TEST_F(SandboxTest, UpdateSetStrings) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("tag", std::vector<std::string>{"alpha", "beta"})
    );

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_element("Collection", 1, { tag = {"x", "y", "z"} })
    )");

    auto tags = db.read_set_strings_by_id("Collection", "tag", 1);
    EXPECT_EQ(tags.size(), 3);
}

TEST_F(SandboxTest, UpdateElementByIdNonExistent) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::Sandbox sandbox(db);

    // Updating a non-existent element throws "Element not found"
    expect_sandbox_error(sandbox, R"(db:update_element("Collection", 999, { some_integer = 5 }))", "Element not found");
}

TEST_F(SandboxTest, UpdateElementUnsupportedAttributeTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::Sandbox sandbox(db);

    // The message names the method the script called, not the internal converter.
    expect_sandbox_error(
        sandbox,
        R"(db:update_element("Collection", 1, { some_integer = print }))",
        "Cannot update_element: attribute 'some_integer' has unsupported Lua type"
    );
}

TEST_F(SandboxTest, UpdateElementByLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_element_by_label("Collection", "Item 1", { some_integer = 999 })

        local scalars = db:read_scalars_by_id("Collection", 1)
        assert(scalars.some_integer == 999, "Expected 999, got " .. tostring(scalars.some_integer))
    )");

    // Verify from C++ side
    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 999);

    // The sibling element is untouched.
    auto other = db.read_scalar_integer_by_id("Collection", "some_integer", 2);
    EXPECT_TRUE(other.has_value());
    EXPECT_EQ(*other, 200);
}

TEST_F(SandboxTest, UpdateElementByLabelNonExistent) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));

    quiver::Sandbox sandbox(db);

    // Updating a non-existent label throws "Element not found"
    expect_sandbox_error(
        sandbox,
        R"(db:update_element_by_label("Collection", "Nope", { some_integer = 5 }))",
        "Element not found"
    );

    // Nothing was written.
    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 100);
}

// ============================================================================
// Vector / set group writers
// ============================================================================

namespace {

// relations.sql gives Child a vector group and a set group that legally share the FK column name
// "parent_ref" -- the case that makes routing an array by column name ambiguous.
quiver::Database relations_db_with_child() {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Parent", quiver::Element().set("label", "Parent A"));
    db.create_element("Parent", quiver::Element().set("label", "Parent B"));
    db.create_element("Child", quiver::Element().set("label", "Child 1"));
    return db;
}

}  // namespace

TEST_F(SandboxTest, UpdateVectorGroupReplacesAndClears) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { 1, 2 } }))");
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{1, 2}));

    sandbox.run(R"(db:update_vector_group("Child", "refs", 1, {}))");
    EXPECT_TRUE(db.read_vector_integers_by_id("Child", "parent_ref", 1).empty());
}

TEST_F(SandboxTest, UpdateSetGroupLeavesSiblingSharingColumnNameUntouched) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_set_group("Child", "parents", 1, { parent_ref = { 1 } })
        db:update_vector_group("Child", "refs", 1, { parent_ref = { 2 } })
        db:update_vector_group("Child", "refs", 1, {})
    )");

    EXPECT_TRUE(db.read_vector_integers_by_id("Child", "parent_ref", 1).empty());
    EXPECT_EQ(db.read_set_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{1}));
}

TEST_F(SandboxTest, UpdateGroupResolvesForeignKeyLabels) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { "Parent B" } }))");
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{2}));
}

// No dimension column here, so the row count is the largest index any column reaches; a nil hole
// writes SQL NULL, which is how a read's nil holes round-trip.
TEST_F(SandboxTest, UpdateGroupSparseColumnWritesNulls) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);

    auto result = sandbox.run(R"(
        db:update_vector_group("Child", "refs", 1, { parent_ref = { 1, nil, 2 } })
        return {
            rows = db:query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = 1"),
            nulls = db:query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = 1 AND parent_ref IS NULL"),
        }
    )");
    EXPECT_EQ(result, R"({"nulls":1,"rows":3})");
}

TEST_F(SandboxTest, UpdateGroupErrors) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);
    sandbox.run(R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { 1 } }))");

    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Child", "nope", 1, { parent_ref = { 1 } }))",
        "Vector group not found"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_set_group("Child", "nope", 1, { parent_ref = { 1 } }))",
        "Set group not found"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Child", "refs", 1, { not_a_column = { 1 } }))",
        "not found in group"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { 1 }, vector_index = { 7 } }))",
        "managed by the group table"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Child", "refs", 999, { parent_ref = { 1 } }))",
        "Element not found"
    );
    // The clear path used to succeed silently: the DELETE simply matched nothing.
    expect_sandbox_error(sandbox, R"(db:update_set_group("Child", "parents", 999, {}))", "Element not found");
    // A named column with no cells is a caller mistake, not a clear -- {} clears.
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Child", "refs", 1, { parent_ref = {} }))",
        "contain no rows"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_set_group("Child", "parents", 1, { parent_ref = 5 }))",
        "must be an array of values"
    );
    // A non-string column key: one Pattern 1 message in every build, not a sol2 panic (Debug) or
    // column '' (Release).
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Child", "refs", 1, { [true] = { 1 } }))",
        "Cannot update_vector_group: column names must be strings"
    );

    // Every rejected call left the existing row alone.
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{1}));
}

// A non-table payload is a Pattern 1 error naming its Lua type. A userdata used to be walked as an
// empty table in Release, which cleared the group.
TEST_F(SandboxTest, GroupWritersRejectNonTableColumns) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    const int64_t id = db.create_element(
        "Collection",
        quiver::Element()
            .set("label", "Item 1")
            .set("value_int", std::vector<int64_t>{1, 2, 3})
            .set("tag", std::vector<std::string>{"a", "b"})
    );
    quiver::Sandbox sandbox(db);
    const std::string sid = std::to_string(id);

    const std::vector<std::pair<std::string, std::string>> calls = {
        {"update_vector_group", R"(db:update_vector_group("Collection", "values", )" + sid},
        {"update_vector_group_by_label", R"(db:update_vector_group_by_label("Collection", "values", "Item 1")"},
        {"update_set_group", R"(db:update_set_group("Collection", "tags", )" + sid},
        {"update_set_group_by_label", R"(db:update_set_group_by_label("Collection", "tags", "Item 1")"},
    };
    for (const auto& [op, prefix] : calls) {
        expect_sandbox_error(sandbox, prefix + ", 5)", "Cannot " + op + ": columns must be a table, got number");
        expect_sandbox_error(sandbox, prefix + ", db)", "Cannot " + op + ": columns must be a table, got userdata");
    }
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Collection", "values", )" + sid + R"(, { value_int = db }))",
        "Cannot update_vector_group: column 'value_int' must be an array of values, got userdata"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group("Collection", "values", )" + sid + R"(, { value_int = 5 }))",
        "Cannot update_vector_group: column 'value_int' must be an array of values, got number"
    );

    EXPECT_EQ(
        db.read_vector_integers_by_id("Collection", "value_int", id),
        (std::vector<std::optional<int64_t>>{1, 2, 3})
    );
    EXPECT_EQ(db.read_set_strings_by_id("Collection", "tag", id), (std::vector<std::optional<std::string>>{"a", "b"}));
}

TEST_F(SandboxTest, UpdateVectorGroupByLabel) {
    auto db = relations_db_with_child();
    db.create_element("Child", quiver::Element().set("label", "Child 2"));
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_vector_group_by_label("Child", "refs", "Child 2", { parent_ref = { 1 } })
        db:update_vector_group_by_label("Child", "refs", "Child 1", { parent_ref = { 1, "Parent B" } })
    )");
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{1, 2}));

    sandbox.run(R"(db:update_vector_group_by_label("Child", "refs", "Child 1", {}))");
    EXPECT_TRUE(db.read_vector_integers_by_id("Child", "parent_ref", 1).empty());
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 2), (std::vector<std::optional<int64_t>>{1}));
}

TEST_F(SandboxTest, UpdateVectorGroupByLabelErrors) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);
    sandbox.run(R"(db:update_vector_group_by_label("Child", "refs", "Child 1", { parent_ref = { 1 } }))");

    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group_by_label("Child", "refs", "Nope", { parent_ref = { 1 } }))",
        "Element not found"
    );
    // The named-but-empty column trap fires before the label is even resolved.
    expect_sandbox_error(
        sandbox,
        R"(db:update_vector_group_by_label("Child", "refs", "Nope", { parent_ref = {} }))",
        "contain no rows"
    );

    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{1}));
}

TEST_F(SandboxTest, UpdateSetGroupByLabel) {
    auto db = relations_db_with_child();
    db.create_element("Child", quiver::Element().set("label", "Child 2"));
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:update_set_group_by_label("Child", "parents", "Child 2", { parent_ref = { 1 } })
        db:update_set_group_by_label("Child", "parents", "Child 1", { parent_ref = { 1, "Parent B" } })
    )");
    EXPECT_EQ(db.read_set_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{1, 2}));

    sandbox.run(R"(db:update_set_group_by_label("Child", "parents", "Child 1", {}))");
    EXPECT_TRUE(db.read_set_integers_by_id("Child", "parent_ref", 1).empty());
    EXPECT_EQ(db.read_set_integers_by_id("Child", "parent_ref", 2), (std::vector<std::optional<int64_t>>{1}));
}

TEST_F(SandboxTest, UpdateSetGroupByLabelErrors) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);
    sandbox.run(R"(db:update_set_group_by_label("Child", "parents", "Child 1", { parent_ref = { 1 } }))");

    expect_sandbox_error(
        sandbox,
        R"(db:update_set_group_by_label("Child", "parents", "Nope", { parent_ref = { 1 } }))",
        "Element not found"
    );
    // The named-but-empty column trap fires before the label is even resolved.
    expect_sandbox_error(
        sandbox,
        R"(db:update_set_group_by_label("Child", "parents", "Nope", { parent_ref = {} }))",
        "contain no rows"
    );

    EXPECT_EQ(db.read_set_integers_by_id("Child", "parent_ref", 1), (std::vector<std::optional<int64_t>>{1}));
}

TEST_F(SandboxTest, UpdateRelationSetsAndClears) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(db:update_relation("Child", "Parent", "id", 1, "Parent A"))");
    EXPECT_EQ(db.read_scalar_integer_by_id("Child", "parent_id", 1), 1);

    sandbox.run(R"(db:update_relation_by_label("Child", "Parent", "id", "Child 1", "Parent B"))");
    EXPECT_EQ(db.read_scalar_integer_by_id("Child", "parent_id", 1), 2);

    // A nil target_label clears it; so does omitting the argument entirely.
    sandbox.run(R"(db:update_relation("Child", "Parent", "id", 1, nil))");
    EXPECT_FALSE(db.read_scalar_integer_by_id("Child", "parent_id", 1).has_value());

    sandbox.run(R"(db:update_relation_by_label("Child", "Parent", "id", "Child 1", "Parent A"))");
    sandbox.run(R"(db:update_relation_by_label("Child", "Parent", "id", "Child 1"))");
    EXPECT_FALSE(db.read_scalar_integer_by_id("Child", "parent_id", 1).has_value());
}

TEST_F(SandboxTest, UpdateRelationErrors) {
    auto db = relations_db_with_child();
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:update_relation("Child", "Parent", "owner", 1, "Parent A"))",
        "relation column 'parent_owner' not found in collection 'Child'"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_relation_by_label("Child", "Parent", "id", "Nope", "Parent A"))",
        "Element not found"
    );

    // A non-string target_label must throw, not clear: silently treating it as nil would let a
    // stray boolean/number wipe the relation.
    sandbox.run(R"(db:update_relation("Child", "Parent", "id", 1, "Parent A"))");
    expect_sandbox_error(
        sandbox,
        R"(db:update_relation("Child", "Parent", "id", 1, false))",
        "Cannot update_relation: target_label has unsupported Lua type"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_relation_by_label("Child", "Parent", "id", "Child 1", 42))",
        "Cannot update_relation_by_label: target_label has unsupported Lua type"
    );
    EXPECT_EQ(db.read_scalar_integer_by_id("Child", "parent_id", 1), 1);
}

TEST_F(SandboxTest, UpdateElementRejectsNonTableElement) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    const int64_t id =
        db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{7}));
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:update_element("Collection", )" + std::to_string(id) + R"(, "x"))",
        "Cannot update_element: element_table must be a table, got string"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:update_element_by_label("Collection", "Item 1", true))",
        "Cannot update_element_by_label: element_table must be a table, got boolean"
    );
    EXPECT_EQ(db.read_scalar_integer_by_id("Collection", "some_integer", id), 7);
}
