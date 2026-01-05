#ifndef PSR_DATABASE_H
#define PSR_DATABASE_H

#include "export.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

struct sqlite3;

namespace psr {

using Value = std::variant<std::nullptr_t, int64_t, double, std::string, std::vector<uint8_t>, std::vector<int64_t>,
                           std::vector<double>, std::vector<std::string>>;

enum class LogLevel { debug, info, warn, error, off };

struct PSR_API DatabaseOptions {
    bool read_only = false;
    LogLevel console_level = LogLevel::info;
};

class PSR_API Database {
public:
    explicit Database(const std::string& path, const DatabaseOptions& options = DatabaseOptions());
    ~Database();

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Movable
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    bool is_open() const;
    void close();

    void execute(const std::string& sql, const std::vector<Value>& params = {});

    const std::string& path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace psr

#endif  // PSR_DATABASE_H
