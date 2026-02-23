#ifndef QUIVER_DIMENSION_H
#define QUIVER_DIMENSION_H

#include "export.h"
#include "time_properties.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace quiver {

struct Dimension {
    std::string name;
    int64_t size;
    std::optional<TimeProperties> time;

    bool is_time_dimension() const { return time.has_value(); }
};

}  // namespace quiver

#endif  // QUIVER_DIMENSION_H
