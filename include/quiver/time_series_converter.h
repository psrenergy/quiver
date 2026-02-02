#ifndef QUIVER_TIME_SERIES_CONVERTER_H
#define QUIVER_TIME_SERIES_CONVERTER_H

#include "export.h"

#include <vector>
#include <ctime>
#include <string>

namespace quiver {

class QUIVER_API TimeSeriesConverter {
    explicit TimeSeriesConverter(const std::string& file_path, bool aggregate_time_dimensions = true);
    ~TimeSeriesConverter();

    // Non-copyable
    TimeSeriesConverter(const TimeSeriesConverter&) = delete;
    TimeSeriesConverter& operator=(const TimeSeriesConverter&) = delete;

    // Movable
    TimeSeriesConverter(TimeSeriesConverter&& other) noexcept;
    TimeSeriesConverter& operator=(TimeSeriesConverter&& other) noexcept;

    void csv_to_bin();

    void bin_to_csv();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // CSV Builders
    std::string build_csv_line(const std::vector<double>& data, std::initializer_list<int64_t> current_dimensions);
    std::string build_datetime_string_from_time_dimension_values(std::initializer_list<int64_t> time_dimension_values) const;
    std::time_t build_datetime_from_time_dimension_values(std::initializer_list<int64_t> time_dimension_values) const;
    
    // Iterators
    std::vector<int64_t> next_dimensions(std::initializer_list<int64_t> current_dimensions);
    std::vector<int64_t> dimension_sizes_at_values(std::initializer_list<int64_t> dimension_values) const;

    // IO Initializers
    std::iostream initialize_csv_writer();
    std::iostream initialize_csv_reader();

    // Validations
    void validate_dimensions(std::initializer_list<int64_t> current_dimensions); // validate_csv_dimensions
    void validate_header(const std::string& header); //validate_csv_header
    std::vector<std::string> expected_dimension_names() const;

};

}  // namespace quiver

#endif  // QUIVER_TIME_SERIES_CONVERTER_H
