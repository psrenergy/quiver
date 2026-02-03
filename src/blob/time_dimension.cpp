#include "quiver/time_dimension.h"

#include <ctime>
#include <cstdint>

namespace {
    int64_t day_of_week_from_datetime(std::time_t datetime) {

    }
} // anonymous namespace

namespace quiver {

struct TimeDimension::Impl {
    Frequency frequency;
    int64_t initial_value;
    std::unique_ptr<TimeDimension> parent_dimension;

};

} // namespace quiver