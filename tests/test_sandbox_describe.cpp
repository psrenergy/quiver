#include "test_sandbox.h"

namespace {

quiver::Database open_collections() {
    return quiver::Database::from_schema(
        ":memory:",
        VALID_SCHEMA("collections.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
}

}  // namespace

TEST_F(SandboxTest, DescribeReport) {
    auto db = open_collections();
    db.create_element("Collection", quiver::Element().set("label", "a"));
    db.create_element("Collection", quiver::Element().set("label", "b"));

    quiver::Sandbox sandbox(db);
    sandbox.run(R"LUA(
        local report = db:describe()
        assert(type(report) == "string", "describe should return a string")
        assert(report:find("Collection: Configuration"), "missing Configuration")
        assert(report:find("Collection: Collection %(2 elements%)"), "missing Collection w/ count")
    )LUA");
}

TEST_F(SandboxTest, DescribeCollection) {
    auto db = open_collections();
    quiver::Sandbox sandbox(db);
    sandbox.run(R"LUA(
        local report = db:describe_collection("Collection")
        assert(type(report) == "string", "describe_collection should return a string")
        assert(report:find("Collection: Collection"), "missing header")
        assert(report:find("%[date_time%]"), "missing time-series dimension column")
    )LUA");
}

TEST_F(SandboxTest, DescribeCollectionStatistics) {
    auto db = open_collections();
    db.create_element(
        "Collection",
        quiver::Element()
            .set("label", "a")
            .set("some_integer", static_cast<int64_t>(1))
            .set("value_int", std::vector<int64_t>{10, 20})
    );
    db.create_element("Collection", quiver::Element().set("label", "b").set("some_integer", static_cast<int64_t>(1)));
    db.create_element("Collection", quiver::Element().set("label", "c").set("some_integer", static_cast<int64_t>(5)));

    quiver::Sandbox sandbox(db);
    sandbox.run(R"LUA(
        local report = db:describe_collection("Collection")
        assert(report:find("some_integer (INTEGER); 3 non-null, 0 null; values {1: 2, 5: 1}", 1, true),
               "bad some_integer stats")
        assert(report:find("values: value_int(INTEGER), value_float(REAL); 1/3 non-empty", 1, true),
               "bad vector group stats")
        assert(db:describe() == "Database: :memory:\nVersion: 0\n\n" .. report .. "\n" ..
               db:describe_collection("Configuration"), "reports differ")
    )LUA");
}
