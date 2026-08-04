#include "database_impl.h"

#include <stdexcept>
#include <string>

namespace quiver {

int64_t Database::number_of_elements(const std::string& collection) const {
    impl_->require_collection(collection, "number_of_elements");

    // execute() is non-const, so this const method prepares/steps directly on impl_->db
    sqlite3_stmt* raw_stmt = nullptr;
    const std::string sql = "SELECT COUNT(*) FROM \"" + collection + "\"";
    auto rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &raw_stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }
    StmtPtr stmt(raw_stmt, sqlite3_finalize);

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_ROW) {
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }
    return sqlite3_column_int64(stmt.get(), 0);
}

}  // namespace quiver
