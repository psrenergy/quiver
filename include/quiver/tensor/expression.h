#ifndef QUIVER_TENSOR_EXPRESSION_H
#define QUIVER_TENSOR_EXPRESSION_H

#include "shape.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace quiver::tensor {

template <class Derived>
class Expression {
public:
    Derived& self() noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

    std::size_t size() const { return self().size(); }
    const shape_t& shape() const { return self().shape(); }
    std::size_t rank() const { return shape().size(); }
    auto eval(std::size_t i) const { return self().eval(i); }

protected:
    Expression() = default;
};

namespace detail {

// Public derivation only (RESEARCH.md Open Question Q4 RESOLVED: NO private/protected
// CRTP support for v0.8.0). The probe relies on implicit-conversion-to-pointer-of-Base,
// which is rejected for non-public derivation.
template <template <class> class Base, class Derived>
struct is_base_of_template {
private:
    template <class U>
    static std::true_type test(const Base<U>*);
    static std::false_type test(...);
public:
    static constexpr bool value = decltype(test(std::declval<Derived*>()))::value;
};

template <template <class> class Base, class Derived>
inline constexpr bool is_base_of_template_v = is_base_of_template<Base, Derived>::value;

}  // namespace detail

template <class T>
concept IsTensorExpr = detail::is_base_of_template_v<Expression, std::remove_cvref_t<T>>;

}  // namespace quiver::tensor

#endif  // QUIVER_TENSOR_EXPRESSION_H
