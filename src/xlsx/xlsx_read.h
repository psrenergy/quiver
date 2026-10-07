#ifndef QUIVER_SRC_XLSX_XLSX_READ_H
#define QUIVER_SRC_XLSX_XLSX_READ_H

// Internal, Lua-only reader. Pimpl keeps OpenXLSX and its XML headers out of the
// sol2 binding TU and the public API, like csv_read::Reader.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace quiver::xlsx_read {

struct Options {
    std::variant<int64_t, std::string> sheet = int64_t{1};
    int64_t header_row = 1;
};

using RowSink = std::function<bool(std::vector<std::string>&& cells, int64_t index)>;

class Reader {
public:
    Reader(std::string resolved_path, std::string original_path, std::string operation, Options options = {});
    ~Reader();

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader& operator=(Reader&&) = delete;

    const std::vector<std::string>& header() const;
    int64_t for_each_row(const RowSink& sink);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver::xlsx_read

#endif  // QUIVER_SRC_XLSX_XLSX_READ_H
