#ifndef QUIVER_SANDBOX_H
#define QUIVER_SANDBOX_H

#include "export.h"

#include <cstdint>
#include <memory>
#include <string>

namespace quiver {

class Database;

class QUIVER_API Sandbox {
public:
    explicit Sandbox(Database& db);
    ~Sandbox();

    // Non-copyable
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    // Movable
    Sandbox(Sandbox&&) noexcept;
    Sandbox& operator=(Sandbox&&) noexcept;

    /// Runs a Lua script with access to the database as 'db'.
    ///
    /// Returns the script's return value encoded as JSON, or an empty string if the script
    /// returned nothing. Only the first returned value is encoded.
    ///
    /// To execute a script without keeping its writes, wrap the call in
    /// Database::begin_dry_run / Database::end_dry_run. A transaction or dry run open when run()
    /// starts belongs to the caller: the script's db:commit / db:rollback / db:end_dry_run refuse
    /// to end it. On a database opened read-only, the script cannot write files.
    ///
    /// timeout_ms > 0 limits how long the script runs, in wall-clock milliseconds; 0 (the default)
    /// means no limit, and a negative value throws. The limit is soft: it is checked between Lua
    /// instructions and between SQLite VM steps, so a single long C++ call (export_csv on a huge
    /// collection, an expression save, a typed write) finishes first. A script past its limit fails
    /// with "Cannot run: script exceeded its time limit of <N> ms", which pcall cannot catch. Memory
    /// is not limited.
    std::string run(const std::string& script, int64_t timeout_ms = 0);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver

#endif  // QUIVER_SANDBOX_H
