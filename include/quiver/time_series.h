#ifndef QUIVER_TIME_SERIES_H
#define QUIVER_TIME_SERIES_H

#include "export.h"

#include <vector>
#include <iostream>

namespace quiver {

class QUIVER_API TimeSeries {
    explicit TimeSeries(const std::string& file_path, const TimeSeriesMetadata& metadata, const std::iostream& io);
    ~TimeSeries();

    // Non-copyable
    TimeSeries(const TimeSeries&) = delete;
    TimeSeries& operator=(const TimeSeries&) = delete;

    // Movable
    TimeSeries(TimeSeries&& other) noexcept;
    TimeSeries& operator=(TimeSeries&& other) noexcept;

    static TimeSeries& open_file(const std::string& file_path, const std::string& mode, const std::optional<TimeSeriesMetadata>& metadata);

    void close_file();

    double read(const std::unordered_map<std::string, int64_t>& dims);

    double write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    int64_t calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const;

    void go_to_position(int64_t position);

    // Validations
    void validate_file_is_open() const;
    void validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims);
    // validate_time_dimension_values implemented inline inside validate_dimension_values
    void validate_data_length(const std::vector<double>& data);
};

}  // namespace quiver

#endif  // QUIVER_TIME_SERIES_H
