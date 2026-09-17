#include "csv_write.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

// TEST-12 message catalogue (D-36: {operation} is always the public Lua method the script called).
// Every throw the db:write_csv / w:write_row / w:close feature can raise, wherever it lives -- do
// NOT reword any of these without updating tests/test_lua_runner_write_csv.cpp in the same change.
//
// Raised here, in Writer's constructor (operation is always "write_csv"):
//   "Cannot write_csv: cannot access directory for '<original_path>': <os reason>"
//   "Cannot write_csv: parent directory does not exist for '<original_path>'"                (WRITE-07)
//   "Cannot write_csv: failed to open file '<original_path>'"
//   "Cannot write_csv: failed to write to file '<original_path>'"      (header record write failure)
//
// Raised here, in Writer::write_row (operation is always "write_row"):
//   "Cannot write_row: writer for '<original_path>' is already closed"                        (WRITE-05)
//   "Cannot write_row: failed to write to file '<original_path>'"          (data record write failure)
//
// Raised here, in Writer::close (operation is always "close"):
//   "Cannot close: failed to flush file '<original_path>'"
//
// Raised in src/lua_runner.cpp's cell formatter and row/option decoders (operation is always the
// Lua method that received the bad value -- "write_row" for a cell/row problem, "write_csv" for
// an options-table problem):
//   "Cannot write_row: row must be a table"                (sol2's table check also lets userdata in)
//   "Cannot write_row: row <N> cell #<M> is not a finite number"                               (FMT-05)
//   "Cannot write_row: cell #<M> has unsupported Lua type"                    (table/function/userdata)
//   "Cannot write_row: row key must be a positive integer"
//   "Cannot write_row: row key <N> exceeds the maximum width of 1000000"
//   "Cannot write_row: row <N> has <M> cells but header declares <W>"                            (FMT-07)
//   "Cannot write_csv: unknown option '<name>'"
//   "Cannot write_csv: option key must be a string"
//   "Cannot write_csv: options must be a table"
//   "Cannot write_csv: option 'separator' must be a string"
//   "Cannot write_csv: option 'separator' must be a single character"
//   "Cannot write_csv: option 'separator' must not be a quote, carriage return, newline or NUL"
//   "Cannot write_csv: option 'header' must be a table"
//   "Cannot write_csv: option 'header' key must be a positive integer"
//   "Cannot write_csv: option 'header' key <N> exceeds the maximum width of 1000000"
//   "Cannot write_csv: option 'header' entry must be a string"
//
// The sandbox (in-memory database, an escaping path) raises through the shared
// resolve_sandboxed_path choke point, unchanged by this feature -- see its own messages in
// src/lua_runner.cpp; write_csv is simply one more caller of it, always evaluated before the
// options table (LUA-10).

namespace quiver::csv_write {

namespace fs = std::filesystem;

namespace {

// Emits one record into `out`. Quotes a cell iff it contains the configured separator, the quote
// character, CR or LF (FMT-01), escaping an internal quote by doubling it. A record of exactly
// one empty cell -- and a record of zero cells -- is emitted as a single quoted empty cell
// (FMT-02, extended to the degenerate zero-cell case): an unquoted record of either shape is a
// blank line, and this project's own reader (csv_read::Reader, KEEP_NON_EMPTY) discards it. A
// multi-column record with an empty field stays unquoted -- the rule is deliberately narrow.
// Every record -- including the last -- is terminated with a single LF (FMT-03); the stream is
// opened in std::ios::binary so that LF is never translated to CRLF on Windows.
void append_record(const std::vector<std::string>& cells, char separator, std::string& out) {
    const bool lone_empty_cell = cells.size() <= 1 && (cells.empty() || cells.front().empty());

    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i > 0) {
            out += separator;
        }
        const std::string& cell = cells[i];
        // One pass over the cell, short-circuiting on the first special byte, rather than four
        // separate find() scans that each run to the end for the common (no-quoting) case.
        const bool needs_quotes = lone_empty_cell || std::any_of(cell.begin(), cell.end(), [separator](char c) {
                                      return c == separator || c == '"' || c == '\r' || c == '\n';
                                  });
        if (!needs_quotes) {
            out += cell;
            continue;
        }
        out += '"';
        for (char c : cell) {
            if (c == '"') {
                out += '"';
            }
            out += c;
        }
        out += '"';
    }

    if (cells.empty()) {
        // The loop above never ran; the degenerate zero-cell row still needs its quoted-empty text.
        out += "\"\"";
    }
    out += '\n';
}

}  // namespace

Writer::Writer(std::string resolved_path, std::string original_path, std::string operation, Options options)
    : separator_(options.separator), original_path_(std::move(original_path)) {
    // Check the parent directory the same way csv_read.cpp checks its target: non-throwing
    // std::filesystem overloads plus an explicit std::error_code, quoting original_path_ (the
    // caller's own spelling) and never the resolved path (WRITE-07). resolve_sandboxed_path's
    // weakly_canonical is existence-agnostic, so the target itself need not exist yet -- only its
    // parent directory must.
    std::error_code ec;
    const auto parent = fs::path(resolved_path).parent_path();
    if (!parent.empty()) {
        const bool exists = fs::exists(parent, ec);
        if (ec) {
            throw std::runtime_error("Cannot " + operation + ": cannot access directory for '" + original_path_ +
                                     "': " + ec.message());
        }
        if (!exists) {
            throw std::runtime_error("Cannot " + operation + ": parent directory does not exist for '" +
                                     original_path_ + "'");
        }
    }

    // Default (truncating) mode -- do NOT pass std::ios::app and no overwrite guard is added
    // (WRITE-08, threat T-04-02, accepted): opening over an existing file truncates it.
    out_.open(resolved_path, std::ios::binary | std::ios::trunc | std::ios::out);
    if (out_.fail()) {
        throw std::runtime_error("Cannot " + operation + ": failed to open file '" + original_path_ + "'");
    }

    if (!options.header.empty()) {
        std::string record;
        append_record(options.header, separator_, record);
        out_ << record;
        if (out_.fail()) {
            throw std::runtime_error("Cannot " + operation + ": failed to write to file '" + original_path_ + "'");
        }
    }
}

Writer::~Writer() {
    if (!closed_ && out_.is_open()) {
        out_.close();
    }
}

void Writer::write_row(const std::vector<std::string>& cells, const std::string& operation) {
    if (closed_) {
        throw std::runtime_error("Cannot " + operation + ": writer for '" + original_path_ + "' is already closed");
    }
    // Built entirely into a local string first, then written in one shot: a throw partway through
    // building a record (none exists yet, but a future cell-formatting error would) can never
    // leave a partial line on disk.
    std::string record;
    append_record(cells, separator_, record);
    out_ << record;
    if (out_.fail()) {
        throw std::runtime_error("Cannot " + operation + ": failed to write to file '" + original_path_ + "'");
    }
}

void Writer::close(const std::string& operation) {
    if (closed_) {
        return;
    }
    // One-shot: mark closed and release the handle BEFORE reporting a flush failure. Throwing with
    // closed_ still false left the writer permanently un-closeable -- every later close() raised
    // the same error instead of the documented no-op, and write_row reported "failed to write"
    // rather than "already closed".
    closed_ = true;
    out_.flush();
    const bool failed = out_.fail();
    out_.close();
    if (failed) {
        throw std::runtime_error("Cannot " + operation + ": failed to flush file '" + original_path_ + "'");
    }
}

bool Writer::is_closed() const {
    return closed_;
}

}  // namespace quiver::csv_write
