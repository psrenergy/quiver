#ifndef PSR_MIGRATION_H
#define PSR_MIGRATION_H

#include "export.h"

#include <memory>
#include <string>

namespace margaux {

class PSR_API Migration {
public:
    Migration(int64_t version, const std::string& path);
    ~Migration();

    // Copy and move
    Migration(const Migration& other);
    Migration& operator=(const Migration& other);
    Migration(Migration&& other) noexcept;
    Migration& operator=(Migration&& other) noexcept;

    // Accessors
    int64_t version() const;
    const std::string& path() const;

    // Read migration SQL content
    std::string up_sql() const;
    std::string down_sql() const;

    // Comparison operators (for sorting by version)
    bool operator<(const Migration& other) const;
    bool operator==(const Migration& other) const;
    bool operator!=(const Migration& other) const;
    bool operator>(const Migration& other) const;
    bool operator<=(const Migration& other) const;
    bool operator>=(const Migration& other) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace margaux

#endif  // PSR_MIGRATION_H
