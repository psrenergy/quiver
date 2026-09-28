#ifndef QUIVER_NUMBER_H
#define QUIVER_NUMBER_H

#include <array>
#include <cerrno>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace quiver::utils {

template <typename T>
void append_number(T value, std::string& out) {
    // Shortest round-trippable form, locale-independent: 0.1 stays "0.1" instead of
    // "0.10000000000000001", and 1000000 never becomes "1,000,000".
    std::array<char, 32> buffer{};
    const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    // 32 bytes covers every type this is instantiated with today (int64 needs 20, double 24), but
    // to_chars leaves the buffer contents unspecified when it reports value_too_large -- appending
    // `end - data()` bytes then injects garbage rather than failing. Append nothing instead.
    if (ec != std::errc{}) {
        return;
    }
    out.append(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
}

// The reading half of append_number: a whole-cell float parse, nullopt unless every character is
// consumed, so "9.99abc" or a decimal-comma "1,5" is not a float (strtod alone accepts any valid
// prefix). strtod reads the C locale's decimal point, which a host process can switch to ','
// (Python's locale.setlocale(LC_ALL, "") on a pt-BR machine), while append_number writes '.' in
// every locale. So the cell is spelled in the active locale first, and parses exactly as it would in
// the "C" locale.
inline std::optional<double> parse_float(std::string cell) {
    const std::string_view point = std::localeconv()->decimal_point;
    if (point != ".") {
        if (cell.find(point) != std::string::npos) {
            return std::nullopt;  // "1,5" is not a number in the "C" locale either
        }
        if (const auto dot = cell.find('.'); dot != std::string::npos) {
            cell.replace(dot, 1, point);
        }
    }
    const char* begin = cell.c_str();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(begin, &end);
    if (end == begin || end != begin + cell.size()) {
        return std::nullopt;
    }
    // ERANGE marks a literal no finite double holds -- overflow to inf, or a nonzero value flushed to
    // 0 -- but glibc and Apple libc also raise it for a representable subnormal, which append_number
    // writes, so only the first two are rejected.
    if (errno == ERANGE && (std::isinf(value) || value == 0.0)) {
        return std::nullopt;
    }
    return value;
}

}  // namespace quiver::utils

#endif  // QUIVER_NUMBER_H
