#ifndef QUIVER_TIME_SERIES_H
#define QUIVER_TIME_SERIES_H

#include "export.h"

#include <vector>
#include <iostream>

namespace quiver {

struct CSVReader {
    std::istream& io;
    bool aggregate_time_dimensions;
};

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

    static void csv_to_bin(const std::string& file_path);

    static void bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions = true);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void validate_file_is_open() const;

    void go_to_position(int64_t position);
};

}  // namespace quiver

#endif  // QUIVER_TIME_SERIES_H
