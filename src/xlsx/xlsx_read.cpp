#include "xlsx_read.h"

#include <OpenXLSX.hpp>
#include <XLXmlParser.hpp>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace quiver::xlsx_read {

namespace {

// OpenXLSX ignores pugixml's parse result. Validate entries before it consumes
// them so malformed XML cannot silently produce partial data.
class CheckedArchive : public OpenXLSX::XLZipArchive {
public:
    void addEntryAndCommit(const std::string& name, const std::string& data) {
        generated_entries_.insert(name);
        OpenXLSX::XLZipArchive::addEntryAndCommit(name, data);
    }

    std::string getEntry(const std::string& name) {
        auto data = OpenXLSX::XLZipArchive::getEntry(name);
        // Missing optional properties are created as declaration-only XML during
        // open(), then populated in the DOM. Validate file entries, not those defaults.
        if (!generated_entries_.contains(name) && (name.ends_with(".xml") || name.ends_with(".rels"))) {
            pugi::xml_document check;
            const auto result = check.load_buffer(data.data(), data.size());
            if (!result) {
                throw std::runtime_error("invalid XML in '" + name + "': " + result.description());
            }
        }
        return data;
    }

private:
    std::set<std::string> generated_entries_;
};

// A view of the existing worksheet DOM, without cell()/rows() materializing
// missing cells. It also lets us distinguish an absent formula cache from zero.
class WorksheetXml : public OpenXLSX::XLXmlFile {
public:
    explicit WorksheetXml(const OpenXLSX::XLWorksheet& sheet) : OpenXLSX::XLXmlFile(sheet) {}
    using OpenXLSX::XLXmlFile::xmlDocument;
};

int64_t integer_text(std::string_view text) {
    int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::runtime_error("invalid integer '" + std::string(text) + "'");
    }
    return value;
}

int64_t row_number(const OpenXLSX::XMLNode& row) {
    const auto number = integer_text(row.attribute("r").value());
    if (number < 1 || number > 1'048'576) {
        throw std::runtime_error("worksheet row is out of range: " + std::to_string(number));
    }
    return number;
}

// Both plain inline strings and rich text runs. Phonetic annotations are not
// part of the cell text; shared strings are already flattened by OpenXLSX.
std::string inline_text(const OpenXLSX::XMLNode& node) {
    if (const auto text = node.child("t")) {
        return text.text().get();
    }
    std::string result;
    for (const auto run : node.children("r")) {
        result += run.child("t").text().get();
    }
    return result;
}

bool blank_row(const std::vector<std::string>& cells) {
    return std::all_of(cells.begin(), cells.end(), [](const auto& cell) { return cell.empty(); });
}

bool has_cell_data(const OpenXLSX::XMLNode& cell) {
    // Some writers emit <v/> on formatting-only cells. A formula still counts,
    // so an empty/missing cache reaches cell_text's error rather than being skipped.
    return cell.child("f") || cell.child("is") || !std::string_view(cell.child("v").text().get()).empty();
}

}  // namespace

struct Reader::Impl {
    std::string operation;
    std::string original_path;
    std::string sheet_name;
    int64_t header_row;
    size_t width = 0;
    std::vector<std::string> header;
    // ponytail: worksheet XML and shared strings stay in memory. Switch the
    // backend to an XML streaming reader if workbook size makes this too costly.
    OpenXLSX::XLDocument document{CheckedArchive{}};
    OpenXLSX::XMLNode data;

    Impl(std::string op, std::string path, const std::string& resolved_path, const Options& options)
        : operation(std::move(op)), original_path(std::move(path)), header_row(options.header_row) {
        document.open(resolved_path);
        auto workbook = document.workbook();
        const auto names = workbook.worksheetNames();
        if (const auto* index = std::get_if<int64_t>(&options.sheet)) {
            if (*index < 1 || static_cast<uint64_t>(*index) > names.size()) {
                throw std::runtime_error("worksheet index not found: " + std::to_string(*index));
            }
            sheet_name = names[static_cast<size_t>(*index - 1)];
        } else {
            sheet_name = std::get<std::string>(options.sheet);
            if (std::find(names.begin(), names.end(), sheet_name) == names.end()) {
                throw std::runtime_error("worksheet not found: '" + sheet_name + "'");
            }
        }

        WorksheetXml worksheet(workbook.worksheet(sheet_name));
        data = worksheet.xmlDocument().document_element().child("sheetData");
        if (!data) {
            throw std::runtime_error("worksheet '" + sheet_name + "' has no sheetData");
        }

        OpenXLSX::XMLNode header_node;
        int64_t previous_row = 0;
        for (const auto row : data.children("row")) {
            const auto number = row_number(row);
            if (number <= previous_row) {
                throw std::runtime_error("worksheet rows must be in increasing order");
            }
            previous_row = number;
            if (number == header_row) {
                header_node = row;
            }
            for (const auto cell : row.children("c")) {
                const OpenXLSX::XLCellReference ref(cell.attribute("r").value());
                if (ref.row() != number) {
                    throw std::runtime_error("cell '" + ref.address() + "' is in the wrong worksheet row");
                }
                if (has_cell_data(cell)) {
                    width = std::max(width, static_cast<size_t>(ref.column()));
                }
            }
        }

        if (header_row != 0) {
            if (header_node) {
                header = read_row(header_node);
            }
            if (!header_node || blank_row(header)) {
                throw std::runtime_error(
                    "header row " + std::to_string(header_row) + " not found in worksheet '" + sheet_name + "'"
                );
            }
        }
    }

    std::string cell_text(const OpenXLSX::XMLNode& cell) const {
        const std::string_view type = cell.attribute("t").value();
        const auto value = cell.child("v");
        const std::string text = value.text().get();
        const std::string address = cell.attribute("r").value();
        if (cell.child("f") && (!value || (text.empty() && type != "str"))) {
            throw std::runtime_error(
                "formula in worksheet '" + sheet_name + "' cell '" + address + "' has no cached result"
            );
        }
        if (type == "inlineStr") {
            return inline_text(cell.child("is"));
        }
        if (text.empty()) {
            return {};
        }
        if (type == "s") {
            const auto index = integer_text(text);
            if (index < 0 || index >= static_cast<int64_t>(OpenXLSX::XLMaxSharedStrings)) {
                throw std::runtime_error("invalid shared string index in cell '" + address + "'");
            }
            return document.sharedStrings().getString(static_cast<int32_t>(index));
        }
        if (type == "b" && text != "0" && text != "1") {
            throw std::runtime_error("invalid boolean in cell '" + address + "'");
        }
        if (type.empty() || type == "n" || type == "b" || type == "str" || type == "e" || type == "d") {
            // XLSX numeric text already uses '.', independent of the host locale.
            // Passing it through also avoids rounding a large integer through double.
            return text;
        }
        throw std::runtime_error("unsupported cell type '" + std::string(type) + "' in cell '" + address + "'");
    }

    std::vector<std::string> read_row(const OpenXLSX::XMLNode& row) const {
        std::vector<std::string> cells(width);
        for (const auto cell : row.children("c")) {
            if (has_cell_data(cell)) {
                const OpenXLSX::XLCellReference ref(cell.attribute("r").value());
                cells[static_cast<size_t>(ref.column() - 1)] = cell_text(cell);
            }
        }
        return cells;
    }
};

Reader::Reader(std::string resolved_path, std::string original_path, std::string operation, Options options) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto status = fs::status(resolved_path, ec);
    const auto prefix = "Cannot " + operation + ": ";
    if (status.type() == fs::file_type::not_found) {
        throw std::runtime_error(prefix + "file not found: " + original_path);
    }
    if (ec) {
        throw std::runtime_error(prefix + "cannot access file '" + original_path + "': " + ec.message());
    }
    if (!fs::is_regular_file(status)) {
        throw std::runtime_error(prefix + "path is not a regular file: " + original_path);
    }
    const auto size = fs::file_size(resolved_path, ec);
    if (ec) {
        throw std::runtime_error(prefix + "cannot access file '" + original_path + "': " + ec.message());
    }
    if (size == 0) {
        throw std::runtime_error(prefix + "file '" + original_path + "' is empty");
    }

    try {
        impl_ = std::make_unique<Impl>(operation, original_path, resolved_path, options);
    } catch (const std::exception& error) {
        throw std::runtime_error(prefix + "cannot read file '" + original_path + "': " + error.what());
    }
}

Reader::~Reader() = default;

const std::vector<std::string>& Reader::header() const {
    return impl_->header;
}

int64_t Reader::for_each_row(const RowSink& sink) {
    int64_t index = 0;
    for (const auto row : impl_->data.children("row")) {
        std::vector<std::string> cells;
        try {
            if (row_number(row) <= impl_->header_row) {
                continue;
            }
            cells = impl_->read_row(row);
        } catch (const std::exception& error) {
            throw std::runtime_error(
                "Cannot " + impl_->operation + ": cannot read file '" + impl_->original_path + "': " + error.what()
            );
        }
        if (!blank_row(cells) && !sink(std::move(cells), ++index)) {
            break;
        }
    }
    return index;
}

}  // namespace quiver::xlsx_read
