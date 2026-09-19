#ifndef QUIVER_NUMBER_H
#define QUIVER_NUMBER_H

#include <array>
#include <charconv>
#include <cstddef>
#include <string>

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

}  // namespace quiver::utils

#endif  // QUIVER_NUMBER_H
