#ifndef QUIVER_TENSOR_TENSOR_H
#define QUIVER_TENSOR_TENSOR_H

#include "expression.h"
#include "shape.h"

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace quiver::tensor {

template <class T>
class Tensor : public Expression<Tensor<T>> {
public:
    explicit Tensor(shape_t shape)
        : shape_(std::move(shape)), strides_(compute_strides(shape_)) {
        data_.assign(total_size(shape_), T{});
    }

    Tensor(shape_t shape, T fill)
        : shape_(std::move(shape)), strides_(compute_strides(shape_)) {
        data_.assign(total_size(shape_), fill);
    }

    Tensor(shape_t shape, std::initializer_list<T> init)
        : shape_(std::move(shape)), strides_(compute_strides(shape_)) {
        const std::size_t expected = total_size(shape_);
        if (init.size() != expected) {
            throw std::runtime_error(
                "Cannot construct tensor: initializer_list size " +
                std::to_string(init.size()) +
                " does not match shape size " + std::to_string(expected));
        }
        data_.assign(init.begin(), init.end());
    }

    Tensor(const Tensor&) = default;
    Tensor& operator=(const Tensor&) = default;
    Tensor(Tensor&&) noexcept = default;
    Tensor& operator=(Tensor&&) noexcept = default;
    ~Tensor() = default;

    const shape_t& shape() const noexcept { return shape_; }
    std::size_t size() const noexcept { return data_.size(); }
    std::size_t rank() const noexcept { return shape_.size(); }

    const T* data() const noexcept { return data_.data(); }

    auto begin() noexcept { return data_.begin(); }
    auto end() noexcept { return data_.end(); }
    auto begin() const noexcept { return data_.begin(); }
    auto end() const noexcept { return data_.end(); }

    // Q2 RESOLVED: mutable returns T&, const returns const T& (matches std::vector::operator[]).
    template <class... Idx>
    T& operator()(Idx... idxs) noexcept {
        return data_[ravel_index(strides_, idxs...)];
    }
    template <class... Idx>
    const T& operator()(Idx... idxs) const noexcept {
        return data_[ravel_index(strides_, idxs...)];
    }

    template <class... Idx>
    T& at(Idx... idxs) {
        check_rank_and_bounds(idxs...);
        return data_[ravel_index(strides_, idxs...)];
    }
    template <class... Idx>
    const T& at(Idx... idxs) const {
        check_rank_and_bounds(idxs...);
        return data_[ravel_index(strides_, idxs...)];
    }

    T item() const {
        if (data_.size() != 1) {
            throw std::runtime_error(
                "Cannot item: tensor has " + std::to_string(data_.size()) +
                " elements (expected 1)");
        }
        return data_[0];
    }

    // Returns T by value (intentional contract divergence from operator()): allows
    // Phase 2+ intermediate expression nodes to compute and return temporaries.
    T eval(std::size_t i) const noexcept { return data_[i]; }

private:
    template <class... Idx>
    void check_rank_and_bounds(Idx... idxs) const {
        constexpr std::size_t n = sizeof...(idxs);
        if (n != shape_.size()) {
            throw std::runtime_error(
                "Cannot access tensor: rank mismatch (expected " +
                std::to_string(shape_.size()) + " indices, got " +
                std::to_string(n) + ")");
        }
        std::size_t dim = 0;
        bool oob = false;
        ((oob = oob || (static_cast<std::size_t>(idxs) >= shape_[dim++])), ...);
        if (oob) {
            std::string idx_str = "(";
            std::size_t k = 0;
            ((idx_str += (k++ > 0 ? "," : "") + std::to_string(static_cast<std::size_t>(idxs))), ...);
            idx_str += ")";
            std::string shape_str = "(";
            for (std::size_t j = 0; j < shape_.size(); ++j) {
                if (j > 0) shape_str += ",";
                shape_str += std::to_string(shape_[j]);
            }
            shape_str += ")";
            throw std::runtime_error(
                "Cannot access tensor: index " + idx_str +
                " out of bounds for shape " + shape_str);
        }
    }

    std::vector<T> data_;
    shape_t shape_;
    shape_t strides_;
};

}  // namespace quiver::tensor

#endif  // QUIVER_TENSOR_TENSOR_H
