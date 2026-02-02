#include "quiver/time_series_converter.h"

namespace {

} // anonymous namespace

namespace quiver {

struct TimeSeriesConverter::Impl {
    TimeSeriesMetadata metadata;
    std::string file_path;
    bool aggregate_time_dimensions;

};

} // namespace quiver