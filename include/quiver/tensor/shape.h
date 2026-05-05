#ifndef QUIVER_TENSOR_SHAPE_H
#define QUIVER_TENSOR_SHAPE_H

#include <cstddef>
#include <vector>

namespace quiver::tensor {

using shape_t = std::vector<std::size_t>;

inline shape_t compute_strides(const shape_t& shape) {
    shape_t strides(shape.size());
    std::size_t s = 1;
    for (std::size_t i = shape.size(); i > 0; --i) {
        strides[i - 1] = s;
        s *= shape[i - 1];
    }
    return strides;
}

inline std::size_t total_size(const shape_t& shape) {
    std::size_t n = 1;
    for (auto d : shape) n *= d;
    return n;
}

template <class... Idx>
inline std::size_t ravel_index(const shape_t& strides, Idx... idxs) noexcept {
    std::size_t result = 0;
    std::size_t i = 0;
    ((result += static_cast<std::size_t>(idxs) * strides[i++]), ...);
    return result;
}

}  // namespace quiver::tensor

#endif  // QUIVER_TENSOR_SHAPE_H
