#include "csv_write.h"

#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

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
        const bool needs_quotes = lone_empty_cell || cell.find(separator) != std::string::npos ||
                                  cell.find('"') != std::string::npos || cell.find('\r') != std::string::npos ||
                                  cell.find('\n') != std::string::npos;
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
    out_.flush();
    if (out_.fail()) {
        throw std::runtime_error("Cannot " + operation + ": failed to flush file '" + original_path_ + "'");
    }
    out_.close();
    closed_ = true;
}

bool Writer::is_closed() const {
    return closed_;
}

}  // namespace quiver::csv_write
