#include "quiver/time_dimension.h"

#include <stdexcept>

namespace {
    int64_t day_of_week_from_datetime(std::time_t datetime) {

    }
} // anonymous namespace

namespace quiver {

struct TimeDimension::Impl {
    Frequency frequency;
    int64_t initial_value;
    TimeDimension* parent_dimension;

};

} // namespace quiver