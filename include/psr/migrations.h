#ifndef QUIVER_MIGRATIONS_H
#define QUIVER_MIGRATIONS_H

#include "export.h"
#include "quiver/migration.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace quiver {

class QUIVER_API Migrations {
public:
    Migrations();
    explicit Migrations(const std::string& path);
    ~Migrations();

    // Copy and move
    Migrations(const Migrations& other);
    Migrations& operator=(const Migrations& other);
    Migrations(Migrations&& other) noexcept;
    Migrations& operator=(Migrations&& other) noexcept;

    // Access all migrations (sorted by version)
    const std::vector<Migration>& all() const;

    // Get migrations pending from current version
    std::vector<Migration> pending(int64_t current_version) const;

    // Get the latest/highest version available
    int64_t latest_version() const;

    // Number of migrations
    size_t count() const;

    // Check if empty
    bool empty() const;

    // Iterator support
    using iterator = std::vector<Migration>::const_iterator;
    iterator begin() const;
    iterator end() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver

#endif  // QUIVER_MIGRATIONS_H
