#ifndef QUIVER_TIME_SERIES_METADATA_H
#define QUIVER_TIME_SERIES_METADATA_H

#include "export.h"

#include <vector>
#include <ctime>

namespace quiver {

// TODO: add to_string and from_string functions for Frequency
// TODO: add int_to_datetime and datetime_to_int functions
enum class Frequency { Yearly, Monthly, Weekly, Daily, Hourly };

class QUIVER_API TimeSeriesMetadata {
    explicit TimeSeriesMetadata(const std::vector<std::string>& dimensions,
                                const std::vector<int64_t>& dimension_sizes,
                                const std::vector<std::string>& time_dimensions,
                                const std::vector<std::string>& frequencies,
                                const std::string& initial_datetime,
                                const std::string& unit,
                                const std::vector<std::string>& labels,
                                const std::string& version);
    ~TimeSeriesMetadata();

    // Non-copyable
    TimeSeriesMetadata(const TimeSeriesMetadata&) = delete;
    TimeSeriesMetadata& operator=(const TimeSeriesMetadata&) = delete;

    // Movable
    TimeSeriesMetadata(TimeSeriesMetadata&& other) noexcept;
    TimeSeriesMetadata& operator=(TimeSeriesMetadata&& other) noexcept;

    static TimeSeriesMetadata& from_toml(const std::string& toml_path);

    void to_toml(const std::string& toml_path) const;

    std::vector<int64_t> time_dimension_sizes() const;

    std::vector<int64_t> dimension_initial_values() const;

    int64_t maximum_number_of_lines() const;

    std::time_t build_datetime_from_time_dimension_values(const std::vector<int64_t>& time_dimension_values) const;

    std::string build_datetime_string_from_time_dimension_values(const std::vector<int64_t>& time_dimension_values) const;

    std::vector<std::string> expected_dimension_names(bool aggregated_time_dimensions) const;

    std::vector<int64_t> dimension_sizes_at_values(const std::vector<int64_t>& dimension_values) const;

    void validate_time_dimension_values(const std::unordered_map<std::string, int64_t>& dims) const;

    void validate_data_length(const std::vector<double>& data) const;

    void validate_dimensions(const std::unordered_map<std::string, int64_t>& dims) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void validate_metadata() const;

    void validate_time_dimension_metadata() const;

    void validate_time_dimension_sizes() const;

    // Hours
    constexpr int MAX_HOURS_IN_DAY = 24;
    constexpr int MAX_HOURS_IN_WEEK = 168; // 7 * 24
    constexpr int MAX_HOURS_IN_MONTH = 744; // 31 * 24
    constexpr int MAX_HOURS_IN_YEAR = 8784; // 366 * 24

    constexpr int MIN_HOURS_IN_DAY = 24;
    constexpr int MIN_HOURS_IN_WEEK = 168; // 7 * 24
    constexpr int MIN_HOURS_IN_MONTH = 672; // 28 * 24
    constexpr int MIN_HOURS_IN_YEAR = 8760; // 365 * 24

    // Days
    constexpr int MAX_DAYS_IN_WEEK = 7;
    constexpr int MAX_DAYS_IN_MONTH = 31;
    constexpr int MAX_DAYS_IN_YEAR = 366;

    constexpr int MIN_DAYS_IN_WEEK = 7;
    constexpr int MIN_DAYS_IN_MONTH = 28;
    constexpr int MIN_DAYS_IN_YEAR = 365;

    // Weeks
    constexpr int MAX_WEEKS_IN_YEAR = 53;

    constexpr int MIN_WEEKS_IN_YEAR = 52;

    // Months

    constexpr int MAX_MONTHS_IN_YEAR = 12;

    constexpr int MIN_MONTHS_IN_YEAR = 12;
};

}  // namespace quiver

#endif  // QUIVER_TIME_SERIES_METADATA_H
