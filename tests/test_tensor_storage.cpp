#include <gtest/gtest.h>
#include <quiver/tensor/expression.h>
#include <quiver/tensor/shape.h>
#include <quiver/tensor/tensor.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace quiver::tensor;

// ============================================================================
// Element types (STG-01)
// ============================================================================

TEST(ElementTypes, AllFourTypesConstruct) {
    Tensor<float> tf({2, 3}, 1.0f);
    Tensor<double> td({2, 3}, 1.0);
    Tensor<int32_t> ti({2, 3}, int32_t{1});
    Tensor<int64_t> tl({2, 3}, int64_t{1});
    EXPECT_EQ(tf.size(), 6u);
    EXPECT_EQ(td.size(), 6u);
    EXPECT_EQ(ti.size(), 6u);
    EXPECT_EQ(tl.size(), 6u);
}

// ============================================================================
// Tensor construction (STG-01, STG-02, D-02, D-04, D-05)
// ============================================================================

TEST(TensorConstruction, ShapeOnlyZeroInitializes) {
    Tensor<double> t({2, 3, 4});
    EXPECT_EQ(t.size(), 24u);
    EXPECT_EQ(t.rank(), 3u);
    EXPECT_EQ(t.shape().size(), 3u);
    EXPECT_EQ(t.shape()[0], 2u);
    EXPECT_EQ(t.shape()[1], 3u);
    EXPECT_EQ(t.shape()[2], 4u);
    for (auto v : t) EXPECT_EQ(v, 0.0);
}

TEST(TensorConstruction, ShapeWithFill) {
    Tensor<int32_t> t({2, 3}, 42);
    EXPECT_EQ(t.size(), 6u);
    EXPECT_EQ(t.rank(), 2u);
    for (auto v : t) EXPECT_EQ(v, 42);
}

TEST(TensorConstruction, ShapeWithInitList) {
    Tensor<float> t({2, 3}, {1.f, 2.f, 3.f, 4.f, 5.f, 6.f});
    EXPECT_EQ(t.size(), 6u);
    EXPECT_EQ(t.at(0, 0), 1.f);
    EXPECT_EQ(t.at(0, 1), 2.f);
    EXPECT_EQ(t.at(0, 2), 3.f);
    EXPECT_EQ(t.at(1, 0), 4.f);
    EXPECT_EQ(t.at(1, 1), 5.f);
    EXPECT_EQ(t.at(1, 2), 6.f);
}

TEST(TensorConstruction, InitListMismatchThrowsPattern1) {
    try {
        Tensor<double> t({2, 3}, {1.0, 2.0});
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("Cannot construct tensor"), std::string::npos);
        EXPECT_NE(what.find("initializer_list size 2"), std::string::npos);
        EXPECT_NE(what.find("shape size 6"), std::string::npos);
    }
}

TEST(TensorConstruction, ZeroSizedDimensionValid) {
    Tensor<double> t({2, 0, 3});
    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.rank(), 3u);
    EXPECT_EQ(t.begin(), t.end());
}

// ============================================================================
// 0-D tensor representation (D-11, D-12, D-13)
// ============================================================================

TEST(ZeroDTensor, EmptyShapeHasOneElement) {
    Tensor<double> t({}, 42.0);
    EXPECT_EQ(t.size(), 1u);
    EXPECT_EQ(t.rank(), 0u);
    EXPECT_EQ(t(), 42.0);
    EXPECT_EQ(t.item(), 42.0);
}

TEST(ZeroDTensor, EmptyShapeWithInitList) {
    Tensor<double> t({}, {3.14});
    EXPECT_EQ(t.size(), 1u);
    EXPECT_EQ(t.rank(), 0u);
    EXPECT_EQ(t.item(), 3.14);
}

TEST(ZeroDTensor, ItemThrowsOnNonScalar) {
    Tensor<int> t({3}, 5);
    try {
        (void)t.item();
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("Cannot item"), std::string::npos);
        EXPECT_NE(what.find("3 elements"), std::string::npos);
    }
}

// ============================================================================
// Tensor indexing (STG-03, D-06, D-07)
// ============================================================================

TEST(TensorIndexing, OperatorParensReadsAndWrites) {
    Tensor<int> t({2, 3});
    t(0, 0) = 1; t(0, 1) = 2; t(0, 2) = 3;
    t(1, 0) = 4; t(1, 1) = 5; t(1, 2) = 6;
    EXPECT_EQ(t(0, 0), 1);
    EXPECT_EQ(t(0, 1), 2);
    EXPECT_EQ(t(0, 2), 3);
    EXPECT_EQ(t(1, 0), 4);
    EXPECT_EQ(t(1, 1), 5);
    EXPECT_EQ(t(1, 2), 6);
}

TEST(TensorIndexing, AtThrowsOnRankMismatch) {
    Tensor<int> t({2, 3});
    try {
        (void)t.at(0);
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("Cannot access tensor"), std::string::npos);
        EXPECT_NE(what.find("rank mismatch"), std::string::npos);
    }
}

TEST(TensorIndexing, AtThrowsOnBoundsViolation) {
    Tensor<int> t({2, 3});
    try {
        (void)t.at(2u, 0u);
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("Cannot access tensor"), std::string::npos);
        EXPECT_NE(what.find("out of bounds"), std::string::npos);
    }
}

TEST(TensorIndexing, AtPassesValidIndex) {
    Tensor<int> t({2, 3});
    t(1, 2) = 42;
    EXPECT_EQ(t.at(1u, 2u), 42);
}

// ============================================================================
// Tensor data() raw pointer (STG-04, D-10)
// ============================================================================

TEST(TensorData, DataPointerMatchesFirstElement) {
    Tensor<float> t({2, 3}, {1.f, 2.f, 3.f, 4.f, 5.f, 6.f});
    const float* p = t.data();
    EXPECT_EQ(p[0], 1.f);
    EXPECT_EQ(p[5], 6.f);
    // D-10: data() is const T* only -- compile-time enforced by return type
    static_assert(std::is_same_v<decltype(t.data()), const float*>);
}

// ============================================================================
// Tensor iteration (STG-05, D-09)
// ============================================================================

TEST(TensorIteration, RangeForCompatible) {
    Tensor<int> t({3}, {10, 20, 30});
    int sum = 0;
    for (auto v : t) sum += v;
    EXPECT_EQ(sum, 60);
}

TEST(TensorIteration, BeginEndForwardToVector) {
    Tensor<int> t({3}, {1, 2, 3});
    auto it = t.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(*it, 3);
    ++it;
    EXPECT_EQ(it, t.end());
}

// ============================================================================
// Move semantics (Anti-Pattern #6 prevention; Pitfall A)
// ============================================================================

TEST(TensorMove, MoveOpsAreNoexcept) {
    static_assert(std::is_nothrow_move_constructible_v<Tensor<double>>);
    static_assert(std::is_nothrow_move_assignable_v<Tensor<double>>);
    static_assert(std::is_nothrow_move_constructible_v<Tensor<float>>);
    static_assert(std::is_nothrow_move_constructible_v<Tensor<int32_t>>);
    static_assert(std::is_nothrow_move_constructible_v<Tensor<int64_t>>);
    SUCCEED();
}

TEST(TensorMove, CopyableValueType) {
    Tensor<double> a({2, 3}, 5.0);
    Tensor<double> b = a;
    EXPECT_EQ(b.size(), 6u);
    EXPECT_EQ(b.shape()[0], 2u);
    for (auto v : b) EXPECT_EQ(v, 5.0);
    a(0, 0) = 99.0;
    EXPECT_EQ(b(0, 0), 5.0);
}

// ============================================================================
// CRTP base verification (EXP-01)
// ============================================================================

TEST(ExpressionBase, TensorDerivesFromExpression) {
    static_assert(std::is_base_of_v<Expression<Tensor<double>>, Tensor<double>>);
    static_assert(std::is_base_of_v<Expression<Tensor<int32_t>>, Tensor<int32_t>>);
    SUCCEED();
}

TEST(ExpressionBase, EvalReturnsValue) {
    Tensor<double> t({3}, {1.0, 2.0, 3.0});
    EXPECT_EQ(t.eval(0), 1.0);
    EXPECT_EQ(t.eval(1), 2.0);
    EXPECT_EQ(t.eval(2), 3.0);
    static_assert(std::is_same_v<decltype(t.eval(0)), double>);
}

// ============================================================================
// IsTensorExpr concept (EXP-02, D-14; Pitfall #7 prevention)
// ============================================================================

TEST(IsTensorExprConcept, AcceptsTensor) {
    static_assert(IsTensorExpr<Tensor<double>>);
    static_assert(IsTensorExpr<Tensor<int32_t>>);
    static_assert(IsTensorExpr<Tensor<float>>);
    static_assert(IsTensorExpr<Tensor<int64_t>>);
    SUCCEED();
}

TEST(IsTensorExprConcept, RejectsNonExpression) {
    static_assert(!IsTensorExpr<int>);
    static_assert(!IsTensorExpr<std::vector<double>>);
    static_assert(!IsTensorExpr<double>);
    static_assert(!IsTensorExpr<float>);
    SUCCEED();
}

TEST(IsTensorExprConcept, StripsRefAndCV) {
    static_assert(IsTensorExpr<Tensor<double>&>);
    static_assert(IsTensorExpr<const Tensor<double>&>);
    static_assert(IsTensorExpr<Tensor<double>&&>);
    SUCCEED();
}

// ============================================================================
// Shape free functions (STG-02 indirect; covers Pitfall B and C from RESEARCH.md)
// ============================================================================

TEST(ShapeFreeFunctions, ComputeStridesRowMajor) {
    shape_t s = compute_strides(shape_t{2, 3, 4});
    ASSERT_EQ(s.size(), 3u);
    EXPECT_EQ(s[0], 12u);
    EXPECT_EQ(s[1], 4u);
    EXPECT_EQ(s[2], 1u);
}

TEST(ShapeFreeFunctions, TotalSizeEmptyShapeIsOne) {
    EXPECT_EQ(total_size(shape_t{}), 1u);
}

TEST(ShapeFreeFunctions, TotalSizeWithZeroDimIsZero) {
    EXPECT_EQ(total_size(shape_t{2, 0, 3}), 0u);
}

TEST(ShapeFreeFunctions, ComputeStridesEmptyShape) {
    shape_t s = compute_strides(shape_t{});
    EXPECT_TRUE(s.empty());
}

// ============================================================================
// Full namespace qualification (Pitfall #14 verification)
// ============================================================================

TEST(NamespaceHygiene, FullyQualifiedNamesWork) {
    quiver::tensor::Tensor<double> t({2, 3}, 1.0);
    EXPECT_EQ(t.size(), 6u);
    static_assert(quiver::tensor::IsTensorExpr<quiver::tensor::Tensor<double>>);
}
