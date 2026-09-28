#include <chrono>
#include <clocale>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <quiver/binary/binary_file.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/binary/csv_converter.h>
#include <quiver/element.h>
#include <sstream>
#include <string>

using namespace quiver;
namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class CSVConverterFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_csv_converter_test").string(); }

    void TearDown() override {
        for (auto ext : {".qvr", ".toml", ".csv"}) {
            auto full = path + ext;
            if (fs::exists(full))
                fs::remove(full);
        }
    }

    std::string path;

    static BinaryMetadata make_simple_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row", "col"})
                                                .set("dimension_sizes", {3, 2})
                                                .set("labels", {"val1", "val2"}));
    }

    static BinaryMetadata make_time_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"stage", "block"})
                                                .set("dimension_sizes", {4, 31})
                                                .set("time_dimensions", {"stage", "block"})
                                                .set("frequencies", {"monthly", "daily"})
                                                .set("labels", {"plant_1", "plant_2"}));
    }

    static BinaryMetadata make_hourly_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"day", "hour"})
                                                .set("dimension_sizes", {3, 24})
                                                .set("time_dimensions", {"day", "hour"})
                                                .set("frequencies", {"daily", "hourly"})
                                                .set("labels", {"val"}));
    }

    void write_toml(const BinaryMetadata& md) {
        std::ofstream f(path + ".toml");
        f << md.to_toml();
    }

    void write_csv(const std::string& content) {
        std::ofstream f(path + ".csv");
        f << content;
    }

    std::string read_csv() {
        std::ifstream f(path + ".csv");
        return {std::istreambuf_iterator<char>(f), {}};
    }

    std::vector<std::string> csv_lines() {
        auto content = read_csv();
        std::vector<std::string> lines;
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty())
                lines.push_back(line);
        }
        return lines;
    }

    // csv_to_bin(path) must throw std::runtime_error carrying exactly `message`.
    void expect_csv_to_bin_error(const std::string& message) {
        try {
            CSVConverter::csv_to_bin(path);
            FAIL() << "expected csv_to_bin to throw";
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), message.c_str());
        }
    }
};

// ============================================================================
// CSVConverterBinToCSV -- Header tests
// ============================================================================

TEST_F(CSVConverterFixture, NonTimeMetadataHeader) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "row,col,val1,val2");
}

TEST_F(CSVConverterFixture, AggregatedNoHourlyHeader) {
    auto md = make_time_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path, true);
    auto lines = csv_lines();
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "date,plant_1,plant_2");
}

TEST_F(CSVConverterFixture, AggregatedWithHourlyHeader) {
    auto md = make_hourly_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path, true);
    auto lines = csv_lines();
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "datetime,val");
}

TEST_F(CSVConverterFixture, NonAggregatedHeader) {
    auto md = make_time_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path, false);
    auto lines = csv_lines();
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "stage,block,plant_1,plant_2");
}

// ============================================================================
// CSVConverterBinToCSV -- Data correctness
// ============================================================================

TEST_F(CSVConverterFixture, CreatesCSVFile) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path);
    EXPECT_TRUE(fs::exists(path + ".csv"));
}

TEST_F(CSVConverterFixture, DataValuesMatch) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.5, 2.5}, {{"row", 1}, {"col", 1}});
        binary_file.write({3.5, 4.5}, {{"row", 1}, {"col", 2}});
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    // line 1 = header, line 2 = row=1,col=1
    ASSERT_GE(lines.size(), 3u);
    EXPECT_EQ(lines[1], "1,1,1.5,2.5");
    EXPECT_EQ(lines[2], "1,2,3.5,4.5");
}

TEST_F(CSVConverterFixture, NaNValuesAppearAsNull) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }  // all NaN
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    ASSERT_GE(lines.size(), 2u);
    EXPECT_NE(lines[1].find("null"), std::string::npos);
}

TEST_F(CSVConverterFixture, FloatPrecision) {
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"row"})
                                               .set("dimension_sizes", {1})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.23456789}, {{"row", 1}});
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    ASSERT_GE(lines.size(), 2u);
    // Shortest round-trip form (utils::append_number), not 6 significant digits.
    EXPECT_EQ(lines[1], "1,1.23456789");
}

// ============================================================================
// CSVConverterBinToCSV -- Row count
// ============================================================================

TEST_F(CSVConverterFixture, NonTimeRowCountEqualsProduct) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    // 3 * 2 = 6 data rows + 1 header
    EXPECT_EQ(lines.size(), 7u);
}

TEST_F(CSVConverterFixture, TimeMetadataRowCountLessThanProduct) {
    // Monthly + daily: 4 months * 31 max days = 124 max,
    // but Feb has 28, so actual is 31 + 28 + 31 + 28 = 118
    // (Starting Jan 2025: Jan=31, Feb=28, Mar=31, Apr=28? No: Apr has 30, but dim size is 31 max)
    // Actually: the file pre-allocates 4*31 positions but next_dimensions respects variable month lengths:
    // Jan=31d, Feb=28d, Mar=31d, Apr=30d → 31+28+31+30=120 data rows
    auto md = make_time_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    size_t data_rows = lines.size() - 1;
    // Must be less than 4 * 31 = 124 (because of variable month lengths)
    EXPECT_LT(data_rows, 124u);
    // Jan(31) + Feb(28) + Mar(31) + Apr(30) = 120
    EXPECT_EQ(data_rows, 120u);
}

TEST_F(CSVConverterFixture, HourlyMetadataRowCount) {
    auto md = make_hourly_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    size_t data_rows = lines.size() - 1;
    // 3 days * 24 hours = 72 (fixed sizes)
    EXPECT_EQ(data_rows, 72u);
}

TEST_F(CSVConverterFixture, AggregatedDateIsTheStartOfEachMonth) {
    // A monthly file starting January 31 is labelled by calendar month; it used to read 2025-01-31, 2025-03-03,
    // 2025-03-31, 2025-05-01
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-31T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month"})
                                               .set("dimension_sizes", {4})
                                               .set("time_dimensions", {"month"})
                                               .set("frequencies", {"monthly"})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path, true);
    auto lines = csv_lines();
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[1], "2025-01-01,null");
    EXPECT_EQ(lines[2], "2025-02-01,null");
    EXPECT_EQ(lines[3], "2025-03-01,null");
    EXPECT_EQ(lines[4], "2025-04-01,null");
}

TEST_F(CSVConverterFixture, AggregatedDatetimeKeepsANonMidnightStart) {
    // Monthly x hourly from 2025-01-15T06:00: the first row is the start itself (it used to be rejected, since the
    // monthly step dropped the time of day), and rows advance one hour at a time to the end of January
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-15T06:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month", "hour"})
                                               .set("dimension_sizes", {1, 744})
                                               .set("time_dimensions", {"month", "hour"})
                                               .set("frequencies", {"monthly", "hourly"})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }
    CSVConverter::bin_to_csv(path, true);
    auto lines = csv_lines();
    ASSERT_EQ(lines.size(), 1u + 402u);  // header + hours 343..744 of January
    EXPECT_EQ(lines[1], "2025-01-15T06:00:00,null");
    EXPECT_EQ(lines[2], "2025-01-15T07:00:00,null");
    EXPECT_EQ(lines.back(), "2025-01-31T23:00:00,null");
    EXPECT_NO_THROW(CSVConverter::csv_to_bin(path));  // re-reads the labels bin_to_csv wrote
}

// ============================================================================
// CSVConverterCsvToBin -- Happy paths
// ============================================================================

TEST_F(CSVConverterFixture, CsvToBinCreatesQvrFile) {
    auto md = make_simple_metadata();
    write_toml(md);
    // Write a minimal CSV
    std::string csv = "row,col,val1,val2\n";
    csv += "1,1,1.0,2.0\n1,2,3.0,4.0\n";
    csv += "2,1,5.0,6.0\n2,2,7.0,8.0\n";
    csv += "3,1,9.0,10.0\n3,2,11.0,12.0\n";
    write_csv(csv);
    CSVConverter::csv_to_bin(path);
    EXPECT_TRUE(fs::exists(path + ".qvr"));
}

TEST_F(CSVConverterFixture, CsvToBinNonTimeRoundTrip) {
    auto md = make_simple_metadata();
    write_toml(md);
    std::string csv = "row,col,val1,val2\n";
    csv += "1,1,1.5,2.5\n1,2,3.5,4.5\n";
    csv += "2,1,5.5,6.5\n2,2,7.5,8.5\n";
    csv += "3,1,9.5,10.5\n3,2,11.5,12.5\n";
    write_csv(csv);
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_DOUBLE_EQ(v[0], 1.5);
    EXPECT_DOUBLE_EQ(v[1], 2.5);
    auto v2 = reader.read({{"row", 3}, {"col", 2}});
    EXPECT_DOUBLE_EQ(v2[0], 11.5);
    EXPECT_DOUBLE_EQ(v2[1], 12.5);
}

TEST_F(CSVConverterFixture, CsvToBinAggregatedDatetimeWithHourly) {
    auto md = make_hourly_metadata();
    // First create binary, then csv, then read back
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({42.0}, {{"day", 1}, {"hour", 1}});
    }
    CSVConverter::bin_to_csv(path, true);  // creates CSV with datetime header
    // Delete .qvr so csv_to_bin recreates it
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v = reader.read({{"day", 1}, {"hour", 1}});
    EXPECT_DOUBLE_EQ(v[0], 42.0);
}

TEST_F(CSVConverterFixture, CsvToBinAggregatedDateWithoutHourly) {
    auto md = make_time_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({10.0, 20.0}, {{"stage", 1}, {"block", 1}});
    }
    CSVConverter::bin_to_csv(path, true);
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v = reader.read({{"stage", 1}, {"block", 1}});
    EXPECT_DOUBLE_EQ(v[0], 10.0);
    EXPECT_DOUBLE_EQ(v[1], 20.0);
}

TEST_F(CSVConverterFixture, CsvToBinNonAggregatedTimeDimensions) {
    auto md = make_time_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({5.0, 6.0}, {{"stage", 1}, {"block", 1}});
    }
    CSVConverter::bin_to_csv(path, false);  // non-aggregated
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v = reader.read({{"stage", 1}, {"block", 1}});
    EXPECT_DOUBLE_EQ(v[0], 5.0);
    EXPECT_DOUBLE_EQ(v[1], 6.0);
}

// ============================================================================
// CSVConverterCsvToBin -- Error cases
// ============================================================================

TEST_F(CSVConverterFixture, HeaderMismatchWrongColumnName) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,bad_name,val1,val2\n1,1,1.0,2.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

TEST_F(CSVConverterFixture, HeaderTooFewColumns) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col\n1,1\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

TEST_F(CSVConverterFixture, HeaderTooManyColumns) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2,extra\n1,1,1.0,2.0,3.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

TEST_F(CSVConverterFixture, DimensionValueMismatch) {
    auto md = make_simple_metadata();
    write_toml(md);
    // Row order wrong: should start with row=1,col=1 but starts with row=2,col=1
    write_csv("row,col,val1,val2\n2,1,1.0,2.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

TEST_F(CSVConverterFixture, NonNumericDataValue) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,abc,2.0\n");
    expect_csv_to_bin_error("Cannot csv_to_bin: invalid float value 'abc' for label 'val1'");
}

TEST_F(CSVConverterFixture, EmptyDataField) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,,2.0\n");
    expect_csv_to_bin_error("Cannot csv_to_bin: invalid float value '' for label 'val1'");
}

// std::stod read the longest valid prefix, so this cell was stored as 9.99 with no error.
TEST_F(CSVConverterFixture, TrailingGarbageDataValue) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,1.0,9.99abc\n");
    expect_csv_to_bin_error("Cannot csv_to_bin: invalid float value '9.99abc' for label 'val2'");
}

// A cell past the last label belongs to a row wider than the header, so there is no label to name
// (and indexing labels there would read past the end). Only the Pattern 1 prefix is pinned: a
// row-width check in front of the conversion would report this row first, just as correctly.
TEST_F(CSVConverterFixture, NonNumericCellPastLastLabel) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2\n1,1,1.0,2.0,abc\n");
    try {
        CSVConverter::csv_to_bin(path);
        FAIL() << "expected csv_to_bin to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_TRUE(std::string(e.what()).starts_with("Cannot csv_to_bin: ")) << e.what();
    }
}

TEST_F(CSVConverterFixture, EmptyCSVFile) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

// ============================================================================
// CSVConverterRoundTrip
// ============================================================================

TEST_F(CSVConverterFixture, BinToCsvToBinWithoutHourly) {
    auto md = make_time_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({100.0, 200.0}, {{"stage", 1}, {"block", 1}});
        binary_file.write({300.0, 400.0}, {{"stage", 2}, {"block", 15}});
    }
    CSVConverter::bin_to_csv(path, true);
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v1 = reader.read({{"stage", 1}, {"block", 1}});
    EXPECT_DOUBLE_EQ(v1[0], 100.0);
    EXPECT_DOUBLE_EQ(v1[1], 200.0);
    auto v2 = reader.read({{"stage", 2}, {"block", 15}});
    EXPECT_DOUBLE_EQ(v2[0], 300.0);
    EXPECT_DOUBLE_EQ(v2[1], 400.0);
}

TEST_F(CSVConverterFixture, CsvToBinToCsvWithoutHourly) {
    auto md = make_time_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"stage", 1}, {"block", 1}});
    }
    CSVConverter::bin_to_csv(path, true);
    auto csv1 = read_csv();

    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);
    fs::remove(path + ".csv");
    CSVConverter::bin_to_csv(path, true);
    auto csv2 = read_csv();

    EXPECT_EQ(csv1, csv2);
}

TEST_F(CSVConverterFixture, BinToCsvToBinWithHourly) {
    auto md = make_hourly_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({77.0}, {{"day", 1}, {"hour", 12}});
        binary_file.write({88.0}, {{"day", 3}, {"hour", 24}});
    }
    CSVConverter::bin_to_csv(path, true);

    // Verify datetime strings include time component
    auto lines = csv_lines();
    ASSERT_GE(lines.size(), 2u);
    EXPECT_EQ(lines[0], "datetime,val");
    // Check some datetime lines contain 'T'
    bool has_time_component = false;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].find('T') != std::string::npos) {
            has_time_component = true;
            break;
        }
    }
    EXPECT_TRUE(has_time_component);

    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"day", 1}, {"hour", 12}})[0], 77.0);
    EXPECT_DOUBLE_EQ(reader.read({{"day", 3}, {"hour", 24}})[0], 88.0);
}

TEST_F(CSVConverterFixture, CsvToBinToCsvWithHourly) {
    auto md = make_hourly_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0}, {{"day", 1}, {"hour", 1}});
    }
    CSVConverter::bin_to_csv(path, true);
    auto csv1 = read_csv();

    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);
    fs::remove(path + ".csv");
    CSVConverter::bin_to_csv(path, true);
    auto csv2 = read_csv();

    EXPECT_EQ(csv1, csv2);
}

TEST_F(CSVConverterFixture, RoundTripWithNullValues) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
    }  // all NaN
    CSVConverter::bin_to_csv(path);

    // Verify CSV contains "null"
    auto lines = csv_lines();
    ASSERT_GE(lines.size(), 2u);
    EXPECT_NE(lines[1].find("null"), std::string::npos);

    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v = reader.read({{"row", 1}, {"col", 1}}, true);
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_TRUE(std::isnan(v[1]));
}

// bin_to_csv writes each value in the shortest form that reads back to the same double, and
// csv_to_bin parses it whole, so the round trip is exact. {:.6g} brought 1.23456789 back as 1.23457,
// and std::stod rejected the subnormal on Linux/macOS (ERANGE -> out_of_range). EXPECT_EQ, not
// EXPECT_DOUBLE_EQ: 0.1 + 0.2 and 0.3 are one ULP apart, inside EXPECT_DOUBLE_EQ's tolerance.
TEST_F(CSVConverterFixture, RoundTripIsLossless) {
    const std::vector<double> values = {1.23456789, 0.1 + 0.2, 1234567.89, -2.5e300, 1e-310};
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"row"})
                                               .set("dimension_sizes", {5})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        for (size_t i = 0; i < values.size(); ++i) {
            binary_file.write({values[i]}, {{"row", static_cast<int64_t>(i + 1)}});
        }
    }
    CSVConverter::bin_to_csv(path);
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(reader.read({{"row", static_cast<int64_t>(i + 1)}})[0], values[i]) << "row " << (i + 1);
    }
}

// A host can switch the process's C locale to a decimal comma -- Python's
// locale.setlocale(locale.LC_ALL, "") does on a pt-BR or de-DE machine -- and strtod follows it,
// while bin_to_csv writes '.' in every locale. std::stod read "1.5" back as 1 there.
TEST_F(CSVConverterFixture, DecimalCommaLocaleReadsWrittenFloats) {
    struct RestoreNumericLocale {
        std::string saved = std::setlocale(LC_NUMERIC, nullptr);
        ~RestoreNumericLocale() { std::setlocale(LC_NUMERIC, saved.c_str()); }
    } restore;
    bool switched = false;
    for (const char* name : {"pt-BR", "pt_BR.UTF-8", "de_DE.UTF-8"}) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr) {
            switched = true;
            break;
        }
    }
    if (!switched) {
        GTEST_SKIP() << "no decimal-comma locale installed";
    }

    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.5, 0.1 + 0.2}, {{"row", 1}, {"col", 1}});
    }
    CSVConverter::bin_to_csv(path);
    auto lines = csv_lines();
    ASSERT_GE(lines.size(), 2u);
    EXPECT_EQ(lines[1], "1,1,1.5,0.30000000000000004");
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    auto v = reader.read({{"row", 1}, {"col", 1}});
    EXPECT_EQ(v[0], 1.5);
    EXPECT_EQ(v[1], 0.1 + 0.2);
}

TEST_F(CSVConverterFixture, AggregatedAndNonAggregatedProduceSameBinary) {
    auto md = make_time_metadata();

    std::vector<double> v1a, v1b, v2a, v2b;

    // Aggregated round-trip
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"stage", 1}, {"block", 1}});
        binary_file.write({3.0, 4.0}, {{"stage", 1}, {"block", 5}});
    }
    CSVConverter::bin_to_csv(path, true);
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);
    {
        auto reader = BinaryFile::open_file(path, 'r');
        v1a = reader.read({{"stage", 1}, {"block", 1}});
        v1b = reader.read({{"stage", 1}, {"block", 5}});
    }

    // Reset
    fs::remove(path + ".qvr");
    fs::remove(path + ".csv");

    // Non-aggregated round-trip
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"stage", 1}, {"block", 1}});
        binary_file.write({3.0, 4.0}, {{"stage", 1}, {"block", 5}});
    }
    CSVConverter::bin_to_csv(path, false);
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);
    {
        auto reader = BinaryFile::open_file(path, 'r');
        v2a = reader.read({{"stage", 1}, {"block", 1}});
        v2b = reader.read({{"stage", 1}, {"block", 5}});
    }

    EXPECT_DOUBLE_EQ(v1a[0], v2a[0]);
    EXPECT_DOUBLE_EQ(v1a[1], v2a[1]);
    EXPECT_DOUBLE_EQ(v1b[0], v2b[0]);
    EXPECT_DOUBLE_EQ(v1b[1], v2b[1]);
}

TEST_F(CSVConverterFixture, RoundTripMixedTimeAndNonTime) {
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month", "scenario", "day"})
                                               .set("dimension_sizes", {2, 2, 31})
                                               .set("time_dimensions", {"month", "day"})
                                               .set("frequencies", {"monthly", "daily"})
                                               .set("labels", {"val"}));
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({99.0}, {{"month", 1}, {"scenario", 1}, {"day", 1}});
        binary_file.write({77.0}, {{"month", 2}, {"scenario", 2}, {"day", 15}});
    }
    CSVConverter::bin_to_csv(path, true);
    fs::remove(path + ".qvr");
    CSVConverter::csv_to_bin(path);

    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_DOUBLE_EQ(reader.read({{"month", 1}, {"scenario", 1}, {"day", 1}})[0], 99.0);
    EXPECT_DOUBLE_EQ(reader.read({{"month", 2}, {"scenario", 2}, {"day", 15}})[0], 77.0);
}

// ============================================================================
// CSVConverterValidateHeader
// ============================================================================

TEST_F(CSVConverterFixture, ValidateHeaderCorrect) {
    auto md = make_simple_metadata();
    {
        auto binary_file = BinaryFile::open_file(path, 'w', md);
        binary_file.write({1.0, 2.0}, {{"row", 1}, {"col", 1}});
    }
    CSVConverter::bin_to_csv(path);
    // If we can convert back, header was valid
    fs::remove(path + ".qvr");
    EXPECT_NO_THROW(CSVConverter::csv_to_bin(path));
}

TEST_F(CSVConverterFixture, ValidateHeaderWrongColumn) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,wrong,val1,val2\n1,1,1.0,2.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

TEST_F(CSVConverterFixture, ValidateHeaderExtraColumn) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1,val2,extra\n1,1,1.0,2.0,3.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

TEST_F(CSVConverterFixture, ValidateHeaderMissingColumn) {
    auto md = make_simple_metadata();
    write_toml(md);
    write_csv("row,col,val1\n1,1,1.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

// ============================================================================
// CSVConverterValidateDimensions
// ============================================================================

TEST_F(CSVConverterFixture, ValidateDimensionsCorrect) {
    auto md = make_simple_metadata();
    write_toml(md);
    std::string csv = "row,col,val1,val2\n";
    csv += "1,1,1.0,2.0\n1,2,3.0,4.0\n";
    csv += "2,1,5.0,6.0\n2,2,7.0,8.0\n";
    csv += "3,1,9.0,10.0\n3,2,11.0,12.0\n";
    write_csv(csv);
    EXPECT_NO_THROW(CSVConverter::csv_to_bin(path));
}

TEST_F(CSVConverterFixture, ValidateDimensionsWrongValue) {
    auto md = make_simple_metadata();
    write_toml(md);
    // First row should be row=1,col=1 but we put row=1,col=2
    write_csv("row,col,val1,val2\n1,2,1.0,2.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}

TEST_F(CSVConverterFixture, ValidateDimensionsWrongAggregatedDatetime) {
    auto md = make_time_metadata();
    write_toml(md);
    // Should be 2025-01-01 but we put 2025-02-01
    write_csv("date,plant_1,plant_2\n2025-02-01,1.0,2.0\n");
    EXPECT_THROW(CSVConverter::csv_to_bin(path), std::runtime_error);
}
