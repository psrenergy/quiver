#ifndef QUIVER_TIME_DIMENSION_H
#define QUIVER_TIME_DIMENSION_H

#include "export.h"
#include "dimension.h"

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>

namespace quiver {

enum class Frequency { Yearly, Monthly, Weekly, Daily, Hourly };

QUIVER_API std::string frequency_to_string(Frequency frequency);
QUIVER_API Frequency frequency_from_string(const std::string& str);

class QUIVER_API TimeDimension : public Dimension {
    explicit TimeDimension(std::string name, int64_t size, Frequency frequency);
    ~TimeDimension();

    // Non-copyable
    TimeDimension(const TimeDimension&) = delete;
    TimeDimension& operator=(const TimeDimension&) = delete;

    // Movable
    TimeDimension(TimeDimension&& other) noexcept;
    TimeDimension& operator=(TimeDimension&& other) noexcept;

    // Setters
    void set_initial_value(int64_t initial_value);
    void set_parent_dimension(int64_t parent_index);

    int64_t datetime_to_int(std::time_t datetime) const;
    std::time_t int_to_datetime(int64_t value) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver

#endif  // QUIVER_TIME_DIMENSION_H
