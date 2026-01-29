#include "quiver/time_series_metadata.h"

namespace {
    std::vector<int64_t> build_dimension_parent_indexes(int64_t number_of_dimensions, const std::vector<int64_t>& time_dimension_indexes) {

    }

    std::vector<int64_t> compute_time_dimension_initial_values(std::time_t initial_date, const std::vector<Frequency>& frequencies) {

    }

    int64_t day_of_week_from_datetime(std::time_t datetime) {

    }
} // anonymous namespace

namespace quiver {

struct TimeSeriesMetadata::Impl {
    std::vector<std::string> dimensions;
    std::vector<std::int64_t> dimension_sizes;
    std::vector<std::string> time_dimensions;
    std::vector<Frequency> frequencies;
    std::time_t initial_datetime;
    std::string unit;
    std::vector<std::string> labels;
    std::string version;
    //
    int64_t number_of_dimensions;
    int64_t number_of_time_dimensions;
    int64_t number_of_labels;
    std::vector<int64_t> time_dimension_initial_values;
    std::vector<int64_t> time_dimension_indexes;
    std::vector<int64_t> dimension_parent_indexes;

};

} // namespace quiver