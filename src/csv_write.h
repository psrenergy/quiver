#ifndef QUIVER_SRC_CSV_WRITE_H
#define QUIVER_SRC_CSV_WRITE_H

// Internal CSV writer behind the Lua-only db:write_csv binding (src/lua_runner.cpp). No public
// include/quiver/ counterpart, no QUIVER_API, no C API, no FFI binding, for the same reason
// csv_read has none: Julia/Dart/Python/JS already have native CSV libraries, and Lua needs this
// specifically because `io` is deliberately absent from its sandbox (root CLAUDE.md design
// decisions).
//
// Unlike csv_read::Reader, this class is deliberately NOT Pimpl'd (D-37): Reader hides
// csv-parser's headers from src/lua_runner.cpp (which already needs /bigobj on MSVC for sol2's
// template depth); this writer is hand-rolled with no dependency to hide, so a Pimpl here would
// be cargo cult.
//
// write_row takes std::vector<std::string>, not an optional or variant cell type: a nil cell and
// an empty-string cell are the same empty cell (D-40), so all Lua type dispatch (string / integer
// / float / boolean / nil / rejected) happens in src/lua_runner.cpp and this class never sees a
// sol2 type.
//
// The full Pattern 1 message catalogue for this feature (this header/cpp plus the write_row cell
// formatter it feeds from src/lua_runner.cpp) is pinned as a comment block at the top of
// src/csv_write.cpp -- TEST-12 matches on those exact strings; do not reword any of them without
// updating that comment and the tests together.

#include <fstream>
#include <string>
#include <vector>

namespace quiver::csv_write {

struct Options {
    char separator = ',';
    // Column names to write as the first record. Empty means no header row.
    std::vector<std::string> header;
};

class Writer {
public:
    // `resolved_path` is the sandbox-checked absolute path used for the actual file access;
    // `original_path` is the caller's own spelling, quoted verbatim in every error message this
    // constructor raises -- never the resolved path. `operation` is the public Lua method name
    // that constructed this instance ("write_csv"), threaded into every Pattern 1 message the
    // same way csv_read::Reader does. Truncates the target at open (WRITE-08) -- no overwrite
    // guard exists, by decision (threat T-04-02, accepted). Writes the header record (if any) as
    // the first record, through the same emitter every data row uses, before returning.
    Writer(std::string resolved_path, std::string original_path, std::string operation, Options options = {});
    ~Writer();

    // Non-movable as well as non-copyable: the sole owner (LuaRunner::Impl::CsvWriter) holds a
    // shared_ptr and constructs in place, so nothing moves a Writer. A defaulted move would have
    // to claim `noexcept` over std::ofstream's move (which is not noexcept, so a throw would
    // terminate) and would leave the moved-from source with closed_ == false.
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = delete;
    Writer& operator=(Writer&&) = delete;

    // Writes one record. `operation` is the calling Lua method's own name ("write_row"), per
    // D-36: each Writer method threads its own operation string rather than one baked in at
    // construction, so a write_row failure says "Cannot write_row: ..." even though the writer
    // was opened by write_csv. Builds the whole record into a local buffer before writing it, so
    // a throw partway through can never leave a partial line on disk.
    void write_row(const std::vector<std::string>& cells, const std::string& operation);

    // Flushes and closes. Idempotent -- a second call is a no-op, not an error.
    void close(const std::string& operation);

    bool is_closed() const;

private:
    std::ofstream out_;
    char separator_;
    std::string original_path_;
    bool closed_ = false;
};

}  // namespace quiver::csv_write

#endif  // QUIVER_SRC_CSV_WRITE_H
