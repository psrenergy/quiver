#include <chrono>
#include <gtest/gtest.h>
#include <quiver/blob/blob_metadata.h>
#include <quiver/blob/dimension.h>
#include <quiver/blob/time_properties.h>
#include <quiver/element.h>

using namespace quiver;
using namespace std::chrono;

// ============================================================================
// Helper
// ============================================================================

static std::string make_valid_toml() {
    return R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "block"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["plant_1", "plant_2"]
)";
}

static Element make_valid_element() {
    return Element()
        .set("version", "1")
        .set("initial_datetime", "2025-01-01T00:00:00")
        .set("unit", "MW")
        .set("dimensions", {"stage", "block"})
        .set("dimension_sizes", {4, 31})
        .set("time_dimensions", {"stage", "block"})
        .set("frequencies", {"monthly", "daily"})
        .set("labels", {"plant_1", "plant_2"});
}

// ============================================================================
// BlobMetadataFromToml
// ============================================================================

TEST(BlobMetadataFromToml, AllFieldsPopulated) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    EXPECT_EQ(md.version, "1");
    EXPECT_EQ(md.unit, "MW");
    EXPECT_EQ(md.labels.size(), 2u);
    EXPECT_EQ(md.labels[0], "plant_1");
    EXPECT_EQ(md.labels[1], "plant_2");
    EXPECT_EQ(md.dimensions.size(), 2u);
    EXPECT_EQ(md.dimensions[0].name, "stage");
    EXPECT_EQ(md.dimensions[0].size, 4);
    EXPECT_EQ(md.dimensions[1].name, "block");
    EXPECT_EQ(md.dimensions[1].size, 31);
    EXPECT_EQ(md.number_of_time_dimensions, 2);
}

TEST(BlobMetadataFromToml, TimeDimensionsMarked) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    EXPECT_TRUE(md.dimensions[0].is_time_dimension());
    EXPECT_TRUE(md.dimensions[1].is_time_dimension());
}

TEST(BlobMetadataFromToml, FrequenciesAssigned) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    EXPECT_EQ(md.dimensions[0].time->frequency, TimeFrequency::Monthly);
    EXPECT_EQ(md.dimensions[1].time->frequency, TimeFrequency::Daily);
}

TEST(BlobMetadataFromToml, ParentIndicesSet) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    EXPECT_EQ(md.dimensions[0].time->parent_dimension_index, -1);
    EXPECT_EQ(md.dimensions[1].time->parent_dimension_index, 0);
}

TEST(BlobMetadataFromToml, InitialValuesJan1st) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    // Outermost time dim always starts at 1
    EXPECT_EQ(md.dimensions[0].time->initial_value, 1);
    // Daily under monthly, Jan 1st → day 1
    EXPECT_EQ(md.dimensions[1].time->initial_value, 1);
}

TEST(BlobMetadataFromToml, InitialValuesNonJan1st) {
    std::string toml = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "block"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-03-15T00:00:00"
unit = "MW"
labels = ["val"]
)";
    auto md = BlobMetadata::from_toml(toml);
    EXPECT_EQ(md.dimensions[0].time->initial_value, 1);
    EXPECT_EQ(md.dimensions[1].time->initial_value, 15);
}

TEST(BlobMetadataFromToml, InitialValuesHourlyOffset) {
    std::string toml = R"(
version = "1"
dimensions = ["month", "day", "hour"]
dimension_sizes = [12, 31, 24]
time_dimensions = ["month", "day", "hour"]
frequencies = ["monthly", "daily", "hourly"]
initial_datetime = "2025-01-01T10:00:00"
unit = "MW"
labels = ["val"]
)";
    auto md = BlobMetadata::from_toml(toml);
    EXPECT_EQ(md.dimensions[0].time->initial_value, 1);
    EXPECT_EQ(md.dimensions[1].time->initial_value, 1);
    EXPECT_EQ(md.dimensions[2].time->initial_value, 11);  // hour 10 → 10+1=11
}

TEST(BlobMetadataFromToml, MixedTimeAndNonTime) {
    std::string toml = R"(
version = "1"
dimensions = ["stage", "scenario", "block"]
dimension_sizes = [4, 3, 31]
time_dimensions = ["stage", "block"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    auto md = BlobMetadata::from_toml(toml);
    EXPECT_EQ(md.dimensions.size(), 3u);
    EXPECT_TRUE(md.dimensions[0].is_time_dimension());
    EXPECT_FALSE(md.dimensions[1].is_time_dimension());
    EXPECT_TRUE(md.dimensions[2].is_time_dimension());
    EXPECT_EQ(md.number_of_time_dimensions, 2);
}

TEST(BlobMetadataFromToml, ErrorTimeDimensionNotInDimensions) {
    std::string toml = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["stage", "nonexistent"]
frequencies = ["monthly", "daily"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    EXPECT_THROW(BlobMetadata::from_toml(toml), std::runtime_error);
}

TEST(BlobMetadataFromToml, ErrorTimeDimensionsOutOfOrder) {
    std::string toml = R"(
version = "1"
dimensions = ["stage", "block"]
dimension_sizes = [4, 31]
time_dimensions = ["block", "stage"]
frequencies = ["daily", "monthly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    EXPECT_THROW(BlobMetadata::from_toml(toml), std::runtime_error);
}

TEST(BlobMetadataFromToml, NoTimeDimensions) {
    std::string toml = R"(
version = "1"
dimensions = ["row", "col"]
dimension_sizes = [3, 2]
time_dimensions = []
frequencies = []
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    auto md = BlobMetadata::from_toml(toml);
    EXPECT_EQ(md.number_of_time_dimensions, 0);
    EXPECT_FALSE(md.dimensions[0].is_time_dimension());
    EXPECT_FALSE(md.dimensions[1].is_time_dimension());
}

// ============================================================================
// BlobMetadataFromElement
// ============================================================================

TEST(BlobMetadataFromElement, ValidWithTimeDimensions) {
    auto md = BlobMetadata::from_element(make_valid_element());
    EXPECT_EQ(md.version, "1");
    EXPECT_EQ(md.unit, "MW");
    EXPECT_EQ(md.labels.size(), 2u);
    EXPECT_EQ(md.dimensions.size(), 2u);
    EXPECT_EQ(md.dimensions[0].name, "stage");
    EXPECT_EQ(md.dimensions[0].size, 4);
    EXPECT_EQ(md.dimensions[1].name, "block");
    EXPECT_EQ(md.dimensions[1].size, 31);
    EXPECT_TRUE(md.dimensions[0].is_time_dimension());
    EXPECT_TRUE(md.dimensions[1].is_time_dimension());
    EXPECT_EQ(md.dimensions[0].time->frequency, TimeFrequency::Monthly);
    EXPECT_EQ(md.dimensions[1].time->frequency, TimeFrequency::Daily);
    EXPECT_EQ(md.dimensions[0].time->parent_dimension_index, -1);
    EXPECT_EQ(md.dimensions[1].time->parent_dimension_index, 0);
    EXPECT_EQ(md.number_of_time_dimensions, 2);
}

TEST(BlobMetadataFromElement, ValidWithoutTimeDimensions) {
    auto md = BlobMetadata::from_element(Element()
                                             .set("version", "1")
                                             .set("initial_datetime", "2025-01-01T00:00:00")
                                             .set("unit", "MW")
                                             .set("dimensions", {"row", "col"})
                                             .set("dimension_sizes", {3, 2})
                                             .set("labels", {"val"}));
    EXPECT_EQ(md.number_of_time_dimensions, 0);
    EXPECT_FALSE(md.dimensions[0].is_time_dimension());
    EXPECT_FALSE(md.dimensions[1].is_time_dimension());
}

TEST(BlobMetadataFromElement, MixedTimeAndNonTime) {
    auto md = BlobMetadata::from_element(Element()
                                             .set("version", "1")
                                             .set("initial_datetime", "2025-01-01T00:00:00")
                                             .set("unit", "MW")
                                             .set("dimensions", {"stage", "scenario", "block"})
                                             .set("dimension_sizes", {4, 3, 31})
                                             .set("time_dimensions", {"stage", "block"})
                                             .set("frequencies", {"monthly", "daily"})
                                             .set("labels", {"val"}));
    EXPECT_EQ(md.dimensions.size(), 3u);
    EXPECT_TRUE(md.dimensions[0].is_time_dimension());
    EXPECT_FALSE(md.dimensions[1].is_time_dimension());
    EXPECT_TRUE(md.dimensions[2].is_time_dimension());
}

TEST(BlobMetadataFromElement, RoundTripEquivalenceWithToml) {
    auto md_element = BlobMetadata::from_element(make_valid_element());
    auto md_toml = BlobMetadata::from_toml(make_valid_toml());

    EXPECT_EQ(md_element.version, md_toml.version);
    EXPECT_EQ(md_element.unit, md_toml.unit);
    EXPECT_EQ(md_element.labels, md_toml.labels);
    EXPECT_EQ(md_element.dimensions.size(), md_toml.dimensions.size());
    EXPECT_EQ(md_element.number_of_time_dimensions, md_toml.number_of_time_dimensions);
    for (size_t i = 0; i < md_element.dimensions.size(); ++i) {
        EXPECT_EQ(md_element.dimensions[i].name, md_toml.dimensions[i].name);
        EXPECT_EQ(md_element.dimensions[i].size, md_toml.dimensions[i].size);
        EXPECT_EQ(md_element.dimensions[i].is_time_dimension(), md_toml.dimensions[i].is_time_dimension());
        if (md_element.dimensions[i].is_time_dimension()) {
            EXPECT_EQ(md_element.dimensions[i].time->frequency, md_toml.dimensions[i].time->frequency);
            EXPECT_EQ(md_element.dimensions[i].time->initial_value, md_toml.dimensions[i].time->initial_value);
            EXPECT_EQ(md_element.dimensions[i].time->parent_dimension_index,
                      md_toml.dimensions[i].time->parent_dimension_index);
        }
    }
}

TEST(BlobMetadataFromElement, MissingVersion) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {3})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, MissingUnit) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {3})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, MissingInitialDatetime) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {3})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, MissingDimensions) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimension_sizes", {3})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, MissingDimensionSizes) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, MissingLabels) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {3})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, VersionMustBeString) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", int64_t{1})
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {3})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, DimensionsMustBeStrings) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", std::vector<int64_t>{1, 2})
                                                .set("dimension_sizes", {3, 2})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, DimensionSizesMustBeIntegers) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", std::vector<std::string>{"3"})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

TEST(BlobMetadataFromElement, InvalidVersionPropagatesValidation) {
    EXPECT_THROW(BlobMetadata::from_element(Element()
                                                .set("version", "2")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {3})
                                                .set("labels", {"val"})),
                 std::runtime_error);
}

// ============================================================================
// BlobMetadataToToml
// ============================================================================

TEST(BlobMetadataToToml, RoundTrip) {
    auto md1 = BlobMetadata::from_toml(make_valid_toml());
    auto toml_str = md1.to_toml();
    auto md2 = BlobMetadata::from_toml(toml_str);

    EXPECT_EQ(md1.version, md2.version);
    EXPECT_EQ(md1.unit, md2.unit);
    EXPECT_EQ(md1.labels, md2.labels);
    EXPECT_EQ(md1.dimensions.size(), md2.dimensions.size());
    for (size_t i = 0; i < md1.dimensions.size(); ++i) {
        EXPECT_EQ(md1.dimensions[i].name, md2.dimensions[i].name);
        EXPECT_EQ(md1.dimensions[i].size, md2.dimensions[i].size);
    }
}

TEST(BlobMetadataToToml, OutputContainsAllKeys) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    auto toml_str = md.to_toml();

    EXPECT_NE(toml_str.find("version"), std::string::npos);
    EXPECT_NE(toml_str.find("dimensions"), std::string::npos);
    EXPECT_NE(toml_str.find("dimension_sizes"), std::string::npos);
    EXPECT_NE(toml_str.find("time_dimensions"), std::string::npos);
    EXPECT_NE(toml_str.find("frequencies"), std::string::npos);
    EXPECT_NE(toml_str.find("initial_datetime"), std::string::npos);
    EXPECT_NE(toml_str.find("unit"), std::string::npos);
    EXPECT_NE(toml_str.find("labels"), std::string::npos);
}

TEST(BlobMetadataToToml, ValidationCalledOnSerialize) {
    BlobMetadata md;
    md.version = "";  // invalid
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.to_toml(), std::runtime_error);
}

TEST(BlobMetadataToToml, ISO8601DatetimeFormat) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    auto toml_str = md.to_toml();
    EXPECT_NE(toml_str.find("2025-01-01T00:00:00"), std::string::npos);
}

// ============================================================================
// BlobMetadataValidate
// ============================================================================

TEST(BlobMetadataValidate, ValidMetadataPasses) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    EXPECT_NO_THROW(md.validate());
}

TEST(BlobMetadataValidate, WrongVersion) {
    BlobMetadata md;
    md.version = "2";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, EmptyVersion) {
    BlobMetadata md;
    md.version = "";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, EmptyDimensions) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, EmptyLabels) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, NegativeTimeDimCount) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = -1;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, ExcessTimeDimCount) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = 5;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, ZeroDimensionSize) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 0, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, NegativeDimensionSize) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", -1, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, DuplicateDimensionNames) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 3, std::nullopt}, {"row", 2, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

TEST(BlobMetadataValidate, DuplicateLabels) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val", "val"};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_THROW(md.validate(), std::runtime_error);
}

// ============================================================================
// BlobMetadataValidateTimeDimensions
// ============================================================================

TEST(BlobMetadataValidateTimeDimensions, ZeroTimeDimsSkipsValidation) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    md.dimensions = {{"row", 3, std::nullopt}};
    md.number_of_time_dimensions = 0;
    EXPECT_NO_THROW(md.validate_time_dimension_metadata());
}

TEST(BlobMetadataValidateTimeDimensions, DuplicateFrequencies) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    TimeProperties t1{TimeFrequency::Monthly, 1, -1};
    TimeProperties t2{TimeFrequency::Monthly, 1, 0};
    md.dimensions = {{"stage", 12, t1}, {"block", 31, t2}};
    md.number_of_time_dimensions = 2;
    EXPECT_THROW(md.validate_time_dimension_metadata(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensions, WrongFrequencyOrder) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    TimeProperties t1{TimeFrequency::Daily, 1, -1};
    TimeProperties t2{TimeFrequency::Monthly, 1, 0};
    md.dimensions = {{"block", 31, t1}, {"stage", 12, t2}};
    md.number_of_time_dimensions = 2;
    EXPECT_THROW(md.validate_time_dimension_metadata(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensions, WeeklyNotFirst) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    TimeProperties t1{TimeFrequency::Monthly, 1, -1};
    TimeProperties t2{TimeFrequency::Weekly, 1, 0};
    md.dimensions = {{"month", 12, t1}, {"week", 52, t2}};
    md.number_of_time_dimensions = 2;
    EXPECT_THROW(md.validate_time_dimension_metadata(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensions, ValidMonthlyDaily) {
    auto md = BlobMetadata::from_toml(make_valid_toml());
    EXPECT_NO_THROW(md.validate_time_dimension_metadata());
}

TEST(BlobMetadataValidateTimeDimensions, ValidYearlyMonthlyDailyHourly) {
    std::string toml = R"(
version = "1"
dimensions = ["year", "month", "day", "hour"]
dimension_sizes = [3, 12, 31, 24]
time_dimensions = ["year", "month", "day", "hour"]
frequencies = ["yearly", "monthly", "daily", "hourly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    auto md = BlobMetadata::from_toml(toml);
    EXPECT_NO_THROW(md.validate_time_dimension_metadata());
}

TEST(BlobMetadataValidateTimeDimensions, ValidWeeklyAlone) {
    std::string toml = R"(
version = "1"
dimensions = ["week"]
dimension_sizes = [52]
time_dimensions = ["week"]
frequencies = ["weekly"]
initial_datetime = "2025-01-01T00:00:00"
unit = "MW"
labels = ["val"]
)";
    auto md = BlobMetadata::from_toml(toml);
    EXPECT_NO_THROW(md.validate_time_dimension_metadata());
}

// ============================================================================
// BlobMetadataValidateTimeDimensionSizes -- all 9 valid combinations
// ============================================================================

// Helper: build metadata with a single parent/child time-dim pair
static BlobMetadata
make_time_pair(TimeFrequency parent_freq, int64_t parent_size, TimeFrequency child_freq, int64_t child_size) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    TimeProperties tp{parent_freq, 1, -1};
    TimeProperties tc{child_freq, 1, 0};
    md.dimensions = {{frequency_to_string(parent_freq), parent_size, tp},
                     {frequency_to_string(child_freq), child_size, tc}};
    md.number_of_time_dimensions = 2;
    return md;
}

// --- Hourly under Daily ---
TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderDailyValid) {
    auto md = make_time_pair(TimeFrequency::Daily, 31, TimeFrequency::Hourly, 24);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderDailyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Daily, 31, TimeFrequency::Hourly, 23);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderDailyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Daily, 31, TimeFrequency::Hourly, 25);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Hourly under Weekly ---
TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderWeeklyValid) {
    auto md = make_time_pair(TimeFrequency::Weekly, 52, TimeFrequency::Hourly, 168);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderWeeklyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Weekly, 52, TimeFrequency::Hourly, 167);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderWeeklyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Weekly, 52, TimeFrequency::Hourly, 169);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Hourly under Monthly ---
TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderMonthlyValidMin) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Hourly, 672);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderMonthlyValidMax) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Hourly, 744);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderMonthlyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Hourly, 671);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderMonthlyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Hourly, 745);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Hourly under Yearly ---
TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderYearlyValidMin) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Hourly, 8760);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderYearlyValidMax) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Hourly, 8784);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderYearlyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Hourly, 8759);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, HourlyUnderYearlyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Hourly, 8785);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Daily under Weekly ---
TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderWeeklyValid) {
    auto md = make_time_pair(TimeFrequency::Weekly, 52, TimeFrequency::Daily, 7);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderWeeklyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Weekly, 52, TimeFrequency::Daily, 6);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderWeeklyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Weekly, 52, TimeFrequency::Daily, 8);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Daily under Monthly ---
TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderMonthlyValidMin) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Daily, 28);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderMonthlyValidMax) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Daily, 31);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderMonthlyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Daily, 27);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderMonthlyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Monthly, 12, TimeFrequency::Daily, 32);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Daily under Yearly ---
TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderYearlyValidMin) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Daily, 365);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderYearlyValidMax) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Daily, 366);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderYearlyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Daily, 364);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, DailyUnderYearlyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Daily, 367);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Monthly under Yearly ---
TEST(BlobMetadataValidateTimeDimensionSizes, MonthlyUnderYearlyValid) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Monthly, 12);
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

TEST(BlobMetadataValidateTimeDimensionSizes, MonthlyUnderYearlyBelowMin) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Monthly, 11);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

TEST(BlobMetadataValidateTimeDimensionSizes, MonthlyUnderYearlyAboveMax) {
    auto md = make_time_pair(TimeFrequency::Yearly, 3, TimeFrequency::Monthly, 13);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// --- Outermost has no size constraint ---
TEST(BlobMetadataValidateTimeDimensionSizes, OutermostHasNoSizeConstraint) {
    BlobMetadata md;
    md.version = "1";
    md.unit = "MW";
    md.labels = {"val"};
    TimeProperties tp{TimeFrequency::Yearly, 1, -1};
    md.dimensions = {{"year", 999, tp}};
    md.number_of_time_dimensions = 1;
    EXPECT_NO_THROW(md.validate_time_dimension_sizes());
}

// --- Invalid combination ---
TEST(BlobMetadataValidateTimeDimensionSizes, InvalidCombination) {
    auto md = make_time_pair(TimeFrequency::Hourly, 24, TimeFrequency::Hourly, 24);
    EXPECT_THROW(md.validate_time_dimension_sizes(), std::runtime_error);
}

// ============================================================================
// BlobMetadataAddDimension
// ============================================================================

TEST(BlobMetadataAddDimension, NonTimeDimension) {
    BlobMetadata md;
    md.add_dimension("row", 3);
    ASSERT_EQ(md.dimensions.size(), 1u);
    EXPECT_EQ(md.dimensions[0].name, "row");
    EXPECT_EQ(md.dimensions[0].size, 3);
    EXPECT_FALSE(md.dimensions[0].is_time_dimension());
}

TEST(BlobMetadataAddDimension, TimeDimension) {
    BlobMetadata md;
    md.add_time_dimension("month", 12, "monthly");
    ASSERT_EQ(md.dimensions.size(), 1u);
    EXPECT_EQ(md.dimensions[0].name, "month");
    EXPECT_EQ(md.dimensions[0].size, 12);
    EXPECT_TRUE(md.dimensions[0].is_time_dimension());
    EXPECT_EQ(md.dimensions[0].time->frequency, TimeFrequency::Monthly);
    EXPECT_EQ(md.dimensions[0].time->parent_dimension_index, -1);
}

TEST(BlobMetadataAddDimension, InvalidFrequencyThrows) {
    BlobMetadata md;
    EXPECT_THROW(md.add_time_dimension("bad", 10, "invalid_freq"), std::invalid_argument);
}

TEST(BlobMetadataAddDimension, MultipleAddsAccumulate) {
    BlobMetadata md;
    md.add_dimension("row", 3);
    md.add_dimension("col", 2);
    md.add_time_dimension("month", 12, "monthly");
    EXPECT_EQ(md.dimensions.size(), 3u);
}

TEST(BlobMetadataAddDimension, TimeDimensionParentIndexMinusOne) {
    BlobMetadata md;
    md.add_time_dimension("year", 5, "yearly");
    EXPECT_EQ(md.dimensions[0].time->parent_dimension_index, -1);
}
