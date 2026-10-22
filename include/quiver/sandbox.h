#ifndef QUIVER_SANDBOX_H
#define QUIVER_SANDBOX_H

#include "export.h"

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
    std::string run(const std::string& script);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver

#endif  // QUIVER_SANDBOX_H
