#include "quiver/time_series_metadata.h"

namespace {
    TimeSeries& open_reader(const std::string& file_path) {

    }
    
    TimeSeries& open_writer(const std::string& file_path, const TimeSeriesMetadata& metadata) {

    }

    // This comment serves as a reminder that the following functions were deprecated, and should be used inline when needed.
    // validate_file_mode
    // validate_not_negative
    // validate_file_exists

} // anonymous namespace

namespace quiver {

struct TimeSeries::Impl {
    std::iostream io;
    std::string file_path;
    TimeSeriesMetadata metadata;
};

} // namespace quiver