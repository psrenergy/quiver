#include "test_sandbox.h"

// Scripts hand a value back to the host as JSON. Only the first returned value is encoded.

namespace {

// Every case here is schema-independent, so one in-memory database serves them all.
quiver::Database return_database() {
    return quiver::Database::from_schema(
        ":memory:",
        VALID_SCHEMA("collections.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
}

}  // namespace

TEST_F(SandboxTest, ReturnNothingYieldsEmptyString) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(sandbox.run("local x = 1"), "");
}

TEST_F(SandboxTest, ReturnNilYieldsNull) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // Distinct from returning nothing at all.
    EXPECT_EQ(sandbox.run("return nil"), "null");
}

TEST_F(SandboxTest, ReturnScalars) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(sandbox.run("return 42"), "42");
    EXPECT_EQ(sandbox.run("return -7"), "-7");
    EXPECT_EQ(sandbox.run("return 1.5"), "1.5");
    // Shortest round-trippable form, not "0.10000000000000001".
    EXPECT_EQ(sandbox.run("return 0.1"), "0.1");
    EXPECT_EQ(sandbox.run("return true"), "true");
    EXPECT_EQ(sandbox.run("return false"), "false");
    EXPECT_EQ(sandbox.run("return 'hello'"), "\"hello\"");
    // int64 beyond double precision must survive as an integer.
    EXPECT_EQ(sandbox.run("return 9007199254740993"), "9007199254740993");
}

TEST_F(SandboxTest, ReturnOnlyFirstValue) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(sandbox.run("return 1, 2, 3"), "1");
}

TEST_F(SandboxTest, ReturnNonFiniteNumbersBecomeNull) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // JSON has no NaN/Infinity literals.
    EXPECT_EQ(sandbox.run("return 0/0"), "null");
    EXPECT_EQ(sandbox.run("return math.huge"), "null");
    EXPECT_EQ(sandbox.run("return -math.huge"), "null");
}

TEST_F(SandboxTest, ReturnStringsAreEscaped) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(sandbox.run(R"(return 'a"b\\c')"), R"("a\"b\\c")");
    EXPECT_EQ(sandbox.run("return 'line\\nbreak\\ttab'"), "\"line\\nbreak\\ttab\"");
    // Control characters below 0x20 without a short escape use \u00XX.
    EXPECT_EQ(sandbox.run("return string.char(1)"), "\"\\u0001\"");
}

TEST_F(SandboxTest, ReturnArrays) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(sandbox.run("return {1, 2, 3}"), "[1,2,3]");
    EXPECT_EQ(sandbox.run("return {'a', 'b'}"), "[\"a\",\"b\"]");
    EXPECT_EQ(sandbox.run("return {{1, 2}, {3}}"), "[[1,2],[3]]");
    // An empty table is ambiguous; it encodes as an array.
    EXPECT_EQ(sandbox.run("return {}"), "[]");
}

TEST_F(SandboxTest, ReturnObjectsWithSortedKeys) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // Lua's pairs order is unspecified, so keys are sorted to keep the output deterministic.
    EXPECT_EQ(sandbox.run("return {b = 2, a = 1, c = 3}"), R"({"a":1,"b":2,"c":3})");
    EXPECT_EQ(sandbox.run("return {outer = {inner = 'v'}}"), R"({"outer":{"inner":"v"}})");
    // Non-contiguous integer keys are not an array; they stringify as object keys.
    EXPECT_EQ(sandbox.run("return {[1] = 'a', [3] = 'c'}"), R"({"1":"a","3":"c"})");
    // A mixed table is an object too.
    EXPECT_EQ(sandbox.run("return {[1] = 'a', name = 'x'}"), R"({"1":"a","name":"x"})");
}

TEST_F(SandboxTest, ReturnDatabaseReads) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 10 })
        db:create_element("Collection", { label = "Item 2", some_integer = 20 })
    )");

    EXPECT_EQ(sandbox.run(R"(return db:read_scalar_integers("Collection", "some_integer"))"), "[10,20]");
    EXPECT_EQ(sandbox.run(R"(return { ids = db:read_element_ids("Collection") })"), R"({"ids":[1,2]})");
}

TEST_F(SandboxTest, ReturnUnsupportedTypeThrows) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(sandbox, "return function() end", "Cannot run: script returned an unsupported Lua type");
    expect_sandbox_error(sandbox, "return coroutine.create(function() end)", "unsupported Lua type");
    // A userdata (the db global itself) is not encodable either.
    expect_sandbox_error(sandbox, "return db", "unsupported Lua type");
}

TEST_F(SandboxTest, ReturnUnsupportedTableKeyThrows) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        "return {[1.5] = 'x'}",
        "Cannot run: script returned a table with an unsupported key type"
    );
}

TEST_F(SandboxTest, ReturnDuplicateStringifiedKeyThrows) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // Two distinct Lua keys, one JSON key -- refuse rather than silently drop whichever `pairs`
    // happened to yield first.
    expect_sandbox_error(
        sandbox,
        R"(
        local t = {}
        t[1] = 'integer-key'
        t['1'] = 'string-key'
        return t
    )",
        "Cannot run: script returned a table with duplicate key '1'"
    );
}

TEST_F(SandboxTest, ReturnNonUtf8StringThrows) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // JSON must be UTF-8 (RFC 8259) but a Lua string is an arbitrary byte array. Rejected here
    // because downstream Python/Dart raise opaque decode errors and JS corrupts silently.
    expect_sandbox_error(
        sandbox,
        "return string.char(200)",
        "Cannot run: script return value contains a string that is not valid UTF-8"
    );
    // A truncated multi-byte sequence is rejected too.
    expect_sandbox_error(sandbox, "return string.char(0xC3)", "not valid UTF-8");
    // So are an overlong encoding and a UTF-16 surrogate, which a strict decoder also rejects.
    expect_sandbox_error(sandbox, "return string.char(0xC0, 0xAF)", "not valid UTF-8");
    expect_sandbox_error(sandbox, "return string.char(0xED, 0xA0, 0x80)", "not valid UTF-8");
    // Well-formed UTF-8 passes through unescaped, one, three and four bytes wide.
    EXPECT_EQ(sandbox.run("return string.char(0xC3, 0xA9)"), "\"\xC3\xA9\"");
    EXPECT_EQ(sandbox.run("return string.char(0xE2, 0x82, 0xAC)"), "\"\xE2\x82\xAC\"");
    EXPECT_EQ(sandbox.run("return string.char(0xF0, 0x9F, 0x8E, 0xAF)"), "\"\xF0\x9F\x8E\xAF\"");
}

TEST_F(SandboxTest, ReturnTooLargeThrows) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // The depth cap does not bound the output: sharing sub-tables gives 2^20 nodes at only 21
    // levels of nesting, so the size cap is what stops an untrusted script from hanging the host.
    expect_sandbox_error(
        sandbox,
        R"(
        local t = {string.rep('x', 4096)}
        for _ = 1, 20 do t = {t, t} end
        return t
    )",
        "Cannot run: script return value exceeds"
    );
}

TEST_F(SandboxTest, ReturnTooDeeplyNestedThrows) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // The depth cap is what stops a self-referencing table from blowing the stack.
    expect_sandbox_error(
        sandbox,
        R"(
        local t = {}
        t[1] = t
        return t
    )",
        "nests deeper than 32 levels"
    );

    // A merely deep (acyclic) table trips the same cap.
    expect_sandbox_error(
        sandbox,
        R"(
        local root = {}
        local node = root
        for _ = 1, 40 do
            node[1] = {}
            node = node[1]
        end
        return root
    )",
        "nests deeper than 32 levels"
    );
}

TEST_F(SandboxTest, ReturnAtTheDepthLimitSucceeds) {
    auto db = return_database();
    quiver::Sandbox sandbox(db);

    // 31 nested tables plus the scalar leaf sits just inside the cap.
    auto result = sandbox.run(R"(
        local root = {}
        local node = root
        for _ = 1, 30 do
            node[1] = {}
            node = node[1]
        end
        node[1] = 7
        return root
    )");
    EXPECT_EQ(std::count(result.begin(), result.end(), '['), 31);
    EXPECT_NE(result.find('7'), std::string::npos);
}
