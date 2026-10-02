#include <gtest/gtest.h>
#include <quiver/binary/time_properties.h>

#include <chrono>

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
    for (
        auto freq :
        {TimeFrequency::Yearly,
         TimeFrequency::Monthly,
         TimeFrequency::Weekly,
         TimeFrequency::Daily,
         TimeFrequency::Hourly}
    ) {
        EXPECT_EQ(frequency_from_string(frequency_to_string(freq)), freq);
    }
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

TEST(TimePropertiesAddOffset, ValueOneIsThePeriodHoldingTheBase) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto base = sys_days{2025y / January / 1d};
    auto result = props.add_offset_from_int(base, 1);
    EXPECT_EQ(result, base);
}

TEST(TimePropertiesAddOffset, IgnoresInitialValue) {
    // Periods count from the one holding the base, not from initial_value
    TimeProperties props{TimeFrequency::Daily, 5, 0};
    auto base = sys_days{2025y / January / 5d};
    EXPECT_EQ(props.add_offset_from_int(base, 10), sys_days{2025y / January / 14d});
}

TEST(TimePropertiesAddOffset, MonthlyStartsFromTheFirstOfTheBaseMonth) {
    TimeProperties props{TimeFrequency::Monthly, 1, -1};
    auto base = sys_days{2025y / January / 31d} + hours{6};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / February / 1d});  // not March 3
}

TEST(TimePropertiesAddOffset, YearlyStartsFromJanuaryFirstOfTheBaseYear) {
    TimeProperties props{TimeFrequency::Yearly, 1, -1};
    auto base = sys_days{2024y / February / 29d};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / January / 1d});
}

TEST(TimePropertiesAddOffset, WeeklyStartsOnTheBaseDay) {
    TimeProperties props{TimeFrequency::Weekly, 1, -1};
    auto base = sys_days{2025y / March / 15d} + hours{6};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / March / 22d});
}

TEST(TimePropertiesAddOffset, DailyStartsAtMidnight) {
    TimeProperties props{TimeFrequency::Daily, 1, 0};
    auto base = sys_days{2025y / March / 15d} + hours{6};
    EXPECT_EQ(props.add_offset_from_int(base, 1), sys_days{2025y / March / 15d});
}

TEST(TimePropertiesAddOffset, HourlyStartsOnTheHour) {
    TimeProperties props{TimeFrequency::Hourly, 1, 0};
    auto base = sys_days{2025y / March / 15d} + hours{6} + minutes{30};
    EXPECT_EQ(props.add_offset_from_int(base, 2), sys_days{2025y / March / 15d} + hours{7});
}
