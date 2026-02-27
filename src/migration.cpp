#include "quiver/migration.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace quiver {

namespace fs = std::filesystem;

Migration::Migration(int64_t version, std::string path) : version_(version), path_(std::move(path)) {}

int64_t Migration::version() const {
    return version_;
}

const std::string& Migration::path() const {
    return path_;
}

std::string Migration::up_sql() const {
    auto sql_path = fs::path(path_) / "up.sql";
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
    const auto sql_path = fs::path(path_) / "down.sql";
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

std::strong_ordering Migration::operator<=>(const Migration& other) const {
    return version_ <=> other.version_;
}

bool Migration::operator==(const Migration& other) const {
    return version_ == other.version_;
}

}  // namespace quiver
