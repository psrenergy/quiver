#include "quiver/migration.h"

#include "utils/file.h"

#include <filesystem>

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
    return utils::read_file(fs::path(path_) / "up.sql").value_or("");
}

std::string Migration::down_sql() const {
    return utils::read_file(fs::path(path_) / "down.sql").value_or("");
}

std::strong_ordering Migration::operator<=>(const Migration& other) const {
    return version_ <=> other.version_;
}

bool Migration::operator==(const Migration& other) const {
    return version_ == other.version_;
}

}  // namespace quiver
