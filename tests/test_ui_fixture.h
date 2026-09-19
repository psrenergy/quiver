#ifndef QUIVER_TEST_UI_FIXTURE_H
#define QUIVER_TEST_UI_FIXTURE_H

#include "test_utils.h"

#include <quiver/database.h>
#include <string>

namespace quiver::test {

// Opens a file-backed Database whose file physically lives inside a fixture directory under
// tests/schemas/ (`dir_under_schemas`, e.g. "ui_golden" or "ui/enum_basic"). Database::from_schema
// is self-cleaning (removes a stale db_path before creating -- src/database.cpp), so no manual
// teardown is needed. Leaving the database file inside the fixture directory is what makes
// impl_->path's parent directory the one holding any `ui/` sibling (D-28: the sidecar must
// physically sit at <db_dir>/ui/, and Phase 1 has no config-path override).
//
// `db_stem` is per gtest case (not per suite) so `ctest -j` cannot race two cases writing the
// same file inside one shared fixture directory (D-29, tightened for gtest_discover_tests
// registering every TEST as its own ctest test).
inline Database
open_ui_fixture_at(const char* test_file, const std::string& dir_under_schemas, const std::string& db_stem) {
    const auto fixture_dir = path_from(test_file, "schemas/" + dir_under_schemas);
    const auto schema_path = fixture_dir + "/schema.sql";
    const auto db_path = fixture_dir + "/" + db_stem + ".sqlite";
    return Database::from_schema(db_path, schema_path, {.read_only = false, .console_level = LogLevel::Off});
}

// Opens a fixture directory under tests/schemas/ui/<fixture_dir>/ -- the tree every PARSE-*/
// DESC-*/CORPUS-* fixture lives in (tests/schemas/ui_golden/ deliberately lives outside it, see
// open_ui_fixture_at above).
inline Database open_ui_fixture(const char* test_file, const std::string& fixture_dir, const std::string& db_stem) {
    return open_ui_fixture_at(test_file, "ui/" + fixture_dir, db_stem);
}

}  // namespace quiver::test

#endif  // QUIVER_TEST_UI_FIXTURE_H
