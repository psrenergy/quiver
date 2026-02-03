#ifndef QUIVER_DIMENSION_H
#define QUIVER_DIMENSION_H

#include "export.h"

#include <cstdint>
#include <memory>
#include <string>

namespace quiver {

class QUIVER_API Dimension {
    explicit Dimension(std::string name, int64_t size);
    ~Dimension();

    // Non-copyable
    Dimension(const Dimension&) = delete;
    Dimension& operator=(const Dimension&) = delete;

    // Movable
    Dimension(Dimension&& other) noexcept;
    Dimension& operator=(Dimension&& other) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver

#endif  // QUIVER_DIMENSION_H
