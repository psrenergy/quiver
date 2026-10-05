#include "sandbox/internal.h"
#include "utils/number.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiver::lua_internal {

namespace {

// Depth cap for the return-value encoder. Doubles as the cycle guard: a self-referencing table
// would otherwise recurse until the stack dies, and the script is untrusted input to a host.
constexpr int kMaxReturnDepth = 32;

// Size cap for the encoded result. The depth cap alone does not bound the output: a table that
// shares sub-tables (`local t = {1}; for _ = 1, 30 do t = {t, t} end`) is only 31 levels deep but
// expands to 2^30 nodes, so an untrusted script could otherwise hang the host and exhaust memory.
constexpr std::size_t kMaxReturnBytes = 64UL * 1024UL * 1024UL;

// Plain std::string, not std::ostringstream: no locale (num_put would insert thousands separators
// under a grouping global locale, producing invalid JSON), no per-token sentry, no final copy out.
void append_json(const sol::object& value, std::string& out, int depth);

void append_json_string(std::string_view str, std::string& out) {
    static constexpr char kHex[] = "0123456789abcdef";

    out += '"';
    // Runs of bytes that need no escaping are appended whole.
    std::size_t run = 0;
    const auto flush_run = [&](std::size_t end) {
        if (end > run) {
            out.append(str.data() + run, end - run);
        }
    };

    for (std::size_t i = 0; i < str.size(); ++i) {
        const auto ch = static_cast<unsigned char>(str[i]);
        const char* escape = nullptr;
        switch (ch) {
        case '"':
            escape = "\\\"";
            break;
        case '\\':
            escape = "\\\\";
            break;
        case '\b':
            escape = "\\b";
            break;
        case '\f':
            escape = "\\f";
            break;
        case '\n':
            escape = "\\n";
            break;
        case '\r':
            escape = "\\r";
            break;
        case '\t':
            escape = "\\t";
            break;
        default:
            break;
        }

        if (escape != nullptr) {
            flush_run(i);
            out += escape;
            run = i + 1;
        } else if (ch < 0x20) {
            flush_run(i);
            out += "\\u00";
            out += kHex[ch >> 4U];
            out += kHex[ch & 0x0FU];
            run = i + 1;
        } else if (ch >= 0x80) {
            // JSON text must be UTF-8 (RFC 8259) but a Lua string is an arbitrary byte array.
            // Reject invalid sequences here -- this is the only layer that can diagnose them;
            // downstream, Python raises UnicodeDecodeError, Dart raises FormatException and JS
            // silently substitutes U+FFFD.
            // Unicode 15 table 3-7: the second byte's range narrows for four lead bytes, which is
            // what rejects overlong encodings and UTF-16 surrogates (both of which a strict
            // decoder such as Python's also rejects).
            const std::size_t extra = ch >= 0xF0 ? 3 : (ch >= 0xE0 ? 2 : (ch >= 0xC2 ? 1 : 0));
            const unsigned char lo = ch == 0xE0 ? 0xA0 : (ch == 0xF0 ? 0x90 : 0x80);
            const unsigned char hi = ch == 0xED ? 0x9F : (ch == 0xF4 ? 0x8F : 0xBF);
            auto valid = extra > 0 && ch <= 0xF4 && i + extra < str.size();
            for (std::size_t k = 1; valid && k <= extra; ++k) {
                const auto next = static_cast<unsigned char>(str[i + k]);
                valid = k == 1 ? (next >= lo && next <= hi) : (next >= 0x80 && next <= 0xBF);
            }
            if (!valid) {
                throw std::runtime_error("Cannot run: script return value contains a string that is not valid UTF-8");
            }
            i += extra;
        }
    }
    flush_run(str.size());
    out += '"';
}

void append_json_double(double value, std::string& out) {
    // JSON has no NaN/Infinity literals.
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    quiver::utils::append_number(value, out);
}

void append_json_table(const sol::table& table, std::string& out, int depth) {
    // Array iff the keys are exactly 1..n. Walked with pairs, never sol::table::size() --
    // lua_rawlen returns an arbitrary border on a table with holes.
    std::int64_t count = 0;
    std::int64_t max_index = 0;
    bool is_array = true;
    for (auto& pair : table) {
        if (!pair.first.is<std::int64_t>()) {
            is_array = false;
            break;  // count is only read by the array branch below
        }
        const auto index = pair.first.as<std::int64_t>();
        if (index < 1) {
            is_array = false;
            break;
        }
        ++count;
        max_index = std::max(max_index, index);
    }

    if (is_array && count == max_index) {
        out += '[';
        for (std::int64_t i = 1; i <= count; ++i) {
            if (i > 1) {
                out += ',';
            }
            append_json(table[i], out, depth + 1);
        }
        out += ']';
        return;
    }

    // Sorted keys so the output is deterministic (Lua's pairs order is not), matching the
    // alphabetical convention of lua_table_to_value_map. One vector, not a std::map: a large
    // returned table would otherwise cost one tree node allocation per entry.
    std::vector<std::pair<std::string, sol::object>> entries;
    for (auto& pair : table) {
        if (pair.first.is<std::string>()) {
            entries.emplace_back(pair.first.as<std::string>(), pair.second);
        } else if (pair.first.is<std::int64_t>()) {
            entries.emplace_back(std::to_string(pair.first.as<std::int64_t>()), pair.second);
        } else {
            throw std::runtime_error("Cannot run: script returned a table with an unsupported key type");
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    out += '{';
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) {
            // An integer key and a string key spelling the same text (`{[1] = 'a', ['1'] = 'b'}`)
            // are distinct in Lua but one JSON key -- refuse rather than silently drop one, in
            // whichever order pairs happened to yield them.
            if (entries[i].first == entries[i - 1].first) {
                throw std::runtime_error(
                    "Cannot run: script returned a table with duplicate key '" + entries[i].first + "'"
                );
            }
            out += ',';
        }
        append_json_string(entries[i].first, out);
        out += ':';
        append_json(entries[i].second, out, depth + 1);
    }
    out += '}';
}

void append_json(const sol::object& value, std::string& out, int depth) {
    if (depth > kMaxReturnDepth) {
        throw std::runtime_error(
            "Cannot run: script return value nests deeper than " + std::to_string(kMaxReturnDepth) + " levels"
        );
    }
    if (out.size() > kMaxReturnBytes) {
        throw std::runtime_error(
            "Cannot run: script return value exceeds " + std::to_string(kMaxReturnBytes) + " bytes of JSON"
        );
    }

    if (!value.valid() || value.is<sol::lua_nil_t>()) {
        out += "null";
    } else if (value.get_type() == sol::type::boolean) {
        out += (value.as<bool>() ? "true" : "false");
    } else if (value.is<std::int64_t>()) {
        quiver::utils::append_number(value.as<std::int64_t>(), out);
    } else if (value.is<double>()) {
        append_json_double(value.as<double>(), out);
    } else if (value.is<std::string>()) {
        append_json_string(value.as<std::string>(), out);
    } else if (value.get_type() == sol::type::table) {
        // get_type, not is<sol::table>(): sol2's table check also accepts userdata, which would
        // quietly encode the db global (or a BinaryFile) as an empty object.
        append_json_table(value.as<sol::table>(), out, depth);
    } else {
        throw std::runtime_error("Cannot run: script returned an unsupported Lua type");
    }
}

}  // namespace

std::string encode_return_json(const sol::object& value) {
    std::string out;
    append_json(value, out, 0);
    return out;
}

}  // namespace quiver::lua_internal
