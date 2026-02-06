#ifndef QUIVER_MIGRATION_H
#define QUIVER_MIGRATION_H

#include "export.h"

#include <cstdint>
#include <string>

namespace quiver {

class QUIVER_API Migration {
public:
    Migration(int64_t version, const std::string& path);

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
    int64_t version_;
    std::string path_;
};

}  // namespace quiver

#endif  // QUIVER_MIGRATION_H
