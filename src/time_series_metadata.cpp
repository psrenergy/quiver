#include "quiver/time_series_metadata.h"
#include "quiver/dimension.h"
#include "quiver/time_dimension.h"

namespace {

} // anonymous namespace

namespace quiver {

struct TimeSeriesMetadata::Impl {
    std::vector<std::unique_ptr<Dimension>> dimensions;
    std::time_t initial_datetime;
    std::string unit;
    std::vector<std::string> labels;
    std::string version;
    //
    int64_t number_of_dimensions;
    int64_t number_of_time_dimensions;
    int64_t number_of_labels;

};

} // namespace quiver