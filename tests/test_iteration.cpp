#include <gtest/gtest.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/binary/iteration.h>
#include <quiver/element.h>

using namespace quiver;

namespace {

BinaryMetadata make_simple_metadata() {
    return BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2025-01-01T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"row", "col"})
            .set("dimension_sizes", {3, 2})
            .set("labels", {"val1", "val2"})
    );
}

BinaryMetadata make_time_metadata() {
    return BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2025-01-01T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"stage", "block"})
            .set("dimension_sizes", {4, 31})
            .set("time_dimensions", {"stage", "block"})
            .set("frequencies", {"monthly", "daily"})
            .set("labels", {"plant_1", "plant_2"})
    );
}

}  // namespace

TEST(IterationTest, FirstDimensionsOnNonTimeMetadata) {
    auto md = make_simple_metadata();
    auto first = first_dimensions(md);
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first[0], 1);
    EXPECT_EQ(first[1], 1);
}

TEST(IterationTest, FirstDimensionsOnTimeMetadata) {
    auto md = make_time_metadata();
    auto first = first_dimensions(md);
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first[0], 1);
    EXPECT_EQ(first[1], 1);
}

TEST(IterationTest, NextDimensionsTraversesNonTimeMetadata) {
    auto md = make_simple_metadata();
    std::vector<std::vector<int64_t>> positions;
    std::vector<int64_t> dims = first_dimensions(md);
    positions.push_back(dims);
    while (auto nxt = next_dimensions(md, dims)) {
        dims = *nxt;
        positions.push_back(dims);
    }
    ASSERT_EQ(positions.size(), 6u);  // 3 * 2
    EXPECT_EQ(positions[0], (std::vector<int64_t>{1, 1}));
    EXPECT_EQ(positions[1], (std::vector<int64_t>{1, 2}));
    EXPECT_EQ(positions[2], (std::vector<int64_t>{2, 1}));
    EXPECT_EQ(positions[3], (std::vector<int64_t>{2, 2}));
    EXPECT_EQ(positions[4], (std::vector<int64_t>{3, 1}));
    EXPECT_EQ(positions[5], (std::vector<int64_t>{3, 2}));
}

TEST(IterationTest, NextDimensionsReturnsNullOptOnEnd) {
    auto md = make_simple_metadata();
    auto last = next_dimensions(md, std::vector<int64_t>{3, 2});
    EXPECT_FALSE(last.has_value());
}

TEST(IterationTest, NextDimensionsTraversesTimeMetadata) {
    auto md = make_time_metadata();
    size_t count = 1;
    std::vector<int64_t> dims = first_dimensions(md);
    while (auto nxt = next_dimensions(md, dims)) {
        dims = *nxt;
        ++count;
    }
    // Jan 2025 = 31 days, Feb 2025 = 28, Mar 2025 = 31, Apr 2025 = 30 -> 120 total cells.
    EXPECT_EQ(count, 120u);
}

TEST(IterationTest, NextDimensionsResumesMidPeriodStartOnlyInTheStartingPeriod) {
    // yearly(2) x monthly(12) x daily(31) from 2025-03-15: initial values (1, 3, 15). Only March
    // 2025 starts on the 15th; March 2026 is a whole month. Checking only the parent (month == 3)
    // used to resume day 15 there too, skipping 2026-03-01..14.
    auto md = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2025-03-15T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"year", "month", "day"})
            .set("dimension_sizes", {2, 12, 31})
            .set("time_dimensions", {"year", "month", "day"})
            .set("frequencies", {"yearly", "monthly", "daily"})
            .set("labels", {"v"})
    );
    EXPECT_EQ(first_dimensions(md), (std::vector<int64_t>{1, 3, 15}));

    auto after_feb_2026 = next_dimensions(md, std::vector<int64_t>{2, 2, 28});
    ASSERT_TRUE(after_feb_2026.has_value());
    EXPECT_EQ(*after_feb_2026, (std::vector<int64_t>{2, 3, 1}));

    size_t count = 1;
    std::vector<int64_t> dims = first_dimensions(md);
    while (auto nxt = next_dimensions(md, dims)) {
        dims = *nxt;
        ++count;
    }
    // 2025-03-15..2025-12-31 = 292 days, plus the 365 days of 2026.
    EXPECT_EQ(count, 657u);
}

TEST(IterationTest, NextDimensionsResumesMidPeriodStartWhenANonTimeOuterDimensionRollsOver) {
    // scenario(2) x monthly(2) x daily(31) from 2025-01-05: each scenario restarts the calendar,
    // so scenario 2 resumes on day 5, not day 1. This is why the restore step exists.
    auto md = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2025-01-05T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"scenario", "month", "day"})
            .set("dimension_sizes", {2, 2, 31})
            .set("time_dimensions", {"month", "day"})
            .set("frequencies", {"monthly", "daily"})
            .set("labels", {"v"})
    );
    auto second_scenario = next_dimensions(md, std::vector<int64_t>{1, 2, 28});
    ASSERT_TRUE(second_scenario.has_value());
    EXPECT_EQ(*second_scenario, (std::vector<int64_t>{2, 1, 5}));

    size_t count = 1;
    std::vector<int64_t> dims = first_dimensions(md);
    while (auto nxt = next_dimensions(md, dims)) {
        dims = *nxt;
        ++count;
    }
    // Per scenario: 2025-01-05..31 (27 days) + February 2025 (28 days) = 55.
    EXPECT_EQ(count, 110u);

    // With three time levels the month must be restored before the day compares against it:
    // starts computed from a snapshot taken before the restore would give (2, 1, 3, 1) here.
    auto md4 = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2025-03-15T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"scenario", "year", "month", "day"})
            .set("dimension_sizes", {2, 2, 12, 31})
            .set("time_dimensions", {"year", "month", "day"})
            .set("frequencies", {"yearly", "monthly", "daily"})
            .set("labels", {"v"})
    );
    auto second_scenario_4 = next_dimensions(md4, std::vector<int64_t>{1, 2, 12, 31});
    ASSERT_TRUE(second_scenario_4.has_value());
    EXPECT_EQ(*second_scenario_4, (std::vector<int64_t>{2, 1, 3, 15}));
}
