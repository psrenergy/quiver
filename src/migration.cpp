#include "margaux/migration.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace margaux {

namespace fs = std::filesystem;

struct Migration::Impl {
    int64_t version;
    std::string path;

    Impl(int64_t v, const std::string& p) : version(v), path(p) {}
    Impl(const Impl& other) : version(other.version), path(other.path) {}
};

Migration::Migration(int64_t version, const std::string& path) : impl_(std::make_unique<Impl>(version, path)) {}

Migration::~Migration() = default;

Migration::Migration(const Migration& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

Migration& Migration::operator=(const Migration& other) {
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

Migration::Migration(Migration&& other) noexcept = default;
Migration& Migration::operator=(Migration&& other) noexcept = default;

int64_t Migration::version() const {
    return impl_->version;
}

const std::string& Migration::path() const {
    return impl_->path;
}

std::string Migration::up_sql() const {
    fs::path sql_path = fs::path(impl_->path) / "up.sql";
    if (!fs::exists(sql_path)) {
        return "";
    }

    std::ifstream file(sql_path);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string Migration::down_sql() const {
    const auto sql_path = fs::path(impl_->path) / "down.sql";
    if (!fs::exists(sql_path)) {
        return "";
    }

    std::ifstream file(sql_path);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool Migration::operator<(const Migration& other) const {
    return impl_->version < other.impl_->version;
}

bool Migration::operator==(const Migration& other) const {
    return impl_->version == other.impl_->version;
}

bool Migration::operator!=(const Migration& other) const {
    return impl_->version != other.impl_->version;
}

bool Migration::operator>(const Migration& other) const {
    return impl_->version > other.impl_->version;
}

bool Migration::operator<=(const Migration& other) const {
    return impl_->version <= other.impl_->version;
}

bool Migration::operator>=(const Migration& other) const {
    return impl_->version >= other.impl_->version;
}

}  // namespace margaux
