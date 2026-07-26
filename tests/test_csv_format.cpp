#include "utils/csv_format.h"

#include <gtest/gtest.h>

using namespace quiver;

// ============================================================================
// Separator pairing helpers: a ',' decimal cannot also delimit columns, so it
// pairs with ';' (and '.' grouping); a '.' decimal pairs with ',' delimiter
// (and ',' grouping). These must stay mutually consistent across export/import.
// ============================================================================

TEST(CsvFormat, FieldDelimiterForDecimal) {
    EXPECT_EQ(csv_format::field_delimiter_for_decimal(','), ';');
    EXPECT_EQ(csv_format::field_delimiter_for_decimal('.'), ',');
}

TEST(CsvFormat, DecimalSeparatorForDelimiter) {
    EXPECT_EQ(csv_format::decimal_separator_for_delimiter(';'), ',');
    EXPECT_EQ(csv_format::decimal_separator_for_delimiter(','), '.');
}

TEST(CsvFormat, GroupingSeparatorForDelimiter) {
    EXPECT_EQ(csv_format::grouping_separator_for_delimiter(';'), '.');
    EXPECT_EQ(csv_format::grouping_separator_for_delimiter(','), ',');
}

// The pairing round-trips: the delimiter chosen for a locale's decimal separator
// resolves back to that same decimal separator on import.
TEST(CsvFormat, SeparatorPairingRoundTrips) {
    for (char decimal : {',', '.'}) {
        const char delimiter = csv_format::field_delimiter_for_decimal(decimal);
        EXPECT_EQ(csv_format::decimal_separator_for_delimiter(delimiter), decimal);
    }
}

TEST(CsvFormat, LocaleDecimalSeparatorIsDotOrComma) {
    // Host-dependent, but the contract is that it is always one of the two.
    const char sep = csv_format::locale_decimal_separator();
    EXPECT_TRUE(sep == '.' || sep == ',');
}

// ============================================================================
// NumberNormalizer: rewrites a locale-formatted number to a C-locale
// ('.' decimal, no grouping) form; leaves non-numbers untouched.
// ============================================================================

// Comma-locale files (';' delimited): ',' decimals, '.' grouping.
TEST(CsvFormat, NumberNormalizer_CommaLocale) {
    const csv_format::NumberNormalizer n(',', '.');

    EXPECT_EQ(n("1000,5"), "1000.5");
    EXPECT_EQ(n("800,75"), "800.75");
    EXPECT_EQ(n("-3,5"), "-3.5");
    EXPECT_EQ(n("42"), "42");                    // plain integer, no separators
    EXPECT_EQ(n("1.234.567,89"), "1234567.89");  // thousands grouping stripped
    EXPECT_EQ(n("1.234"), "1234");               // grouped integer, not a decimal
}

// Dot-locale files (',' delimited): '.' decimals, ',' grouping.
TEST(CsvFormat, NumberNormalizer_DotLocale) {
    const csv_format::NumberNormalizer n('.', ',');

    EXPECT_EQ(n("9.99"), "9.99");
    EXPECT_EQ(n("1000.5"), "1000.5");
    EXPECT_EQ(n("-2000.25"), "-2000.25");
    EXPECT_EQ(n("1,000.5"), "1000.5");     // thousands grouping stripped
    EXPECT_EQ(n("1,234,567"), "1234567");  // grouped integer
    EXPECT_EQ(n("1,234,567.89"), "1234567.89");
}

// Anything that is not a well-formed number in the given locale is returned
// unchanged, so it flows on to the text / enum / datetime handling.
TEST(CsvFormat, NumberNormalizer_NonNumbersUnchanged) {
    const csv_format::NumberNormalizer comma(',', '.');
    EXPECT_EQ(comma(""), "");
    EXPECT_EQ(comma("abc"), "abc");
    EXPECT_EQ(comma("2024-01-15"), "2024-01-15");  // date
    EXPECT_EQ(comma("1.23"), "1.23");              // malformed group (not 3 digits) -> text

    const csv_format::NumberNormalizer dot('.', ',');
    EXPECT_EQ(dot("N/A"), "N/A");
    EXPECT_EQ(dot("1,23"), "1,23");  // malformed group -> text
}
