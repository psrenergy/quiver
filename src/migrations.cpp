#include "quiver/migrations.h"

#include <algorithm>
#include <filesystem>

namespace quiver {

namespace fs = std::filesystem;

Migrations::Migrations() = default;

Migrations::Migrations(const std::string& path) {
    if (!fs::exists(path) || !fs::is_directory(path)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(path)) {
        if (!entry.is_directory()) {
            continue;
        }

        const auto& dirname = entry.path().filename().string();
        try {
            size_t pos = 0;
            const auto migration_version = std::stoll(dirname, &pos);
            if (pos == dirname.size() && migration_version > 0) {
                const auto migration_path = entry.path().string();
                versions_.emplace_back(migration_version, migration_path);
            }
        } catch (const std::exception&) {
            // Not a valid version number, skip
        }
    }

    std::sort(versions_.begin(), versions_.end());
}

const std::vector<Migration>& Migrations::all() const {
    return versions_;
}

std::vector<Migration> Migrations::pending(int64_t current_version) const {
    std::vector<Migration> result;
    for (const auto& migration : versions_) {
        if (migration.version() > current_version) {
            result.push_back(migration);
        }
    }
    return result;
}

int64_t Migrations::latest_version() const {
    if (versions_.empty()) {
        return 0;
    }
    return versions_.back().version();
}

size_t Migrations::count() const {
    return versions_.size();
}

bool Migrations::empty() const {
    return versions_.empty();
}

Migrations::iterator Migrations::begin() const {
    return versions_.begin();
}

Migrations::iterator Migrations::end() const {
    return versions_.end();
}

}  // namespace quiver
