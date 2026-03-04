#include <chrono>
#include <gtest/gtest.h>
#include <quiver/blob/time_properties.h>

using namespace quiver;
using namespace std::chrono;

// ============================================================================
// TimeFrequencyConversion
// ============================================================================

TEST(TimeFrequencyConversion, FrequencyToStringYearly) {
    EXPECT_EQ(frequency_to_string(TimeFrequency::Yearly), "yearly");
}

TEST(TimeFrequencyConversion, FrequencyToStringMonthly) {
    EXPECT_EQ(frequency_to_string(TimeFrequency::Monthly), "monthly");
}

TEST(TimeFrequencyConversion, FrequencyToStringWeekly) {
    EXPECT_EQ(frequency_to_string(TimeFrequency::Weekly), "weekly");
}

TEST(TimeFrequencyConversion, FrequencyToStringDaily) {
    EXPECT_EQ(frequency_to_string(TimeFrequency::Daily), "daily");
}

TEST(TimeFrequencyConversion, FrequencyToStringHourly) {
    EXPECT_EQ(frequency_to_string(TimeFrequency::Hourly), "hourly");
}

TEST(TimeFrequencyConversion, FrequencyFromStringYearly) {
    EXPECT_EQ(frequency_from_string("yearly"), TimeFrequency::Yearly);
}

TEST(TimeFrequencyConversion, FrequencyFromStringMonthly) {
    EXPECT_EQ(frequency_from_string("monthly"), TimeFrequency::Monthly);
}

TEST(TimeFrequencyConversion, FrequencyFromStringWeekly) {
    EXPECT_EQ(frequency_from_string("weekly"), TimeFrequency::Weekly);
}

TEST(TimeFrequencyConversion, FrequencyFromStringDaily) {
    EXPECT_EQ(frequency_from_string("daily"), TimeFrequency::Daily);
}

TEST(TimeFrequencyConversion, FrequencyFromStringHourly) {
    EXPECT_EQ(frequency_from_string("hourly"), TimeFrequency::Hourly);
}

TEST(TimeFrequencyConversion, FrequencyFromStringUnknown) {
    EXPECT_THROW(frequency_from_string("unknown"), std::invalid_argument);
}

TEST(TimeFrequencyConversion, FrequencyFromStringEmpty) {
    EXPECT_THROW(frequency_from_string(""), std::invalid_argument);
}

TEST(TimeFrequencyConversion, FrequencyFromStringCaseSensitive) {
    EXPECT_THROW(frequency_from_string("Yearly"), std::invalid_argument);
    EXPECT_THROW(frequency_from_string("MONTHLY"), std::invalid_argument);
}

TEST(TimeFrequencyConversion, RoundTrip) {
    for (auto freq : {TimeFrequency::Yearly,
                      TimeFrequency::Monthly,
                      TimeFrequency::Weekly,
                      TimeFrequency::Daily,
                      TimeFrequency::Hourly}) {
        EXPECT_EQ(frequency_from_string(frequency_to_string(freq)), freq);
    }
}

// ============================================================================
// TimePropertiesSetters
// ============================================================================

TEST(TimePropertiesSetters, SetInitialValue) {
    TimeProperties props{TimeFrequency::Daily, 1, -1};
    props.set_initial_value(15);
    EXPECT_EQ(props.initial_value, 15);
}

TEST(TimePropertiesSetters, SetParentDimensionIndex) {
    TimeProperties props{TimeFrequency::Daily, 1, -1};
    props.set_parent_dimension_index(2);
    EXPECT_EQ(props.parent_dimension_index, 2);
}

// ============================================================================
// TimePropertiesDatetimeToInt
// ============================================================================

TEST(TimePropertiesDatetimeToInt, MonthlyJanuary) {
    TimeProperties props{TimeFrequency::Monthly, 1, -1};
    auto dt = sys_days{2025y / January / 15d};
    EXPECT_EQ(props.datetime_to_int(dt), 1);
}

TEST(TimePropertiesDatetimeToInt, MonthlyMarch) {
    TimeProperties props{TimeFrequency::Monthly, 1, -1};
    auto dt = sys_days{2025y / March / 1d};
    EXPECT_EQ(props.datetime_to_int(dt), 3);
}

TEST(TimePropertiesDatetimeToInt, MonthlyDecember) {
    TimeProperties props{TimeFrequency::Monthly, 1, -1};
    auto dt = sys_days{2025y / December / 31d};
    EXPECT_EQ(props.datetime_to_int(dt), 12);
}

TEST(TimePropertiesDatetimeToInt, DailyFirst) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto dt = sys_days{2025y / January / 1d};
    EXPECT_EQ(props.datetime_to_int(dt), 1);
}

TEST(TimePropertiesDatetimeToInt, DailyFifteenth) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto dt = sys_days{2025y / January / 15d};
    EXPECT_EQ(props.datetime_to_int(dt), 15);
}

TEST(TimePropertiesDatetimeToInt, DailyThirtyFirst) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto dt = sys_days{2025y / January / 31d};
    EXPECT_EQ(props.datetime_to_int(dt), 31);
}

TEST(TimePropertiesDatetimeToInt, HourlyMidnight) {
    TimeProperties props{TimeFrequency::Hourly, 1, 0};
    auto dt = sys_days{2025y / January / 1d} + hours{0};
    EXPECT_EQ(props.datetime_to_int(dt), 1);  // 0 + 1
}

TEST(TimePropertiesDatetimeToInt, HourlyNoon) {
    TimeProperties props{TimeFrequency::Hourly, 1, 0};
    auto dt = sys_days{2025y / January / 1d} + hours{12};
    EXPECT_EQ(props.datetime_to_int(dt), 13);  // 12 + 1
}

TEST(TimePropertiesDatetimeToInt, HourlyLastHour) {
    TimeProperties props{TimeFrequency::Hourly, 1, 0};
    auto dt = sys_days{2025y / January / 1d} + hours{23};
    EXPECT_EQ(props.datetime_to_int(dt), 24);  // 23 + 1
}

TEST(TimePropertiesDatetimeToInt, YearlyThrows) {
    TimeProperties props{TimeFrequency::Yearly, 1, -1};
    auto dt = sys_days{2025y / January / 1d};
    EXPECT_THROW(props.datetime_to_int(dt), std::invalid_argument);
}

TEST(TimePropertiesDatetimeToInt, WeeklyThrows) {
    TimeProperties props{TimeFrequency::Weekly, 1, -1};
    auto dt = sys_days{2025y / January / 1d};
    EXPECT_THROW(props.datetime_to_int(dt), std::invalid_argument);
}

// ============================================================================
// TimePropertiesAddOffset
// ============================================================================

TEST(TimePropertiesAddOffset, YearlyAddsYears) {
    TimeProperties props{TimeFrequency::Yearly, 1, -1};
    auto base = sys_days{2025y / January / 1d};
    auto result = props.add_offset_from_int(base, 3);
    auto ymd = year_month_day{floor<days>(result)};
    EXPECT_EQ(ymd.year(), 2027y);
}

TEST(TimePropertiesAddOffset, MonthlyAddsMonths) {
    TimeProperties props{TimeFrequency::Monthly, 1, -1};
    auto base = sys_days{2025y / January / 1d};
    auto result = props.add_offset_from_int(base, 4);
    auto ymd = year_month_day{floor<days>(result)};
    EXPECT_EQ(ymd.month(), April);
}

TEST(TimePropertiesAddOffset, WeeklyAddsWeeks) {
    TimeProperties props{TimeFrequency::Weekly, 1, -1};
    auto base = sys_days{2025y / January / 1d};
    auto result = props.add_offset_from_int(base, 3);
    auto expected = base + weeks{2};
    EXPECT_EQ(result, expected);
}

TEST(TimePropertiesAddOffset, DailyAddsDays) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto base = sys_days{2025y / January / 1d};
    auto result = props.add_offset_from_int(base, 15);
    auto ymd = year_month_day{floor<days>(result)};
    EXPECT_EQ(ymd.day(), 15d);
}

TEST(TimePropertiesAddOffset, HourlyAddsHours) {
    TimeProperties props{TimeFrequency::Hourly, 1, 0};
    auto base = sys_days{2025y / January / 1d};
    auto result = props.add_offset_from_int(base, 13);
    auto expected = base + hours{12};
    EXPECT_EQ(result, expected);
}

TEST(TimePropertiesAddOffset, NoOpWhenValueEqualsInitial) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto base = sys_days{2025y / January / 1d};
    auto result = props.add_offset_from_int(base, 1);
    EXPECT_EQ(result, base);
}

TEST(TimePropertiesAddOffset, NonOneInitialValue) {
    TimeProperties props{TimeFrequency::Daily, 5, 0};
    auto base = sys_days{2025y / January / 5d};
    auto result = props.add_offset_from_int(base, 10);
    auto ymd = year_month_day{floor<days>(result)};
    EXPECT_EQ(ymd.day(), 10d);
}
