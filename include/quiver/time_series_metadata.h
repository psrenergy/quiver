#ifndef QUIVER_TIME_SERIES_METADATA_H
#define QUIVER_TIME_SERIES_METADATA_H

#include "export.h"

#include <vector>
#include <ctime>

namespace quiver {

class QUIVER_API TimeSeriesMetadata {
    explicit TimeSeriesMetadata();
    ~TimeSeriesMetadata();

    // Non-copyable
    TimeSeriesMetadata(const TimeSeriesMetadata&) = delete;
    TimeSeriesMetadata& operator=(const TimeSeriesMetadata&) = delete;

    // Movable
    TimeSeriesMetadata(TimeSeriesMetadata&& other) noexcept;
    TimeSeriesMetadata& operator=(TimeSeriesMetadata&& other) noexcept;

    // TOML Serialization
    static TimeSeriesMetadata& from_toml(const std::string& toml_content);
    std::string to_toml() const;

    // Setters
    void set_initial_datetime(const std::string& initial_datetime);
    void set_unit(const std::string& unit);
    void set_labels(const std::vector<std::string>& labels);
    void set_version(const std::string& version);
    void add_dimension(const std::string& name, int64_t size);
    void add_time_dimension(const std::string& name, int64_t size, const std::string& frequency);

    // Getters
    std::vector<int64_t> time_dimension_sizes() const;
    std::vector<int64_t> dimension_initial_values() const;
    int64_t maximum_number_of_lines() const;
    std::vector<std::string> time_dimension_names() const;
    std::vector<std::string> dimension_names() const;
    std::time_t initial_datetime() const;
    std::string unit() const;
    std::vector<std::string> labels() const;
    std::string version() const;
    int64_t number_of_dimensions() const;
    int64_t number_of_time_dimensions() const;
    int64_t number_of_labels() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

};

}  // namespace quiver

#endif  // QUIVER_TIME_SERIES_METADATA_H
