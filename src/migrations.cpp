#include "psr/migrations.h"

#include <algorithm>
#include <filesystem>

namespace psr {

namespace fs = std::filesystem;

struct Migrations::Impl {
    std::vector<Migration> versions;

    Impl() = default;
    Impl(const Impl& other) : versions(other.versions) {}
};

Migrations::Migrations() : impl_(std::make_unique<Impl>()) {}

Migrations::Migrations(const std::string& path) : impl_(std::make_unique<Impl>()) {
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
            auto migration_version = std::stoll(dirname, &pos);
            if (pos == dirname.size() && migration_version > 0) {
                auto migration_path = entry.path().string();
                impl_->versions.emplace_back(migration_version, migration_path);
            }
        } catch (const std::exception&) {
            // Not a valid version number, skip
        }
    }

    std::sort(impl_->versions.begin(), impl_->versions.end());
}

Migrations::~Migrations() = default;

Migrations::Migrations(const Migrations& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

Migrations& Migrations::operator=(const Migrations& other) {
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

Migrations::Migrations(Migrations&& other) noexcept = default;
Migrations& Migrations::operator=(Migrations&& other) noexcept = default;

const std::vector<Migration>& Migrations::all() const {
    return impl_->versions;
}

std::vector<Migration> Migrations::pending(int64_t current_version) const {
    std::vector<Migration> result;
    for (const auto& migration : impl_->versions) {
        if (migration.version() > current_version) {
            result.push_back(migration);
        }
    }
    return result;
}

int64_t Migrations::latest_version() const {
    if (impl_->versions.empty()) {
        return 0;
    }
    return impl_->versions.back().version();
}

size_t Migrations::count() const {
    return impl_->versions.size();
}

bool Migrations::empty() const {
    return impl_->versions.empty();
}

Migrations::iterator Migrations::begin() const {
    return impl_->versions.begin();
}

Migrations::iterator Migrations::end() const {
    return impl_->versions.end();
}

}  // namespace psr
