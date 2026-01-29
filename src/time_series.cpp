#include "quiver/time_series_metadata.h"

namespace {
    TimeSeries& open_reader(const std::string& file_path) {

    }
    
    TimeSeries& open_writer(const std::string& file_path, const TimeSeriesMetadata& metadata) {

    }

    int64_t calculate_file_position(const TimeSeriesMetadata& metadata, const std::unordered_map<std::string, int64_t>& dims) {

    }

    std::iostream initialize_csv_writer(const std::string& file_path, const TimeSeriesMetadata& metadata, bool aggregate_time_dimensions) {

    }

    CSVReader& initialize_csv_reader(const std::string& file_path, const TimeSeriesMetadata& metadata) {

    }

    std::string build_csv_line(const TimeSeriesMetadata& metadata, const std::vector<double>& data, const std::vector<int64_t>& current_dimensions, bool aggregate_time_dimensions) {

    }

    bool time_dimensions_are_aggregated(const TimeSeriesMetadata& metadata, const std::iostream& io) {

    }

    void next_dimensions(std::vector<int64_t>& current_dimensions, const std::vector<int64_t>& dimension_sizes, const std::vector<int64_t>& initial_dimension_values, const std::vector<int64_t>& dimension_parent_indexes) {

    }

    void validate_file_mode(const std::string& mode) {

    }

    void validate_not_negative(int64_t value, const std::string& name) {

    }

    void validate_csv_header(const TimeSeriesMetadata& metadata, const std::string& header, bool aggregated_time_dimensions) {

    }

    void validate_file_exists(const std::string& file_path) {

    }

    void validate_csv_dimensions(const std::iostream& io, const TimeSeriesMetadata& metadata, const std::vector<std::string>& current_dimensions, bool aggregated_time_dimensions) {

    }

} // anonymous namespace

namespace quiver {

struct TimeSeries::Impl {
    std::iostream io;
    std::string file_path;
    TimeSeriesMetadata metadata;
};

} // namespace quiver