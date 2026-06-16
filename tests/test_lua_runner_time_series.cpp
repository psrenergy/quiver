#include "test_lua_runner.h"

TEST_F(LuaRunnerTest, GetTimeSeriesMetadata) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local meta = db:get_time_series_metadata("Collection", "data")
        assert(meta.group_name == "data", "Expected group_name 'data', got " .. tostring(meta.group_name))
        assert(meta.dimension_column == "date_time", "Expected dimension_column 'date_time', got " .. tostring(meta.dimension_column))
        assert(#meta.value_columns >= 1, "Expected at least 1 value column")
        assert(meta.value_columns[1].name == "value", "Expected value column name 'value'")
    )");
}

TEST_F(LuaRunnerTest, ListTimeSeriesGroups) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local groups = db:list_time_series_groups("Collection")
        assert(#groups >= 1, "Expected at least 1 time series group")
        local found = false
        for _, g in ipairs(groups) do
            if g.group_name == "data" then
                found = true
                assert(g.dimension_column == "date_time", "Expected dimension_column 'date_time'")
            end
        end
        assert(found, "Expected to find time series group 'data'")
    )");
}

TEST_F(LuaRunnerTest, ReadTimeSeriesGroupById) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    // Insert time series data via C++
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"value", 1.5}},
        {{"date_time", std::string("2024-01-02")}, {"value", 2.5}},
    };
    db.update_time_series_group("Collection", "data", id, rows);

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local data = db:read_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"()
        assert(#data.date_time == 2, "Expected 2 rows, got " .. #data.date_time)
        assert(data.date_time[1] == "2024-01-01", "Expected date_time '2024-01-01'")
        assert(data.value[1] == 1.5, "Expected value 1.5")
        assert(data.date_time[2] == "2024-01-02", "Expected date_time '2024-01-02'")
        assert(data.value[2] == 2.5, "Expected value 2.5")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, CreateElementWithMultiTimeSeries) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("multi_time_series.sql"));
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Sensor", {
            label = "Sensor 1",
            date_time = {"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"},
            temperature = {20.0, 21.5, 22.0},
            humidity = {45.0, 50.0, 55.0}
        })
    )");

    // Verify temperature group from C++ side
    auto temp_rows = db.read_time_series_group("Sensor", "temperature", 1);
    EXPECT_EQ(temp_rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(temp_rows[0].at("date_time")), "2024-01-01T10:00:00");
    EXPECT_EQ(std::get<std::string>(temp_rows[1].at("date_time")), "2024-01-02T10:00:00");
    EXPECT_EQ(std::get<std::string>(temp_rows[2].at("date_time")), "2024-01-03T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(temp_rows[0].at("temperature")), 20.0);
    EXPECT_DOUBLE_EQ(std::get<double>(temp_rows[1].at("temperature")), 21.5);
    EXPECT_DOUBLE_EQ(std::get<double>(temp_rows[2].at("temperature")), 22.0);

    // Verify humidity group from C++ side
    auto hum_rows = db.read_time_series_group("Sensor", "humidity", 1);
    EXPECT_EQ(hum_rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(hum_rows[0].at("date_time")), "2024-01-01T10:00:00");
    EXPECT_EQ(std::get<std::string>(hum_rows[1].at("date_time")), "2024-01-02T10:00:00");
    EXPECT_EQ(std::get<std::string>(hum_rows[2].at("date_time")), "2024-01-03T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(hum_rows[0].at("humidity")), 45.0);
    EXPECT_DOUBLE_EQ(std::get<double>(hum_rows[1].at("humidity")), 50.0);
    EXPECT_DOUBLE_EQ(std::get<double>(hum_rows[2].at("humidity")), 55.0);
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroup) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-06-01", "2024-06-02", "2024-06-03" },
            value = { 10.0, 20.0, 30.0 },
        })
    )";
    lua.run(script);

    auto rows = db.read_time_series_group("Collection", "data", id);
    EXPECT_EQ(rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2024-06-01");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 10.0);
    EXPECT_EQ(std::get<std::string>(rows[2].at("date_time")), "2024-06-03");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[2].at("value")), 30.0);
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupScalarColumnThrows) {
    // Negative path: passing scalars (the upsert_time_series_row shape) where the
    // column-oriented update expects arrays must throw instead of silently
    // transposing to zero rows and committing a no-op clear.
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, { date_time = "2024-06-01", value = 10.0 })
    )";
    expect_lua_error(lua, script, "must be an array of values");
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupAllEmptyColumnsThrows) {
    // Negative path: named columns that transpose to zero rows are a caller
    // mistake (only an empty table {} clears the group), so this must throw
    // rather than silently delete the element's existing rows.
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, { date_time = {}, value = {} })
    )";
    expect_lua_error(lua, script, "contain no rows");
}

// --- NULL round-trip: dimension column is the row-count authority; value columns
// may be shorter, sparse, or empty, with missing cells written as SQL NULL ---

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupNilHoleWritesNull) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01", "2024-01-02", "2024-01-03" },
            value = { 10.0, nil, 30.0 },
        })
    )";
    lua.run(script);

    auto rows = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(rows.size(), 3);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 10.0);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[1].at("value")));
    EXPECT_DOUBLE_EQ(std::get<double>(rows[2].at("value")), 30.0);
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupShortColumnPadsNull) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01", "2024-01-02", "2024-01-03" },
            value = { 10.0 },
        })
    )";
    lua.run(script);

    auto rows = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(rows.size(), 3);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 10.0);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[1].at("value")));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[2].at("value")));
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupValueColumnLongerThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01", "2024-01-02", "2024-01-03" },
            value = { 10.0, 20.0, 30.0, 40.0 },
        })
    )";
    expect_lua_error(lua, script, "has length 4 but expected 3");
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupMissingDimensionThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, { value = { 10.0 } })
    )";
    expect_lua_error(lua, script, "missing dimension column 'date_time'");
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupDimensionNilThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01", nil, "2024-01-03" },
            value = { 10.0, 20.0, 30.0 },
        })
    )";
    expect_lua_error(lua, script, "dimension column 'date_time' has nil at index 2");
}

TEST_F(LuaRunnerTest, ReadTimeSeriesGroupNullIsNilHole) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    // Seed 3 rows with a NULL value in the middle row.
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"value", 1.5}},
        {{"date_time", std::string("2024-01-02")}, {"value", nullptr}},
        {{"date_time", std::string("2024-01-03")}, {"value", 3.5}},
    };
    db.update_time_series_group("Collection", "data", id, rows);

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local data = db:read_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"()
        -- The dimension column is the row-count authority; value columns may have nil holes.
        assert(#data.date_time == 3, "Expected 3 rows, got " .. #data.date_time)
        assert(data.value[1] == 1.5, "Expected value[1] == 1.5")
        assert(data.value[2] == nil, "Expected value[2] to be nil, SQL NULL")
        assert(data.value[3] == 3.5, "Expected value[3] == 3.5")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, TimeSeriesGroupNilRoundTrip) {
    // The reported scenario: read a group with a NULL cell, modify another cell,
    // and write the whole group back. The nil hole must survive the round-trip.
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"value", 1.5}},
        {{"date_time", std::string("2024-01-02")}, {"value", nullptr}},
        {{"date_time", std::string("2024-01-03")}, {"value", 3.5}},
    };
    db.update_time_series_group("Collection", "data", id, rows);

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local ts = db:read_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"()
        ts.value[1] = 99.0
        db:update_time_series_group("Collection", "data", )" +
                         std::to_string(id) + R"(, ts)
    )";
    lua.run(script);

    auto result = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(result.size(), 3);
    EXPECT_DOUBLE_EQ(std::get<double>(result[0].at("value")), 99.0);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[1].at("value")));
    EXPECT_DOUBLE_EQ(std::get<double>(result[2].at("value")), 3.5);
}

TEST_F(LuaRunnerTest, TimeSeriesGroupAllNullColumnRoundTrip) {
    // An all-NULL value column reads back as an empty table (with the key present)
    // and round-trips as all-NULL under the dimension-authority transpose.
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("nullable_time_series.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"status", nullptr}},
        {{"date_time", std::string("2024-01-02")}, {"status", nullptr}},
    };
    db.update_time_series_group("Sensor", "readings", id, rows);

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local ts = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(#ts.date_time == 2, "Expected 2 rows, got " .. #ts.date_time)
        assert(type(ts.status) == "table", "Expected status column key present as a table")
        assert(next(ts.status) == nil, "Expected status column to be an empty table, all NULL")
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, ts)
    )";
    lua.run(script);

    auto result = db.read_time_series_group("Sensor", "readings", id);
    ASSERT_EQ(result.size(), 2);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[0].at("status")));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[1].at("status")));
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupMultiDimNulls) {
    // Multi-dimension group (date_time + block are both PK): both dimension
    // columns must be dense and equal-length; value columns may be short/empty.
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Resource", quiver::Element().set("label", "Resource 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Resource", "load", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-06-01", "2024-06-02", "2024-06-03" },
            block = { 1, 1, 1 },
            load = { 1.5 },
            flag = {},
        })
    )";
    lua.run(script);

    auto rows = db.read_time_series_group("Resource", "load", id);
    ASSERT_EQ(rows.size(), 3);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("load")), 1.5);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[1].at("load")));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[2].at("load")));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[0].at("flag")));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[1].at("flag")));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[2].at("flag")));
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupMultiDimBlockLengthMismatchThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Resource", quiver::Element().set("label", "Resource 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Resource", "load", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-06-01", "2024-06-02", "2024-06-03" },
            block = { 1, 2 },
            load = { 1.5, 2.5, 3.5 },
        })
    )";
    expect_lua_error(lua, script, "column 'block' has length 2 but expected 3");
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesGroupMultiDimMissingBlockThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Resource", quiver::Element().set("label", "Resource 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Resource", "load", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-06-01" },
            load = { 1.5 },
        })
    )";
    expect_lua_error(lua, script, "missing dimension column 'block'");
}

TEST_F(LuaRunnerTest, UpsertTimeSeriesRowInsert) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:upsert_time_series_row("Collection", "data", )" +
                         std::to_string(id) + R"(, { date_time = "2024-06-01", value = 10.0 })
    )";
    lua.run(script);

    auto rows = db.read_time_series_group("Collection", "data", id);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2024-06-01");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 10.0);
}

TEST_F(LuaRunnerTest, UpsertTimeSeriesRowSamePK) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script_first = R"(
        db:upsert_time_series_row("Collection", "data", )" +
                               std::to_string(id) + R"(, { date_time = "2024-06-01", value = 10.0 })
    )";
    lua.run(script_first);

    std::string script_second = R"(
        db:upsert_time_series_row("Collection", "data", )" +
                                std::to_string(id) + R"(, { date_time = "2024-06-01", value = 99.0 })
    )";
    lua.run(script_second);

    auto rows = db.read_time_series_group("Collection", "data", id);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2024-06-01");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 99.0);
}

TEST_F(LuaRunnerTest, UpsertTimeSeriesRowMultiDim) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("multi_dim_time_series.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Resource", quiver::Element().set("label", "Resource 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:upsert_time_series_row("Resource", "load", )" +
                         std::to_string(id) + R"(, { date_time = "2024-06-01", block = 1, load = 42.5, flag = 1 })
    )";
    lua.run(script);

    auto rows = db.read_time_series_group("Resource", "load", id);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2024-06-01");
    EXPECT_EQ(std::get<int64_t>(rows[0].at("block")), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("load")), 42.5);
    EXPECT_EQ(std::get<int64_t>(rows[0].at("flag")), 1);
}

TEST_F(LuaRunnerTest, UpsertTimeSeriesRowMissingDimErrors) {
    // Negative path: omitting the required date_time dimension column must
    // surface the C++ "Cannot upsert_time_series_row: row missing required ..."
    // error through sol2 -> LuaRunner::run -> std::runtime_error. Mirrors the
    // Julia / Dart / Python suites which all cover this case.
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:upsert_time_series_row("Collection", "data", )" +
                         std::to_string(id) + R"(, { value = 10.0 })
    )";
    EXPECT_THROW({ lua.run(script); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, HasTimeSeriesFiles) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local has = db:has_time_series_files("Collection")
        assert(has == true, "Expected has_time_series_files to return true for collections.sql")
    )");
}

TEST_F(LuaRunnerTest, ListTimeSeriesFilesColumns) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local cols = db:list_time_series_files_columns("Collection")
        assert(#cols == 2, "Expected 2 columns, got " .. #cols)

        local found_data = false
        local found_metadata = false
        for _, c in ipairs(cols) do
            if c == "data_file" then found_data = true end
            if c == "metadata_file" then found_metadata = true end
        end
        assert(found_data, "Expected 'data_file' column")
        assert(found_metadata, "Expected 'metadata_file' column")
    )");
}

TEST_F(LuaRunnerTest, ReadTimeSeriesFiles) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local files = db:read_time_series_files("Collection")
        -- Initially files may be nil/empty - just verify the table is returned
        assert(type(files) == "table", "Expected table, got " .. type(files))
    )");
}

TEST_F(LuaRunnerTest, UpdateTimeSeriesFiles) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_time_series_files("Collection", {
            data_file = "/path/to/data.csv",
            metadata_file = "/path/to/meta.json"
        })

        local files = db:read_time_series_files("Collection")
        assert(files.data_file == "/path/to/data.csv", "Expected data_file '/path/to/data.csv', got " .. tostring(files.data_file))
        assert(files.metadata_file == "/path/to/meta.json", "Expected metadata_file '/path/to/meta.json', got " .. tostring(files.metadata_file))
    )");

    // Verify from C++ side
    auto files = db.read_time_series_files("Collection");
    EXPECT_EQ(files["data_file"].value(), "/path/to/data.csv");
    EXPECT_EQ(files["metadata_file"].value(), "/path/to/meta.json");
}

TEST_F(LuaRunnerTest, MultiColumnTimeSeriesUpdateAndRead) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01T10:00:00", "2024-01-02T10:00:00" },
            temperature = { 20.5, 22.3 },
            humidity = { 45, 50 },
            status = { "normal", "warning" },
        })

        local data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(#data.date_time == 2, "Expected 2 rows, got " .. #data.date_time)

        -- Verify columns
        assert(data.date_time[1] == "2024-01-01T10:00:00", "Row 1 date_time mismatch: " .. tostring(data.date_time[1]))
        assert(data.temperature[1] == 20.5, "Row 1 temperature mismatch: " .. tostring(data.temperature[1]))
        assert(data.humidity[1] == 45, "Row 1 humidity mismatch: " .. tostring(data.humidity[1]))
        assert(data.status[1] == "normal", "Row 1 status mismatch: " .. tostring(data.status[1]))

        assert(data.date_time[2] == "2024-01-02T10:00:00", "Row 2 date_time mismatch: " .. tostring(data.date_time[2]))
        assert(data.temperature[2] == 22.3, "Row 2 temperature mismatch: " .. tostring(data.temperature[2]))
        assert(data.humidity[2] == 50, "Row 2 humidity mismatch: " .. tostring(data.humidity[2]))
        assert(data.status[2] == "warning", "Row 2 status mismatch: " .. tostring(data.status[2]))

        -- Verify types
        assert(type(data.date_time[1]) == "string", "date_time should be string")
        assert(type(data.temperature[1]) == "number", "temperature should be number")
        assert(type(data.humidity[1]) == "number", "humidity should be number")
        assert(type(data.status[1]) == "string", "status should be string")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, MultiColumnTimeSeriesReadEmpty) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(next(data) == nil, "Expected empty table for empty time series")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, MultiColumnTimeSeriesReplace) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        -- First update: 2 rows
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01", "2024-01-02" },
            temperature = { 10.0, 15.0 },
            humidity = { 30, 40 },
            status = { "low", "mid" },
        })

        local data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(#data.date_time == 2, "Expected 2 rows after first update, got " .. #data.date_time)

        -- Second update: 3 different rows (replaces previous)
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-06-01", "2024-06-02", "2024-06-03" },
            temperature = { 25.0, 26.5, 28.0 },
            humidity = { 60, 65, 70 },
            status = { "high", "high", "critical" },
        })

        data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(#data.date_time == 3, "Expected 3 rows after replace, got " .. #data.date_time)
        assert(data.date_time[1] == "2024-06-01", "First row should be 2024-06-01")
        assert(data.temperature[1] == 25.0, "First row temperature should be 25.0")
        assert(data.status[3] == "critical", "Third row status should be 'critical'")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, MultiColumnTimeSeriesClear) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        -- Insert 2 rows
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01", "2024-01-02" },
            temperature = { 10.0, 15.0 },
            humidity = { 30, 40 },
            status = { "ok", "ok" },
        })

        local data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(#data.date_time == 2, "Expected 2 rows before clear, got " .. #data.date_time)

        -- Clear by updating with empty table
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, {})

        data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(next(data) == nil, "Expected empty table after clear")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, MultiColumnTimeSeriesOrdering) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        -- Insert rows out of order
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-03", "2024-01-01", "2024-01-02" },
            temperature = { 30.0, 10.0, 20.0 },
            humidity = { 70, 30, 50 },
            status = { "high", "low", "mid" },
        })

        local data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(#data.date_time == 3, "Expected 3 rows, got " .. #data.date_time)

        -- Verify rows are returned sorted by date_time (dimension column)
        assert(data.date_time[1] == "2024-01-01", "First row should be 2024-01-01, got " .. data.date_time[1])
        assert(data.date_time[2] == "2024-01-02", "Second row should be 2024-01-02, got " .. data.date_time[2])
        assert(data.date_time[3] == "2024-01-03", "Third row should be 2024-01-03, got " .. data.date_time[3])

        -- Verify corresponding values match the correct rows
        assert(data.temperature[1] == 10.0, "Row 1 temperature should be 10.0")
        assert(data.temperature[2] == 20.0, "Row 2 temperature should be 20.0")
        assert(data.temperature[3] == 30.0, "Row 3 temperature should be 30.0")
        assert(data.status[1] == "low", "Row 1 status should be 'low'")
        assert(data.status[2] == "mid", "Row 2 status should be 'mid'")
        assert(data.status[3] == "high", "Row 3 status should be 'high'")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, MultiColumnTimeSeriesMultiRow) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Sensor", quiver::Element().set("label", "Sensor 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"(, {
            date_time = { "2024-01-01T00:00:00", "2024-01-02T00:00:00", "2024-01-03T00:00:00",
                          "2024-01-04T00:00:00", "2024-01-05T00:00:00" },
            temperature = { 10.1, 15.2, 20.3, 25.4, 30.5 },
            humidity = { 30, 40, 50, 60, 70 },
            status = { "cold", "cool", "mild", "warm", "hot" },
        })

        local data = db:read_time_series_group("Sensor", "readings", )" +
                         std::to_string(id) + R"()
        assert(#data.date_time == 5, "Expected 5 rows, got " .. #data.date_time)

        -- Verify all 5 rows with correct types and values
        local expected_temps = {10.1, 15.2, 20.3, 25.4, 30.5}
        local expected_humidity = {30, 40, 50, 60, 70}
        local expected_status = {"cold", "cool", "mild", "warm", "hot"}

        for i = 1, 5 do
            local expected_date = "2024-01-0" .. i .. "T00:00:00"
            assert(data.date_time[i] == expected_date, "Row " .. i .. " date_time mismatch: " .. tostring(data.date_time[i]))
            assert(data.temperature[i] == expected_temps[i], "Row " .. i .. " temperature mismatch: " .. tostring(data.temperature[i]))
            assert(data.humidity[i] == expected_humidity[i], "Row " .. i .. " humidity mismatch: " .. tostring(data.humidity[i]))
            assert(data.status[i] == expected_status[i], "Row " .. i .. " status mismatch: " .. tostring(data.status[i]))
        end

        -- Verify types on last row
        assert(type(data.date_time[5]) == "string", "date_time should be string type")
        assert(type(data.temperature[5]) == "number", "temperature should be number type")
        assert(type(data.humidity[5]) == "number", "humidity should be number type")
        assert(type(data.status[5]) == "string", "status should be string type")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadTimeSeriesRow) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1" })
        db:create_element("Collection", { label = "Item 2" })
        db:update_time_series_group("Collection", "data", 1, {
            date_time = { "2024-01-01T00:00:00", "2024-02-01T00:00:00" },
            value = { 10.5, 20.5 },
        })
        db:update_time_series_group("Collection", "data", 2, {
            date_time = { "2024-01-01T00:00:00" },
            value = { 30.5 },
        })

        local row = db:read_time_series_row("Collection", "data", "value", "2024-01-15T00:00:00")
        assert(#row == 2, "expected 2 values, got " .. #row)
        assert(row[1] == 10.5, "expected 10.5, got " .. tostring(row[1]))
        assert(row[2] == 30.5, "expected 30.5, got " .. tostring(row[2]))
    )");
}
