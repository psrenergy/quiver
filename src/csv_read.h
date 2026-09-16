#ifndef QUIVER_SRC_CSV_READ_H
#define QUIVER_SRC_CSV_READ_H

// Internal CSV reader wrapping vincentlaucsb/csv-parser for the Lua-only db:read_csv /
// db:read_csv_stream bindings (src/lua_runner.cpp). No public include/quiver/ counterpart, no
// QUIVER_API, no C API, no FFI binding: Julia/Dart/Python/JS already have native CSV libraries,
// and Lua needs this specifically because `io` is deliberately absent from its sandbox (root
// CLAUDE.md design decisions). This is the first internal .cpp in src/ with no public header --
// every other internal helper (utils/string.h, database_internal.h, binary/binary_utils.h) is
// header-only inline; Reader is Pimpl'd specifically so csv-parser's headers never have to be
// included by src/lua_runner.cpp, which already needs /bigobj on MSVC for sol2's template depth.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace quiver::csv_read {

struct Options {
    char separator = ',';
    // 1-based; 0 means "this file has no header" (D-20). Default 1 matches the previous hardcoded
    // header_row(0) (csv-parser is 0-based) exactly, so an unspecified option changes nothing.
    int64_t header_row = 1;
};

// Invoked once per data row with that row's cells (moved-from -- the caller owns them) and the
// row's 1-based ordinal. Returning false stops the read early; the sink's return value, not an
// exception, is the early-stop signal (an exception propagates through for_each_row verbatim).
using RowSink = std::function<bool(std::vector<std::string>&& cells, int64_t index)>;

// Wraps one csv::CSVReader. Both db:read_csv and db:read_csv_stream construct a Reader and drive
// it through header() / for_each_row(), so the two Lua entry points can never diverge in how they
// parse a file (LUA-03).
class Reader {
public:
    // `resolved_path` is the sandbox-checked absolute path used for the actual file access;
    // `original_path` is the caller's own spelling, quoted verbatim in every error message this
    // class raises, so the messages match what the script wrote rather than an internal
    // canonicalization. `operation` is the public Lua method name ("read_csv" /
    // "read_csv_stream"), threaded into every Pattern 1 message the same way
    // resolve_sandboxed_path threads its own. Throws (Pattern 1, naming `operation`) if the
    // resolved path does not exist, is a directory, is empty, or if the underlying parser fails
    // to open/read it.
    Reader(std::string resolved_path, std::string original_path, std::string operation, Options options = {});
    ~Reader();

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) noexcept;
    Reader& operator=(Reader&&) noexcept;

    // Raw column names, in file order, verbatim (duplicates/blanks/surrounding spaces preserved).
    const std::vector<std::string>& header() const;

    // Feeds every data row to `sink`, in file order, and returns the number of rows fed to it. A
    // sink returning false stops the read early; the returned count is then partial.
    int64_t for_each_row(const RowSink& sink);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver::csv_read

#endif  // QUIVER_SRC_CSV_READ_H
