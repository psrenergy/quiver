#ifndef QUIVER_UTILS_CSV_FORMAT_H
#define QUIVER_UTILS_CSV_FORMAT_H

#include <locale>
#include <regex>
#include <string>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

// Locale-aware CSV number formatting helpers.
//
// Excel reads CSV numbers using the OS locale's decimal separator, so a ','
// locale (e.g. pt-BR, de-DE) needs the file written with ',' decimals — and
// because ',' then cannot also separate columns, those files use ';'. These
// helpers keep export and import symmetric across locales.
namespace quiver::csv_format {

// The decimal separator ('.' or ',') of the user's preferred locale. Falls back
// to '.' when the locale can't be determined. Reads the locale without mutating
// the global C locale (so std::stod keeps using '.').
#ifdef __APPLE__
// macOS: locale env vars (LANG/LC_*) are a Terminal convention — GUI apps
// launched from Finder/launchd don't have them, so std::locale("") would always
// resolve to "C". Query the System Settings region via CoreFoundation instead,
// which is the same source Excel for Mac reads its separators from.
inline char locale_decimal_separator() {
    CFLocaleRef loc = CFLocaleCopyCurrent();
    auto sep = static_cast<CFStringRef>(CFLocaleGetValue(loc, kCFLocaleDecimalSeparator));
    char buffer[8] = {};
    const bool ok = sep != nullptr && CFStringGetCString(sep, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(loc);
    return ok && buffer[0] == ',' ? ',' : '.';
}
#else
// Windows (MSVC): std::locale("") reads the user's regional settings from the
// OS, matching Excel. Linux: resolved from LANG/LC_* env vars.
inline char locale_decimal_separator() {
    try {
        std::locale loc("");
        auto sep = std::use_facet<std::numpunct<char>>(loc).decimal_point();
        return sep == ',' ? ',' : '.';
    } catch (...) {
        return '.';
    }
}
#endif

// The field delimiter that must be paired with a decimal separator. A ',' decimal
// forces ';' as the delimiter, because ',' cannot be both at once.
inline char field_delimiter_for_decimal(char decimal_separator) {
    return decimal_separator == ',' ? ';' : ',';
}

// The grouping (thousands) separator paired with a field delimiter: ';' files use
// ',' decimals and '.' grouping; ',' files use '.' decimals and ',' grouping.
inline char decimal_separator_for_delimiter(char field_delimiter) {
    return field_delimiter == ';' ? ',' : '.';
}
inline char grouping_separator_for_delimiter(char field_delimiter) {
    return field_delimiter == ';' ? '.' : ',';
}

// Converts a locale-formatted numeric string to a C-locale ('.' decimal, no
// grouping) string that std::stod / std::stoll can parse. Tolerates optional
// thousands separators in valid 3-digit groups (e.g. "1.234.567,89" -> "1234567.89"
// when decimal_sep is ',' and grouping_sep is '.'). Anything that is not a
// well-formed number (codes, dates, enum labels) is returned unchanged, so it can
// flow on to the existing text / enum / datetime handling.
inline std::string normalize_number(const std::string& text, char decimal_sep, char grouping_sep) {
    if (text.empty()) {
        return text;
    }

    const std::string g =
        std::regex_replace(std::string(1, grouping_sep), std::regex(R"([.^$|()\[\]{}*+?\\])"), R"(\$&)");
    const std::string d =
        std::regex_replace(std::string(1, decimal_sep), std::regex(R"([.^$|()\[\]{}*+?\\])"), R"(\$&)");

    // Optional sign; integer part either grouped in 3-digit blocks or ungrouped;
    // optional fractional part. Anything else (stray chars, bad grouping) is text.
    const std::regex number_pattern("^-?(\\d{1,3}(" + g + "\\d{3})+|\\d+)(" + d + "\\d+)?$");
    if (!std::regex_match(text, number_pattern)) {
        return text;
    }

    std::string normalized;
    normalized.reserve(text.size());
    for (char c : text) {
        if (c == grouping_sep) {
            continue;  // strip thousands separators
        }
        normalized += (c == decimal_sep) ? '.' : c;
    }
    return normalized;
}

}  // namespace quiver::csv_format

#endif  // QUIVER_UTILS_CSV_FORMAT_H
