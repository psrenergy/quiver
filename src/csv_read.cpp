#include "csv_read.h"

#include <filesystem>
#include <internal/csv_reader.hpp>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace quiver::csv_read {

namespace fs = std::filesystem;

struct Reader::Impl {
    std::string operation;
    std::string original_path;
    std::vector<std::string> header;
    csv::CSVReader reader;

    Impl(std::string op, std::string orig, std::vector<std::string> hdr, csv::CSVReader r)
        : operation(std::move(op)), original_path(std::move(orig)), header(std::move(hdr)), reader(std::move(r)) {}
};

namespace {

// Built in exactly one place (this function), pinning exactly three things -- per D-12:
//   - delimiter: from the caller's Options.
//   - variable_columns: the library default is IGNORE_ROW, which silently discards any row whose
//     field count differs from the header. KEEP_NON_EMPTY keeps a ragged row instead (never
//     padded, never dropped) while still not firing on a fully blank line.
//   - header_row: with no header row pinned, csv-parser guesses one via a heuristic and pops
//     every record up to the guessed index -- silently eating a one-cell title line above the
//     real header, which is exactly the shape of file this milestone exists for.
// Never call guess_csv() (would reinterpret a comma file as ;/|/tab/^) and never call
// chunk_size(...): with CSV_ENABLE_THREADS forced OFF (cmake/Dependencies.cmake), the read
// window is csv-parser's own fixed default, unmultiplied by worker count -- exactly what
// PARSE-09 requires, and no caller (Lua or C++) can move it. If CSV_ENABLE_THREADS is ever
// turned back on, add format.threading(false) here to preserve that guarantee.
csv::CSVFormat make_format(const Options& options) {
    csv::CSVFormat format;
    format.delimiter(options.separator);
    format.variable_columns(csv::VariableColumnPolicy::KEEP_NON_EMPTY);
    format.header_row(0);
    return format;
}

}  // namespace

Reader::Reader(std::string resolved_path, std::string original_path, std::string operation, Options options) {
    // Validation order matters (D-22): not-found, then directory, then empty -- all before the
    // parser is ever constructed, and all quoting the caller's own path spelling, not the
    // resolved one. resolve_sandboxed_path's weakly_canonical does not require the path to
    // exist, so these three checks cannot be skipped.
    if (!fs::exists(resolved_path)) {
        throw std::runtime_error("Cannot " + operation + ": file not found: " + original_path);
    }
    if (fs::is_directory(resolved_path)) {
        throw std::runtime_error("Cannot " + operation + ": path is a directory: " + original_path);
    }
    if (fs::file_size(resolved_path) == 0) {
        throw std::runtime_error("Cannot " + operation + ": file '" + original_path + "' is empty");
    }

    try {
        csv::CSVReader reader(resolved_path, make_format(options));
        auto header = reader.get_col_names();
        impl_ = std::make_unique<Impl>(operation, original_path, std::move(header), std::move(reader));
    } catch (const std::exception& e) {
        // No csv-parser or standard-library message may reach Lua unwrapped (LUA-08).
        throw std::runtime_error("Cannot " + operation + ": cannot read file '" + original_path + "': " + e.what());
    }
}

Reader::~Reader() = default;

Reader::Reader(Reader&&) noexcept = default;

Reader& Reader::operator=(Reader&&) noexcept = default;

const std::vector<std::string>& Reader::header() const {
    return impl_->header;
}

int64_t Reader::for_each_row(const RowSink& sink) {
    int64_t index = 0;
    auto it = impl_->reader.begin();
    const auto end = impl_->reader.end();

    while (true) {
        // Only the csv-parser-facing work (dereferencing/advancing the iterator, copying fields)
        // is wrapped in try/catch. The sink invocation below deliberately sits outside this try:
        // db:read_csv_stream's sink runs untrusted Lua and may itself throw a std::runtime_error
        // (a Lua error re-thrown verbatim, per D-08) that must propagate unwrapped, not get
        // relabeled as a parser failure.
        std::optional<std::vector<std::string>> cells;
        try {
            if (it == end) {
                break;
            }
            csv::CSVRow& row = *it;
            std::vector<std::string> row_cells;
            row_cells.reserve(row.size());
            // Copy every field into an owned string inside this loop, then let `row` (and `it`'s
            // old position) die here -- retaining a csv::CSVRow pins the reader's whole mapped
            // chunk via a shared_ptr<void> and silently defeats the bounded-memory guarantee.
            for (csv::CSVField& field : row) {
                row_cells.emplace_back(field.get<std::string_view>());
            }
            ++it;
            cells = std::move(row_cells);
        } catch (const std::exception& e) {
            throw std::runtime_error("Cannot " + impl_->operation + ": cannot read file '" + impl_->original_path +
                                     "': " + e.what());
        }

        if (!cells) {
            break;
        }
        ++index;
        if (!sink(std::move(*cells), index)) {
            break;
        }
    }

    return index;
}

}  // namespace quiver::csv_read
