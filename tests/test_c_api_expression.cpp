#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <quiver/c/binary/binary_file.h>
#include <quiver/c/binary/binary_metadata.h>
#include <quiver/c/common.h>
#include <quiver/c/element.h>
#include <quiver/c/expression/expression.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Fixture
// ============================================================================

class ExpressionCApiFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path_a = (fs::temp_directory_path() / "quiver_c_expr_a").string();
        path_b = (fs::temp_directory_path() / "quiver_c_expr_b").string();
        path_c = (fs::temp_directory_path() / "quiver_c_expr_c").string();
        path_out = (fs::temp_directory_path() / "quiver_c_expr_out").string();
        cleanup();
    }

    void TearDown() override { cleanup(); }

    void cleanup() {
        for (const auto& p : {path_a, path_b, path_c, path_out}) {
            for (auto ext : {".qvr", ".toml"}) {
                auto full = p + ext;
                if (fs::exists(full))
                    fs::remove(full);
            }
        }
    }

    std::string path_a, path_b, path_c, path_out;

    // Default 3 x 2 metadata, MW, labels {val1,val2}.
    static quiver_binary_metadata_t* make_simple_metadata() { return make_metadata(3, 2, "MW", {"val1", "val2"}); }

    static quiver_binary_metadata_t*
    make_metadata(int64_t rows, int64_t cols, const char* unit, const std::vector<const char*>& labels) {
        quiver_element_t* el = nullptr;
        quiver_element_create(&el);
        quiver_element_set_string(el, "version", "1");
        quiver_element_set_string(el, "initial_datetime", "2025-01-01T00:00:00");
        quiver_element_set_string(el, "unit", unit);
        const char* dims[] = {"row", "col"};
        quiver_element_set_array_string(el, "dimensions", dims, 2);
        int64_t sizes[] = {rows, cols};
        quiver_element_set_array_integer(el, "dimension_sizes", sizes, 2);
        quiver_element_set_array_string(el, "labels", labels.data(), static_cast<int32_t>(labels.size()));

        quiver_binary_metadata_t* md = nullptr;
        quiver_binary_metadata_from_element(el, &md);
        quiver_element_destroy(el);
        return md;
    }

    // Writes a 3 x 2 fixture file, two labels per cell.
    // fill(row, col, label_idx) returns the cell value.
    static void write_fixture(const std::string& path, std::function<double(int, int, int)> fill) {
        auto* md = make_simple_metadata();
        write_fixture_with_metadata(path, md, fill);
        quiver_binary_metadata_free(md);
    }

    // Writes a fixture with caller-provided metadata. Caller still owns md.
    // Assumes a 2D dimensional schema named (row, col) with 2 labels per cell.
    static void write_fixture_with_metadata(const std::string& path,
                                            quiver_binary_metadata_t* md,
                                            std::function<double(int, int, int)> fill) {
        quiver_binary_file_t* f = nullptr;
        ASSERT_EQ(quiver_binary_file_open_write(path.c_str(), md, &f), QUIVER_OK);

        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double data[] = {fill(static_cast<int>(r), static_cast<int>(c), 0),
                                 fill(static_cast<int>(r), static_cast<int>(c), 1)};
                ASSERT_EQ(quiver_binary_file_write(f, dim_names, dim_values, 2, data, 2), QUIVER_OK);
            }
        }
        ASSERT_EQ(quiver_binary_file_close(f), QUIVER_OK);
    }

    // Reads all 12 cells (3 rows x 2 cols x 2 labels) into a flat vector,
    // ordered (r=1,c=1,k=0), (r=1,c=1,k=1), (r=1,c=2,k=0), ...
    static std::vector<double> read_all_cells(const std::string& path) {
        quiver_binary_file_t* f = nullptr;
        EXPECT_EQ(quiver_binary_file_open_read(path.c_str(), &f), QUIVER_OK);
        std::vector<double> out;
        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 3; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double* data = nullptr;
                size_t count = 0;
                EXPECT_EQ(quiver_binary_file_read(f, dim_names, dim_values, 2, /*allow_nulls=*/0, &data, &count),
                          QUIVER_OK);
                for (size_t i = 0; i < count; ++i)
                    out.push_back(data[i]);
                quiver_binary_file_free_float_array(data);
            }
        }
        quiver_binary_file_close(f);
        return out;
    }

    // Helper: build a from-file Expression. Caller owns the returned handle.
    static quiver_expression_t* expr_from_file(const std::string& path) {
        quiver_binary_file_t* f = nullptr;
        EXPECT_EQ(quiver_binary_file_open_read(path.c_str(), &f), QUIVER_OK);
        quiver_expression_t* e = nullptr;
        EXPECT_EQ(quiver_expression_from_file(f, &e), QUIVER_OK);
        quiver_binary_file_close(f);
        return e;
    }

    // Generic metadata builder. Pass empty time_dimensions for non-time data.
    static quiver_binary_metadata_t* make_metadata_v(const std::vector<const char*>& dim_names,
                                                     const std::vector<int64_t>& dim_sizes,
                                                     const std::vector<const char*>& labels,
                                                     const char* unit = "MW",
                                                     const char* initial_datetime = "2025-01-01T00:00:00",
                                                     const std::vector<const char*>& time_dimensions = {},
                                                     const std::vector<const char*>& frequencies = {}) {
        quiver_element_t* el = nullptr;
        quiver_element_create(&el);
        quiver_element_set_string(el, "version", "1");
        quiver_element_set_string(el, "initial_datetime", initial_datetime);
        quiver_element_set_string(el, "unit", unit);
        quiver_element_set_array_string(el, "dimensions", dim_names.data(), static_cast<int32_t>(dim_names.size()));
        quiver_element_set_array_integer(el,
                                         "dimension_sizes",
                                         dim_sizes.data(),
                                         static_cast<int32_t>(dim_sizes.size()));
        quiver_element_set_array_string(el, "labels", labels.data(), static_cast<int32_t>(labels.size()));
        if (!time_dimensions.empty()) {
            quiver_element_set_array_string(el,
                                            "time_dimensions",
                                            time_dimensions.data(),
                                            static_cast<int32_t>(time_dimensions.size()));
            quiver_element_set_array_string(el,
                                            "frequencies",
                                            frequencies.data(),
                                            static_cast<int32_t>(frequencies.size()));
        }

        quiver_binary_metadata_t* md = nullptr;
        quiver_binary_metadata_from_element(el, &md);
        quiver_element_destroy(el);
        return md;
    }

    // Writes a single cell at the given dim positions; rest of the file stays NaN.
    // Sufficient for throws-tests that only need a valid file structure to exist.
    static void write_one_cell(const std::string& path,
                               quiver_binary_metadata_t* md,
                               const std::vector<const char*>& dim_names,
                               const std::vector<int64_t>& dim_values,
                               const std::vector<double>& cell) {
        quiver_binary_file_t* f = nullptr;
        ASSERT_EQ(quiver_binary_file_open_write(path.c_str(), md, &f), QUIVER_OK);
        ASSERT_EQ(quiver_binary_file_write(f,
                                           dim_names.data(),
                                           dim_values.data(),
                                           dim_names.size(),
                                           cell.data(),
                                           cell.size()),
                  QUIVER_OK);
        ASSERT_EQ(quiver_binary_file_close(f), QUIVER_OK);
    }

    // Writes every cell with row-major iteration (rightmost dim varies fastest).
    // Caller must ensure no nested time dimensions (sizes are constant across the grid).
    static void write_dense(const std::string& path,
                            quiver_binary_metadata_t* md,
                            const std::vector<const char*>& dim_names,
                            const std::vector<int64_t>& dim_sizes,
                            int64_t label_count,
                            std::function<double(const std::vector<int64_t>&, size_t)> fill) {
        quiver_binary_file_t* f = nullptr;
        ASSERT_EQ(quiver_binary_file_open_write(path.c_str(), md, &f), QUIVER_OK);

        std::vector<int64_t> dims(dim_sizes.size(), 1);
        std::vector<double> row(static_cast<size_t>(label_count));
        while (true) {
            for (size_t k = 0; k < row.size(); ++k)
                row[k] = fill(dims, k);
            ASSERT_EQ(quiver_binary_file_write(f, dim_names.data(), dims.data(), dims.size(), row.data(), row.size()),
                      QUIVER_OK);

            int i = static_cast<int>(dims.size()) - 1;
            while (i >= 0) {
                dims[i]++;
                if (dims[i] <= dim_sizes[i])
                    break;
                dims[i] = 1;
                i--;
            }
            if (i < 0)
                break;
        }

        ASSERT_EQ(quiver_binary_file_close(f), QUIVER_OK);
    }

    // Reads one cell. Caller frees the returned vector through normal RAII.
    static std::vector<double> read_one_cell(const std::string& path,
                                             const std::vector<const char*>& dim_names,
                                             const std::vector<int64_t>& dim_values) {
        quiver_binary_file_t* f = nullptr;
        EXPECT_EQ(quiver_binary_file_open_read(path.c_str(), &f), QUIVER_OK);
        double* data = nullptr;
        size_t count = 0;
        EXPECT_EQ(quiver_binary_file_read(f,
                                          dim_names.data(),
                                          dim_values.data(),
                                          dim_names.size(),
                                          /*allow_nulls=*/1,
                                          &data,
                                          &count),
                  QUIVER_OK);
        std::vector<double> out(data, data + count);
        quiver_binary_file_free_float_array(data);
        quiver_binary_file_close(f);
        return out;
    }
};

// ============================================================================
// Identity / save round-trip
// ============================================================================

TEST_F(ExpressionCApiFixture, IdentityFile) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 100 + c * 10 + k); });

    auto* e = expr_from_file(path_a);
    ASSERT_NE(e, nullptr);
    ASSERT_EQ(quiver_expression_save(e, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(e);

    auto orig = read_all_cells(path_a);
    auto copy = read_all_cells(path_out);
    ASSERT_EQ(orig.size(), copy.size());
    for (size_t i = 0; i < orig.size(); ++i)
        EXPECT_DOUBLE_EQ(orig[i], copy[i]);
}

TEST_F(ExpressionCApiFixture, SaveOpenedTwiceProducesSameOutput) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r + c + k); });

    auto* e = expr_from_file(path_a);
    auto path_out2 = path_out + "_2";
    ASSERT_EQ(quiver_expression_save(e, path_out.c_str()), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(e, path_out2.c_str()), QUIVER_OK);
    quiver_expression_close(e);

    auto v1 = read_all_cells(path_out);
    auto v2 = read_all_cells(path_out2);
    ASSERT_EQ(v1.size(), v2.size());
    for (size_t i = 0; i < v1.size(); ++i)
        EXPECT_DOUBLE_EQ(v1[i], v2[i]);

    for (auto ext : {".qvr", ".toml"}) {
        auto full = path_out2 + ext;
        if (fs::exists(full))
            fs::remove(full);
    }
}

// ============================================================================
// Binary operators (file op file)
// ============================================================================

TEST_F(ExpressionCApiFixture, AddTwoFiles) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 10 + c + k); });
    write_fixture(path_b, [](int r, int c, int k) { return static_cast<double>(r * 100 + c * 5 + k * 2); });

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* sum = nullptr;
    ASSERT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_ADD, a, b, &sum), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(sum, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(b);
    quiver_expression_close(sum);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] + vb[i]);
}

TEST_F(ExpressionCApiFixture, SubtractTwoFiles) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 100 + c * 10 + k); });
    write_fixture(path_b, [](int r, int c, int k) { return static_cast<double>(r + c + k); });

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* diff = nullptr;
    ASSERT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_SUBTRACT, a, b, &diff), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(diff, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(b);
    quiver_expression_close(diff);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] - vb[i]);
}

TEST_F(ExpressionCApiFixture, MultiplyTwoFiles) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r + c + k + 1); });
    write_fixture(path_b, [](int r, int c, int k) { return static_cast<double>(r * c + k + 1); });

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* product = nullptr;
    ASSERT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_MULTIPLY, a, b, &product), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(product, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(b);
    quiver_expression_close(product);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] * vb[i]);
}

TEST_F(ExpressionCApiFixture, DivideTwoFiles) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 10 + c + k); });
    // Avoid zero divisor.
    write_fixture(path_b, [](int r, int c, int k) { return static_cast<double>(r + c + k + 1); });

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* quotient = nullptr;
    ASSERT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_DIVIDE, a, b, &quotient), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(quotient, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(b);
    quiver_expression_close(quotient);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] / vb[i]);
}

TEST_F(ExpressionCApiFixture, Chained) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r + c + k); });
    write_fixture(path_b, [](int r, int c, int k) { return static_cast<double>(r * 2 + c + k); });
    write_fixture(path_c, [](int r, int c, int k) { return static_cast<double>(r + c * 2 + k); });

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    auto* c = expr_from_file(path_c);

    quiver_expression_t* a_plus_b = nullptr;
    quiver_expression_t* sub_c = nullptr;
    quiver_expression_t* halved = nullptr;
    ASSERT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_ADD, a, b, &a_plus_b), QUIVER_OK);
    ASSERT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_SUBTRACT, a_plus_b, c, &sub_c), QUIVER_OK);
    ASSERT_EQ(quiver_expression_apply_scalar_right(QUIVER_EXPRESSION_OP_DIVIDE, sub_c, 2.0, &halved), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(halved, path_out.c_str()), QUIVER_OK);

    quiver_expression_close(a);
    quiver_expression_close(b);
    quiver_expression_close(c);
    quiver_expression_close(a_plus_b);
    quiver_expression_close(sub_c);
    quiver_expression_close(halved);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vc = read_all_cells(path_c);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], (va[i] + vb[i] - vc[i]) / 2.0);
}

// ============================================================================
// Scalar broadcast (scalar on right)
// ============================================================================

TEST_F(ExpressionCApiFixture, ScalarBroadcastAddRight) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 100 + c * 10 + k); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_right(QUIVER_EXPRESSION_OP_ADD, a, 2.0, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] + 2.0);
}

TEST_F(ExpressionCApiFixture, ScalarBroadcastSubtractRight) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 100 + c * 10 + k); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_right(QUIVER_EXPRESSION_OP_SUBTRACT, a, 5.0, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] - 5.0);
}

TEST_F(ExpressionCApiFixture, ScalarBroadcastMultiplyRight) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 10 + c + k); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_right(QUIVER_EXPRESSION_OP_MULTIPLY, a, 3.0, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] * 3.0);
}

TEST_F(ExpressionCApiFixture, ScalarBroadcastDivideRight) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 100 + c * 10 + k + 1); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_right(QUIVER_EXPRESSION_OP_DIVIDE, a, 4.0, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] / 4.0);
}

// ============================================================================
// Scalar broadcast (scalar on left)
// ============================================================================

TEST_F(ExpressionCApiFixture, ScalarBroadcastAddLeft) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r * 100 + c * 10 + k); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_left(QUIVER_EXPRESSION_OP_ADD, 7.0, a, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 7.0 + va[i]);
}

TEST_F(ExpressionCApiFixture, ScalarBroadcastSubtractLeft) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r + c + k); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_left(QUIVER_EXPRESSION_OP_SUBTRACT, 100.0, a, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 100.0 - va[i]);
}

TEST_F(ExpressionCApiFixture, ScalarBroadcastMultiplyLeft) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r + c + k + 1); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_left(QUIVER_EXPRESSION_OP_MULTIPLY, 5.0, a, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 5.0 * va[i]);
}

TEST_F(ExpressionCApiFixture, ScalarBroadcastDivideLeft) {
    write_fixture(path_a, [](int r, int c, int k) { return static_cast<double>(r + c + k + 1); });

    auto* a = expr_from_file(path_a);
    quiver_expression_t* result = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_left(QUIVER_EXPRESSION_OP_DIVIDE, 60.0, a, &result), QUIVER_OK);
    ASSERT_EQ(quiver_expression_save(result, path_out.c_str()), QUIVER_OK);
    quiver_expression_close(a);
    quiver_expression_close(result);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 60.0 / va[i]);
}

// ============================================================================
// Validation errors (eager at apply, not at save)
// ============================================================================

TEST_F(ExpressionCApiFixture, MismatchedShapesReturnsError) {
    auto* md_a = make_metadata(3, 2, "MW", {"val1", "val2"});
    auto* md_b = make_metadata(4, 2, "MW", {"val1", "val2"});  // rows differ
    write_fixture_with_metadata(path_a, md_a, [](int, int, int) { return 1.0; });
    quiver_binary_metadata_free(md_a);

    // Custom write for path_b because its row count is 4, not the default 3.
    {
        quiver_binary_file_t* f = nullptr;
        ASSERT_EQ(quiver_binary_file_open_write(path_b.c_str(), md_b, &f), QUIVER_OK);
        const char* dim_names[] = {"row", "col"};
        for (int64_t r = 1; r <= 4; ++r) {
            for (int64_t c = 1; c <= 2; ++c) {
                int64_t dim_values[] = {r, c};
                double data[] = {1.0, 1.0};
                quiver_binary_file_write(f, dim_names, dim_values, 2, data, 2);
            }
        }
        quiver_binary_file_close(f);
    }
    quiver_binary_metadata_free(md_b);

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* result = nullptr;
    EXPECT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_ADD, a, b, &result), QUIVER_ERROR);
    EXPECT_EQ(result, nullptr);

    quiver_expression_close(a);
    quiver_expression_close(b);
}

TEST_F(ExpressionCApiFixture, UnitMismatchReturnsError) {
    auto* md_a = make_metadata(3, 2, "MW", {"val1", "val2"});
    auto* md_b = make_metadata(3, 2, "GWh", {"val1", "val2"});
    write_fixture_with_metadata(path_a, md_a, [](int, int, int) { return 1.0; });
    write_fixture_with_metadata(path_b, md_b, [](int, int, int) { return 1.0; });
    quiver_binary_metadata_free(md_a);
    quiver_binary_metadata_free(md_b);

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* result = nullptr;
    EXPECT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_ADD, a, b, &result), QUIVER_ERROR);
    EXPECT_EQ(result, nullptr);

    quiver_expression_close(a);
    quiver_expression_close(b);
}

TEST_F(ExpressionCApiFixture, LabelMismatchReturnsError) {
    auto* md_a = make_metadata(3, 2, "MW", {"val1", "val2"});
    auto* md_b = make_metadata(3, 2, "MW", {"val1", "val3"});  // labels differ
    write_fixture_with_metadata(path_a, md_a, [](int, int, int) { return 1.0; });
    write_fixture_with_metadata(path_b, md_b, [](int, int, int) { return 1.0; });
    quiver_binary_metadata_free(md_a);
    quiver_binary_metadata_free(md_b);

    auto* a = expr_from_file(path_a);
    auto* b = expr_from_file(path_b);
    quiver_expression_t* result = nullptr;
    EXPECT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_ADD, a, b, &result), QUIVER_ERROR);

    quiver_expression_close(a);
    quiver_expression_close(b);
}

// ============================================================================
// Save collision
// ============================================================================

TEST_F(ExpressionCApiFixture, SelfSaveCollisionReturnsError) {
    write_fixture(path_a, [](int, int, int) { return 42.0; });

    auto qvr_path = path_a + ".qvr";
    auto size_before = fs::file_size(qvr_path);
    std::ifstream in_before(qvr_path, std::ios::binary);
    std::vector<char> head_before(64);
    in_before.read(head_before.data(), 64);
    in_before.close();

    auto* e = expr_from_file(path_a);
    EXPECT_EQ(quiver_expression_save(e, path_a.c_str()), QUIVER_ERROR);
    quiver_expression_close(e);

    auto size_after = fs::file_size(qvr_path);
    std::ifstream in_after(qvr_path, std::ios::binary);
    std::vector<char> head_after(64);
    in_after.read(head_after.data(), 64);

    EXPECT_EQ(size_before, size_after);
    EXPECT_EQ(head_before, head_after);
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST_F(ExpressionCApiFixture, CloseNullIsOk) {
    EXPECT_EQ(quiver_expression_close(nullptr), QUIVER_OK);
}

TEST_F(ExpressionCApiFixture, FromFileNullArguments) {
    quiver_expression_t* out = nullptr;
    EXPECT_EQ(quiver_expression_from_file(nullptr, &out), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: file");

    write_fixture(path_a, [](int, int, int) { return 1.0; });
    quiver_binary_file_t* f = nullptr;
    quiver_binary_file_open_read(path_a.c_str(), &f);
    EXPECT_EQ(quiver_expression_from_file(f, nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out");
    quiver_binary_file_close(f);
}

TEST_F(ExpressionCApiFixture, ApplyNullArguments) {
    quiver_expression_t* out = nullptr;
    EXPECT_EQ(quiver_expression_apply(QUIVER_EXPRESSION_OP_ADD, nullptr, nullptr, &out), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: lhs");
}

TEST_F(ExpressionCApiFixture, SaveNullArguments) {
    EXPECT_EQ(quiver_expression_save(nullptr, "out"), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: expression");
}

// ============================================================================
// Metadata access
// ============================================================================

TEST_F(ExpressionCApiFixture, GetMetadataReturnsExpectedFields) {
    write_fixture(path_a, [](int, int, int) { return 1.0; });
    auto* a = expr_from_file(path_a);

    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_expression_get_metadata(a, &md), QUIVER_OK);
    ASSERT_NE(md, nullptr);

    char* unit = nullptr;
    ASSERT_EQ(quiver_binary_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");
    quiver_binary_metadata_free_string(unit);

    quiver_binary_metadata_free(md);
    quiver_expression_close(a);
}

TEST_F(ExpressionCApiFixture, GetMetadataAfterApplyReturnsBroadcastMetadata) {
    write_fixture(path_a, [](int, int, int) { return 1.0; });
    auto* a = expr_from_file(path_a);

    quiver_expression_t* doubled = nullptr;
    ASSERT_EQ(quiver_expression_apply_scalar_right(QUIVER_EXPRESSION_OP_MULTIPLY, a, 2.0, &doubled), QUIVER_OK);

    quiver_binary_metadata_t* md = nullptr;
    ASSERT_EQ(quiver_expression_get_metadata(doubled, &md), QUIVER_OK);

    char* unit = nullptr;
    ASSERT_EQ(quiver_binary_metadata_get_unit(md, &unit), QUIVER_OK);
    EXPECT_STREQ(unit, "MW");  // unit broadcasts unchanged through scalar op
    quiver_binary_metadata_free_string(unit);

    quiver_binary_metadata_free(md);
    quiver_expression_close(a);
    quiver_expression_close(doubled);
}
