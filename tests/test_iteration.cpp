#include <gtest/gtest.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/binary/iteration.h>
#include <quiver/element.h>

using namespace quiver;

namespace {

BinaryMetadata make_simple_metadata() {
    return BinaryMetadata::from_element(Element()
                                            .set("version", "1")
                                            .set("initial_datetime", "2025-01-01T00:00:00")
                                            .set("unit", "MW")
                                            .set("dimensions", {"row", "col"})
                                            .set("dimension_sizes", {3, 2})
                                            .set("labels", {"val1", "val2"}));
}

BinaryMetadata make_time_metadata() {
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
