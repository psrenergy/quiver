#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <quiver/binary/binary_file.h>
#include <quiver/binary/binary_metadata.h>
#include <quiver/binary/iteration.h>
#include <quiver/element.h>
#include <quiver/expression/expression.h>
#include <string>
#include <unordered_map>
#include <vector>

using namespace quiver;
namespace fs = std::filesystem;

class ExpressionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path_a = (fs::temp_directory_path() / "quiver_expr_a").string();
        path_b = (fs::temp_directory_path() / "quiver_expr_b").string();
        path_c = (fs::temp_directory_path() / "quiver_expr_c").string();
        path_out = (fs::temp_directory_path() / "quiver_expr_out").string();
        path_out2 = (fs::temp_directory_path() / "quiver_expr_out2").string();
        // Clean up any leftover artifacts from a previous run before each test.
        cleanup();
    }

    void TearDown() override { cleanup(); }

    void cleanup() {
        for (const auto& p : {path_a, path_b, path_c, path_out, path_out2}) {
            for (auto ext : {".qvr", ".toml"}) {
                auto full = p + ext;
                if (fs::exists(full))
                    fs::remove(full);
            }
        }
    }

    std::string path_a, path_b, path_c, path_out, path_out2;

    static BinaryMetadata make_simple_metadata() {
        return BinaryMetadata::from_element(Element()
                                                .set("version", "1")
                                                .set("initial_datetime", "2025-01-01T00:00:00")
                                                .set("unit", "MW")
                                                .set("dimensions", {"row", "col"})
                                                .set("dimension_sizes", {3, 2})
                                                .set("labels", {"val1", "val2"}));
    }

    static void write_qvr(const std::string& path,
                          const BinaryMetadata& meta,
                          std::function<double(const std::vector<int64_t>& dims, size_t label_idx)> fill) {
        auto writer = BinaryFile::open_file(path, 'w', meta);
        std::vector<int64_t> dims = first_dimensions(meta);
        std::vector<double> row(meta.labels.size());
        for (;;) {
            for (size_t k = 0; k < row.size(); ++k)
                row[k] = fill(dims, k);
            std::unordered_map<std::string, int64_t> dim_map;
            for (size_t i = 0; i < meta.dimensions.size(); ++i) {
                dim_map[meta.dimensions[i].name] = dims[i];
            }
            writer.write(row, dim_map);
            auto nxt = next_dimensions(meta, dims);
            if (!nxt)
                break;
            dims = std::move(*nxt);
        }
    }

    static std::vector<double> read_all_cells(const std::string& path) {
        auto reader = BinaryFile::open_file(path, 'r');
        const auto& meta = reader.get_metadata();
        std::vector<double> out;
        std::vector<int64_t> dims = first_dimensions(meta);
        for (;;) {
            std::unordered_map<std::string, int64_t> dim_map;
            for (size_t i = 0; i < meta.dimensions.size(); ++i) {
                dim_map[meta.dimensions[i].name] = dims[i];
            }
            auto cell = reader.read(dim_map, true);
            out.insert(out.end(), cell.begin(), cell.end());
            auto nxt = next_dimensions(meta, dims);
            if (!nxt)
                break;
            dims = std::move(*nxt);
        }
        return out;
    }
};

TEST_F(ExpressionFixture, IdentityFile) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });

    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e(a);
    e.save(path_out);

    auto orig = read_all_cells(path_a);
    auto copy = read_all_cells(path_out);
    ASSERT_EQ(orig.size(), copy.size());
    for (size_t i = 0; i < orig.size(); ++i)
        EXPECT_DOUBLE_EQ(orig[i], copy[i]);
}

TEST_F(ExpressionFixture, SaveProducesReadableFile) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e(a);
    e.save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    const auto& m = reopened.get_metadata();
    EXPECT_EQ(m.unit, "MW");
    EXPECT_EQ(m.dimensions.size(), 2u);
    EXPECT_EQ(m.dimensions[0].name, "row");
    EXPECT_EQ(m.dimensions[0].size, 3);
    EXPECT_EQ(m.dimensions[1].name, "col");
    EXPECT_EQ(m.dimensions[1].size, 2);
    EXPECT_EQ(m.labels.size(), 2u);
}

TEST_F(ExpressionFixture, SaveOpenedTwiceProducesSameOutput) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e(a);
    e.save(path_out);
    e.save(path_out2);

    auto v1 = read_all_cells(path_out);
    auto v2 = read_all_cells(path_out2);
    ASSERT_EQ(v1.size(), v2.size());
    for (size_t i = 0; i < v1.size(); ++i)
        EXPECT_DOUBLE_EQ(v1[i], v2[i]);
}

TEST_F(ExpressionFixture, SelfSaveCollisionThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 1000 + dims[1] * 10 + static_cast<int64_t>(k));
    });

    // Snapshot input file size + first 64 bytes BEFORE the throw.
    auto qvr_path = path_a + ".qvr";
    auto size_before = fs::file_size(qvr_path);
    std::ifstream in_before(qvr_path, std::ios::binary);
    std::vector<char> head_before(64);
    in_before.read(head_before.data(), 64);
    in_before.close();

    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e(a);
    EXPECT_THROW(e.save(path_a), std::runtime_error);

    // After the throw: size + first 64 bytes must be unchanged. The collision check
    // must reject BEFORE BinaryFile::open_file('w', ...) calls fill_file_with_nulls.
    auto size_after = fs::file_size(qvr_path);
    std::ifstream in_after(qvr_path, std::ios::binary);
    std::vector<char> head_after(64);
    in_after.read(head_after.data(), 64);

    EXPECT_EQ(size_before, size_after);
    EXPECT_EQ(head_before, head_after);
}

TEST_F(ExpressionFixture, SelfSaveCollisionThrowsWithCanonicalizedPath) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 42.0; });

    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e(a);

    // Build a non-canonical reference to the same file (path with redundant "./").
    // weakly_canonical normalizes both this and path_a to the same canonical form.
    auto alt = (fs::temp_directory_path() / "." / "quiver_expr_a").string();
    EXPECT_THROW(e.save(alt), std::runtime_error);
    EXPECT_TRUE(fs::exists(path_a + ".qvr"));  // input still exists, not wiped
}

TEST_F(ExpressionFixture, AddTwoFiles) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 5 + static_cast<int64_t>(k) * 2);
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) + Expression(b);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] + vb[i]);
}

TEST_F(ExpressionFixture, Chained) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 2 + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_c, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] * 2 + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    auto c = BinaryFile::open_file(path_c, 'r');
    Expression e = (Expression(a) + Expression(b) - Expression(c)) / 2.0;
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vc = read_all_cells(path_c);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i) {
        EXPECT_DOUBLE_EQ(vo[i], (va[i] + vb[i] - vc[i]) / 2.0);
    }
}

TEST_F(ExpressionFixture, ScalarBroadcastAddRight) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = Expression(a) + 2.0;
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] + 2.0);
}

TEST_F(ExpressionFixture, SamePathTwice) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + static_cast<int64_t>(k));
    });
    auto a1 = BinaryFile::open_file(path_a, 'r');
    auto a2 = BinaryFile::open_file(path_a, 'r');
    Expression e = Expression(a1) + Expression(a2);  // each ExpressionFile opens independently
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 2.0 * va[i]);
}

TEST_F(ExpressionFixture, MismatchedShapesThrows) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"val1", "val2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {4, 2})
                                                 .set("labels", {"val1", "val2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, UnitMismatchThrows) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"val1", "val2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "GWh")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"val1", "val2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, TimePropertiesMismatchThrows) {
    // Same dim "block" with same size on both sides, but lhs treats it as a (monthly)
    // time dim while rhs treats it as a regular non-time dim. The validator fires the
    // "is a time dimension on lhs but not on rhs" branch — a controlled mismatch
    // we can express via from_element without running into engine constraints on
    // mixed inner-time-dim frequencies (e.g., weekly inside monthly is unsupported).
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("time_dimensions", {"block"})
                                                 .set("frequencies", {"monthly"})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})  // no time_dimensions
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("labels", {"v1", "v2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, InitialDatetimeMismatchThrows) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"month", "block"})
                                                 .set("dimension_sizes", {4, 31})
                                                 .set("time_dimensions", {"month", "block"})
                                                 .set("frequencies", {"monthly", "daily"})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-02-01T00:00:00")  // ← differs
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"month", "block"})
                                                 .set("dimension_sizes", {4, 31})
                                                 .set("time_dimensions", {"month", "block"})
                                                 .set("frequencies", {"monthly", "daily"})
                                                 .set("labels", {"v1", "v2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, LabelMismatchThrows) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"v1", "v3"}));  // ← differs
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, MirrorTimeNonTimeMismatchAThrows) {
    // md_a: block as NON-time. md_b: block as monthly time. Symmetric reject.
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("time_dimensions", {"block"})
                                                 .set("frequencies", {"monthly"})
                                                 .set("labels", {"v1", "v2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, MirrorTimeNonTimeMismatchBThrows) {
    // md_a: block as monthly time. md_b: block as NON-time. Pre-existing direction.
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("time_dimensions", {"block"})
                                                 .set("frequencies", {"monthly"})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "block"})
                                                 .set("dimension_sizes", {3, 12})
                                                 .set("labels", {"v1", "v2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, ParentDimMatchByNameAcceptsCrossPosition) {
    // md_a: parent of `day` is `month` at index 0. md_b: parent of `day` is `month` at index 1.
    // Same parent NAME, different operand indices. Must accept.
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"month", "extra", "day"})
                                                 .set("dimension_sizes", {2, 3, 31})
                                                 .set("time_dimensions", {"month", "day"})
                                                 .set("frequencies", {"monthly", "daily"})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"extra", "month", "day"})
                                                 .set("dimension_sizes", {3, 2, 31})
                                                 .set("time_dimensions", {"month", "day"})
                                                 .set("frequencies", {"monthly", "daily"})
                                                 .set("labels", {"v1", "v2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_NO_THROW({
        Expression e = Expression(a) + Expression(b);
        e.save(path_out);
    });
    EXPECT_NO_THROW(BinaryFile::open_file(path_out, 'r'));
}

TEST_F(ExpressionFixture, ParentDimNameMismatchThrows) {
    // md_a: parent of `block` is `month` (idx 0). md_b: parent of `block` is `stage` (idx 0).
    // Different parent NAMES. Must throw because parent names differ even though
    // `block` exists on both. Both metadata use monthly+daily so the per-row write-time
    // validation succeeds — only the cross-operand parent-name check distinguishes them.
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"month", "block"})
                                                 .set("dimension_sizes", {2, 31})
                                                 .set("time_dimensions", {"month", "block"})
                                                 .set("frequencies", {"monthly", "daily"})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"stage", "block"})
                                                 .set("dimension_sizes", {2, 31})
                                                 .set("time_dimensions", {"stage", "block"})
                                                 .set("frequencies", {"monthly", "daily"})
                                                 .set("labels", {"v1", "v2"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = Expression(a) + Expression(b); }, std::runtime_error);
}

TEST_F(ExpressionFixture, SubtractTwoFiles) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k) + 50);
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) - Expression(b);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] - vb[i]);
}

TEST_F(ExpressionFixture, MultiplyTwoFiles) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k) + 1);  // [1..]
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 2 + static_cast<int64_t>(k) + 1);
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) * Expression(b);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] * vb[i]);
}

TEST_F(ExpressionFixture, DivideTwoFiles) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k) + 100);
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k) + 1);  // never zero
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) / Expression(b);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] / vb[i]);
}

TEST_F(ExpressionFixture, ScalarBroadcastAddLeft) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = 2.0 + Expression(a);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 2.0 + va[i]);
}

TEST_F(ExpressionFixture, ScalarBroadcastSubtractRight) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k) + 50);
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = Expression(a) - 5.0;
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] - 5.0);
}

TEST_F(ExpressionFixture, ScalarBroadcastSubtractLeft) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = 100.0 - Expression(a);  // not commutative
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 100.0 - va[i]);
}

TEST_F(ExpressionFixture, ScalarBroadcastMultiplyRight) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = Expression(a) * 3.0;
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] * 3.0);
}

TEST_F(ExpressionFixture, ScalarBroadcastMultiplyLeft) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 5 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = 4.0 * Expression(a);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 4.0 * va[i]);
}

TEST_F(ExpressionFixture, ScalarBroadcastDivideRight) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k) + 100);
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = Expression(a) / 4.0;
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] / 4.0);
}

TEST_F(ExpressionFixture, ScalarBroadcastDivideLeft) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k) + 1);  // never zero
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = 100.0 / Expression(a);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], 100.0 / va[i]);
}

TEST_F(ExpressionFixture, BroadcastSizeOneDim) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"v1", "v2"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {1, 2})  // size-1 broadcast on row
                                                 .set("labels", {"v1", "v2"}));
    // a: a[r,c,k] = r*100 + c*10 + k
    write_qvr(path_a, md_a, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });
    // b: single row r=1; b[1,c,k] = c*1000 + k
    write_qvr(path_b, md_b, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[1] * 1000 + static_cast<int64_t>(k));
    });

    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) + Expression(b);
    e.save(path_out);

    // Output shape: row=3 (max of 3 and 1), col=2. Each row of output =
    // a[row, col, k] + b[1, col, k].
    auto reopened = BinaryFile::open_file(path_out, 'r');
    EXPECT_EQ(reopened.get_metadata().dimensions[0].size, 3);
    EXPECT_EQ(reopened.get_metadata().dimensions[1].size, 2);

    // Spot-check 3 cells: (1,1,0), (2,1,0), (3,2,1)
    auto cell_111 = reopened.read({{"row", 1}, {"col", 1}}, true);
    auto cell_211 = reopened.read({{"row", 2}, {"col", 1}}, true);
    auto cell_322 = reopened.read({{"row", 3}, {"col", 2}}, true);
    // a[r,c,k] + b[1,c,k]
    EXPECT_DOUBLE_EQ(cell_111[0], (1.0 * 100 + 1.0 * 10 + 0) + (1.0 * 1000 + 0));
    EXPECT_DOUBLE_EQ(cell_211[0], (2.0 * 100 + 1.0 * 10 + 0) + (1.0 * 1000 + 0));
    EXPECT_DOUBLE_EQ(cell_322[1], (3.0 * 100 + 2.0 * 10 + 1) + (2.0 * 1000 + 1));
}

TEST_F(ExpressionFixture, BroadcastLabelsAxis) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {2, 2})
                                                 .set("labels", {"single"}));  // 1 label
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {2, 2})
                                                 .set("labels", {"l1", "l2", "l3"}));  // 3 labels
    // a: a[r,c,0] = r*10 + c (only one label)
    write_qvr(path_a, md_a, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return static_cast<double>(dims[0] * 10 + dims[1]);
    });
    // b: b[r,c,k] = r*100 + c*10 + k (3 labels: k=0,1,2)
    write_qvr(path_b, md_b, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });

    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) + Expression(b);
    e.save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    const auto& m = reopened.get_metadata();
    ASSERT_EQ(m.labels.size(), 3u);
    EXPECT_EQ(m.labels[0], "l1");
    EXPECT_EQ(m.labels[1], "l2");
    EXPECT_EQ(m.labels[2], "l3");

    // Spot-check (1,1) and (2,2)
    auto cell_11 = reopened.read({{"row", 1}, {"col", 1}}, true);
    auto cell_22 = reopened.read({{"row", 2}, {"col", 2}}, true);
    // out[r,c,k] = a[r,c,0] + b[r,c,k]
    EXPECT_DOUBLE_EQ(cell_11[0], (1.0 * 10 + 1.0) + (1.0 * 100 + 1.0 * 10 + 0));
    EXPECT_DOUBLE_EQ(cell_11[1], (1.0 * 10 + 1.0) + (1.0 * 100 + 1.0 * 10 + 1));
    EXPECT_DOUBLE_EQ(cell_11[2], (1.0 * 10 + 1.0) + (1.0 * 100 + 1.0 * 10 + 2));
    EXPECT_DOUBLE_EQ(cell_22[2], (2.0 * 10 + 2.0) + (2.0 * 100 + 2.0 * 10 + 2));
}

TEST_F(ExpressionFixture, UnionDimsAcrossOperands) {
    // lhs: [scenario=2, time=4 monthly]
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "time"})
                                                 .set("dimension_sizes", {2, 4})
                                                 .set("time_dimensions", {"time"})
                                                 .set("frequencies", {"monthly"})
                                                 .set("labels", {"v1"}));
    // rhs: [time=4 monthly, stage=3]
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"time", "stage"})
                                                 .set("dimension_sizes", {4, 3})
                                                 .set("time_dimensions", {"time"})
                                                 .set("frequencies", {"monthly"})
                                                 .set("labels", {"v1"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        // a[scenario, time] = scenario*10 + time
        return static_cast<double>(dims[0] * 10 + dims[1]);
    });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        // b[time, stage] = time*100 + stage
        return static_cast<double>(dims[0] * 100 + dims[1]);
    });

    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) + Expression(b);
    e.save(path_out);

    // Output shape: lhs.dims (in lhs order) ++ rhs-only dims:
    //   [scenario=2, time=4, stage=3]
    auto reopened = BinaryFile::open_file(path_out, 'r');
    const auto& m = reopened.get_metadata();
    ASSERT_EQ(m.dimensions.size(), 3u);
    EXPECT_EQ(m.dimensions[0].name, "scenario");
    EXPECT_EQ(m.dimensions[0].size, 2);
    EXPECT_EQ(m.dimensions[1].name, "time");
    EXPECT_EQ(m.dimensions[1].size, 4);
    EXPECT_EQ(m.dimensions[2].name, "stage");
    EXPECT_EQ(m.dimensions[2].size, 3);

    // Verify two sample cells. Output dims are [scenario, time, stage].
    // out[s, t, st] = a[s, t] + b[t, st] = (s*10 + t) + (t*100 + st).
    auto cell_111 = reopened.read({{"scenario", 1}, {"time", 1}, {"stage", 1}}, true);
    auto cell_242 = reopened.read({{"scenario", 2}, {"time", 4}, {"stage", 2}}, true);
    EXPECT_DOUBLE_EQ(cell_111[0], (1.0 * 10 + 1.0) + (1.0 * 100 + 1.0));
    EXPECT_DOUBLE_EQ(cell_242[0], (2.0 * 10 + 4.0) + (4.0 * 100 + 2.0));
}

TEST_F(ExpressionFixture, OperandDimsInDifferentOrder) {
    // Operand dim ordering may differ from output ordering. Each output
    // dim index must map back to the corresponding operand dim by NAME.
    // Same dim set, swapped order: lhs has [scenario, time], rhs has
    // [time, scenario]. A regression in the search-by-name logic would
    // scramble values without any other test catching it
    // (UnionDimsAcrossOperands has partially-overlapping sets, not
    // same-set-swapped order).
    auto md_a = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"scenario", "time"})
                                                 .set("dimension_sizes", {2, 4})
                                                 .set("labels", {"v1"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"time", "scenario"})  // swapped order
                                                 .set("dimension_sizes", {4, 2})
                                                 .set("labels", {"v1"}));
    // a[scenario, time] = scenario*10 + time
    write_qvr(path_a, md_a, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return static_cast<double>(dims[0] * 10 + dims[1]);
    });
    // b[time, scenario] = time*100 + scenario
    write_qvr(path_b, md_b, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return static_cast<double>(dims[0] * 100 + dims[1]);
    });

    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = Expression(a) + Expression(b);
    e.save(path_out);

    // Output dims follow lhs order (same set ⇒ no rhs-only
    // dims): [scenario=2, time=4].
    auto reopened = BinaryFile::open_file(path_out, 'r');
    const auto& m = reopened.get_metadata();
    ASSERT_EQ(m.dimensions.size(), 2u);
    EXPECT_EQ(m.dimensions[0].name, "scenario");
    EXPECT_EQ(m.dimensions[0].size, 2);
    EXPECT_EQ(m.dimensions[1].name, "time");
    EXPECT_EQ(m.dimensions[1].size, 4);

    // Verify 4 sample positions, covering corners and an interior cell:
    // (s=1, t=1), (s=2, t=4), (s=1, t=4), (s=2, t=2).
    // out[s, t] = a[s, t] + b[t, s] = (s*10 + t) + (t*100 + s).
    struct Sample {
        int64_t s;
        int64_t t;
    };
    for (auto sample : {Sample{1, 1}, Sample{2, 4}, Sample{1, 4}, Sample{2, 2}}) {
        auto cell = reopened.read({{"scenario", sample.s}, {"time", sample.t}}, true);
        double expected =
            static_cast<double>(sample.s * 10 + sample.t) + static_cast<double>(sample.t * 100 + sample.s);
        EXPECT_DOUBLE_EQ(cell[0], expected) << "Mismatch at (s=" << sample.s << ", t=" << sample.t << ")";
    }
}

TEST_F(ExpressionFixture, LargeGridCompletes) {
    // 50x20 grid with 4 labels = 16,000 doubles per file. Smaller than the
    // research-suggested 100x100x5 to keep CI runtimes friendly; still large
    // enough that any per-row allocation regression would manifest.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"row", "col"})
                                               .set("dimension_sizes", {50, 20})
                                               .set("labels", {"l1", "l2", "l3", "l4"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 1000 + dims[1] * 10 + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] * 100 + static_cast<int64_t>(k) * 7);
    });

    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');

    const auto t0 = std::chrono::steady_clock::now();
    Expression e = (Expression(a) + Expression(b)) * 2.0;
    e.save(path_out);
    const auto t1 = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // Generous 5-second budget; the test is a backstop for severe regressions
    // (e.g. accidental per-row heap allocations).
    EXPECT_LT(elapsed, 5000) << "Save took " << elapsed << " ms (budget 5000 ms)";

    // Spot-check a handful of cells for value correctness.
    auto reopened = BinaryFile::open_file(path_out, 'r');
    for (auto rc : {std::pair<int64_t, int64_t>{1, 1}, {25, 10}, {50, 20}}) {
        auto cell = reopened.read({{"row", rc.first}, {"col", rc.second}}, true);
        for (size_t k = 0; k < cell.size(); ++k) {
            double a_val = static_cast<double>(rc.first * 1000 + rc.second * 10 + static_cast<int64_t>(k));
            double b_val = static_cast<double>(rc.first + rc.second * 100 + static_cast<int64_t>(k) * 7);
            EXPECT_DOUBLE_EQ(cell[k], (a_val + b_val) * 2.0);
        }
    }
}

TEST_F(ExpressionFixture, SaveFailsWhenInputIsOpenForWriting) {
    auto md = make_simple_metadata();
    auto writer = BinaryFile::open_file(path_a, 'w', md);

    // Expression construction is fine — it only loads metadata from the .toml.
    // save() is where input files get opened for reading, which collides with the
    // active writer through BinaryFile's write_registry.
    Expression e(writer);
    EXPECT_THROW(
        {
            try {
                e.save(path_out);
            } catch (const std::runtime_error& err) {
                EXPECT_NE(std::string(err.what()).find("Cannot open_file: file is already open for writing"),
                          std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

TEST_F(ExpressionFixture, ImplicitConversionFromBinaryFile) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 5 + static_cast<int64_t>(k) * 2);
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');

    Expression e = a + b;
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], va[i] + vb[i]);
}

// =============================================================================
// ExpressionAggregate — dimension aggregation
// =============================================================================

TEST_F(ExpressionFixture, AggregateSumOverNonTimeDim) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = Expression(a).aggregate("row", ExpressionAggregate::Operation::Sum);
    e.save(path_out);

    // Output has only "col" dim (size 2) and labels [val1, val2] = 4 cells.
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);

    // For each (col, k): sum over row=1..3 of (row*10 + col + k).
    auto expected = [](int64_t col, size_t k) {
        double s = 0.0;
        for (int64_t r = 1; r <= 3; ++r) {
            s += static_cast<double>(r * 10 + col + static_cast<int64_t>(k));
        }
        return s;
    };
    EXPECT_DOUBLE_EQ(vo[0], expected(1, 0));
    EXPECT_DOUBLE_EQ(vo[1], expected(1, 1));
    EXPECT_DOUBLE_EQ(vo[2], expected(2, 0));
    EXPECT_DOUBLE_EQ(vo[3], expected(2, 1));
}

TEST_F(ExpressionFixture, AggregateMeanOverNonTimeDim) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("row", ExpressionAggregate::Operation::Mean).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);

    // Mean over row=1..3 of (10r + col + k) = (60 + 3*col + 3*k) / 3 = 20 + col + k.
    EXPECT_DOUBLE_EQ(vo[0], 21.0);  // col=1, k=0
    EXPECT_DOUBLE_EQ(vo[1], 22.0);  // col=1, k=1
    EXPECT_DOUBLE_EQ(vo[2], 22.0);  // col=2, k=0
    EXPECT_DOUBLE_EQ(vo[3], 23.0);  // col=2, k=1
}

TEST_F(ExpressionFixture, AggregateMinOverNonTimeDim) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("row", ExpressionAggregate::Operation::Min).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);
    // Min over row=1..3: row=1 wins. Value = 10 + col + k.
    EXPECT_DOUBLE_EQ(vo[0], 11.0);  // col=1, k=0
    EXPECT_DOUBLE_EQ(vo[1], 12.0);
    EXPECT_DOUBLE_EQ(vo[2], 12.0);
    EXPECT_DOUBLE_EQ(vo[3], 13.0);
}

TEST_F(ExpressionFixture, AggregateMaxOverNonTimeDim) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("row", ExpressionAggregate::Operation::Max).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);
    // Max over row=1..3: row=3 wins. Value = 30 + col + k.
    EXPECT_DOUBLE_EQ(vo[0], 31.0);
    EXPECT_DOUBLE_EQ(vo[1], 32.0);
    EXPECT_DOUBLE_EQ(vo[2], 32.0);
    EXPECT_DOUBLE_EQ(vo[3], 33.0);
}

TEST_F(ExpressionFixture, AggregatePercentileOverNonTimeDim) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("row", ExpressionAggregate::Operation::Percentile, 0.5).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);
    // Median over row=1..3 of (10r + col + k). Sorted = {col+k+10, col+k+20, col+k+30}.
    // (n-1)*0.5 = 1.0 → middle element = col + k + 20.
    EXPECT_DOUBLE_EQ(vo[0], 21.0);
    EXPECT_DOUBLE_EQ(vo[1], 22.0);
    EXPECT_DOUBLE_EQ(vo[2], 22.0);
    EXPECT_DOUBLE_EQ(vo[3], 23.0);
}

TEST_F(ExpressionFixture, AggregateSumOverTimeDimSimple) {
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"year", "scenario"})
                                               .set("dimension_sizes", {3, 2})
                                               .set("time_dimensions", {"year"})
                                               .set("frequencies", {"yearly"})
                                               .set("labels", {"v1"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t) { return static_cast<double>(dims[0]); });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("year", ExpressionAggregate::Operation::Sum).save(path_out);

    auto vo = read_all_cells(path_out);
    // Output: [scenario(2)] × [v1] = 2 cells. Sum over year=1..3 = 6.
    ASSERT_EQ(vo.size(), 2u);
    EXPECT_DOUBLE_EQ(vo[0], 6.0);
    EXPECT_DOUBLE_EQ(vo[1], 6.0);
}

TEST_F(ExpressionFixture, AggregateSumOverTimeDimVariable) {
    // Reduce "block" (day, parent=month) so the iteration must respect 28-day Feb,
    // 31-day Jan/Mar, 30-day Apr.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"month", "block"})
                                               .set("dimension_sizes", {4, 31})
                                               .set("time_dimensions", {"month", "block"})
                                               .set("frequencies", {"monthly", "daily"})
                                               .set("labels", {"v1"}));
    // Fill every cell with 1.0 → sum over block at each month equals the number of days.
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("block", ExpressionAggregate::Operation::Sum).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);       // 4 months × 1 label
    EXPECT_DOUBLE_EQ(vo[0], 31.0);  // Jan
    EXPECT_DOUBLE_EQ(vo[1], 28.0);  // Feb 2025 (non-leap)
    EXPECT_DOUBLE_EQ(vo[2], 31.0);  // Mar
    EXPECT_DOUBLE_EQ(vo[3], 30.0);  // Apr
}

TEST_F(ExpressionFixture, AggregateSumSkipsNaNs) {
    auto md = make_simple_metadata();
    const double kNan = std::numeric_limits<double>::quiet_NaN();
    write_qvr(path_a, md, [kNan](const std::vector<int64_t>& dims, size_t k) {
        // Mark row=2 as NaN. Sum across row should skip it.
        if (dims[0] == 2)
            return kNan;
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("row", ExpressionAggregate::Operation::Sum).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);
    // sum = row1 + row3 (row2 skipped) = (10+col+k) + (30+col+k) = 40 + 2*col + 2*k
    EXPECT_DOUBLE_EQ(vo[0], 42.0);
    EXPECT_DOUBLE_EQ(vo[1], 44.0);
    EXPECT_DOUBLE_EQ(vo[2], 44.0);
    EXPECT_DOUBLE_EQ(vo[3], 46.0);
}

TEST_F(ExpressionFixture, AggregateAllNaNRangeProducesNaN) {
    auto md = make_simple_metadata();
    const double kNan = std::numeric_limits<double>::quiet_NaN();
    write_qvr(path_a, md, [kNan](const std::vector<int64_t>&, size_t) { return kNan; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate("row", ExpressionAggregate::Operation::Sum).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);
    for (double v : vo) {
        EXPECT_TRUE(std::isnan(v));
    }
}

TEST_F(ExpressionFixture, AggregateTimeDimRewireParents) {
    // dims=[scenario(3), month(12), block(28)] — reducing scenario (non-time, index 0)
    // shifts month → output index 0, block → output index 1. block's parent in input
    // is 1 (month), should remap to 0 in output.
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-02-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"scenario", "month", "block"})
                                               .set("dimension_sizes", {3, 1, 28})
                                               .set("time_dimensions", {"month", "block"})
                                               .set("frequencies", {"monthly", "daily"})
                                               .set("labels", {"v1"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate("scenario", ExpressionAggregate::Operation::Sum);
    const auto& m = out.metadata();

    ASSERT_EQ(m.dimensions.size(), 2u);
    EXPECT_EQ(m.dimensions[0].name, "month");
    EXPECT_EQ(m.dimensions[1].name, "block");
    ASSERT_TRUE(m.dimensions[1].is_time_dimension());
    EXPECT_EQ(m.dimensions[1].time->parent_dimension_index, 0);  // block points at month
}

TEST_F(ExpressionFixture, AggregateReduceOutermostTimeDimWithChildren) {
    // dims=[year(2), month(12)] both time. Reduce year (outermost). Month's parent
    // was 0 (year). After reduction, month becomes orphan (parent=-1).
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"year", "month"})
                                               .set("dimension_sizes", {2, 12})
                                               .set("time_dimensions", {"year", "month"})
                                               .set("frequencies", {"yearly", "monthly"})
                                               .set("labels", {"v1"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate("year", ExpressionAggregate::Operation::Sum);
    const auto& m = out.metadata();

    ASSERT_EQ(m.dimensions.size(), 1u);
    EXPECT_EQ(m.dimensions[0].name, "month");
    ASSERT_TRUE(m.dimensions[0].is_time_dimension());
    EXPECT_EQ(m.dimensions[0].time->parent_dimension_index, -1);
    EXPECT_EQ(m.number_of_time_dimensions(), 1);
}

TEST_F(ExpressionFixture, AggregateDimensionNotFoundThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).aggregate("nonexistent", ExpressionAggregate::Operation::Sum), std::runtime_error);
}

TEST_F(ExpressionFixture, AggregatePercentileMissingParamThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).aggregate("row", ExpressionAggregate::Operation::Percentile), std::runtime_error);
}

TEST_F(ExpressionFixture, AggregateSumExtraParamThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).aggregate("row", ExpressionAggregate::Operation::Sum, 0.5), std::runtime_error);
}

TEST_F(ExpressionFixture, AggregatePercentileOutOfRangeThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).aggregate("row", ExpressionAggregate::Operation::Percentile, 1.5), std::runtime_error);
    EXPECT_THROW(Expression(a).aggregate("row", ExpressionAggregate::Operation::Percentile, -0.1), std::runtime_error);
}

TEST_F(ExpressionFixture, AggregateChained) {
    // Start with 3 dims so chaining two reductions still leaves at least one dim
    // (BinaryMetadata::validate() requires dimensions.size() >= 1).
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"row", "col", "depth"})
                                               .set("dimension_sizes", {3, 2, 2})
                                               .set("labels", {"v1"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 2.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a)
        .aggregate("row", ExpressionAggregate::Operation::Sum)
        .aggregate("col", ExpressionAggregate::Operation::Sum)
        .save(path_out);

    // Output dims = [depth(2)] × 1 label = 2 cells. Each cell sums 3 rows × 2 cols of 2.0 = 12.0.
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 2u);
    EXPECT_DOUBLE_EQ(vo[0], 12.0);
    EXPECT_DOUBLE_EQ(vo[1], 12.0);
}

TEST_F(ExpressionFixture, AggregateComposedWithBinary) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    (Expression(a) + Expression(b)).aggregate("row", ExpressionAggregate::Operation::Sum).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 4u);
    // (a+b) at (r, c, k) = (r*10 + c + k) + (r + c + k) = 11r + 2c + 2k.
    // Sum over r=1..3 = 11*(1+2+3) + 3*(2c + 2k) = 66 + 6c + 6k.
    EXPECT_DOUBLE_EQ(vo[0], 66.0 + 6.0 * 1 + 0.0);  // c=1, k=0
    EXPECT_DOUBLE_EQ(vo[1], 66.0 + 6.0 * 1 + 6.0);  // c=1, k=1
    EXPECT_DOUBLE_EQ(vo[2], 66.0 + 6.0 * 2 + 0.0);  // c=2, k=0
    EXPECT_DOUBLE_EQ(vo[3], 66.0 + 6.0 * 2 + 6.0);  // c=2, k=1
}

// =============================================================================
// ExpressionAggregateAgents — label-axis (column) aggregation
// =============================================================================

TEST_F(ExpressionFixture, AgentSumReducesLabels) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Sum);
    out.save(path_out);

    const auto& m = out.metadata();
    ASSERT_EQ(m.labels.size(), 1u);
    EXPECT_EQ(m.labels[0], "sum");
    ASSERT_EQ(m.dimensions.size(), 2u);

    auto vo = read_all_cells(path_out);
    // 3 rows × 2 cols × 1 label = 6 cells. Each = (10r + c) + (10r + c + 1) = 20r + 2c + 1.
    ASSERT_EQ(vo.size(), 6u);
    auto expected = [](int64_t r, int64_t c) { return static_cast<double>(20 * r + 2 * c + 1); };
    EXPECT_DOUBLE_EQ(vo[0], expected(1, 1));
    EXPECT_DOUBLE_EQ(vo[1], expected(1, 2));
    EXPECT_DOUBLE_EQ(vo[2], expected(2, 1));
    EXPECT_DOUBLE_EQ(vo[3], expected(2, 2));
    EXPECT_DOUBLE_EQ(vo[4], expected(3, 1));
    EXPECT_DOUBLE_EQ(vo[5], expected(3, 2));
}

TEST_F(ExpressionFixture, AgentMeanReducesLabels) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Mean);
    out.save(path_out);

    const auto& m = out.metadata();
    EXPECT_EQ(m.labels[0], "mean");

    auto vo = read_all_cells(path_out);
    // mean = ((10r + c) + (10r + c + 1)) / 2 = 10r + c + 0.5
    EXPECT_DOUBLE_EQ(vo[0], 11.5);  // r=1, c=1
    EXPECT_DOUBLE_EQ(vo[1], 12.5);  // r=1, c=2
}

TEST_F(ExpressionFixture, AgentMinReducesLabels) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Min);
    out.save(path_out);

    EXPECT_EQ(out.metadata().labels[0], "min");
    auto vo = read_all_cells(path_out);
    // Min between (10r + c) and (10r + c + 1) = (10r + c).
    EXPECT_DOUBLE_EQ(vo[0], 11.0);
    EXPECT_DOUBLE_EQ(vo[1], 12.0);
}

TEST_F(ExpressionFixture, AgentMaxReducesLabels) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Max);
    out.save(path_out);

    EXPECT_EQ(out.metadata().labels[0], "max");
    auto vo = read_all_cells(path_out);
    // Max between (10r + c) and (10r + c + 1) = (10r + c + 1).
    EXPECT_DOUBLE_EQ(vo[0], 12.0);
    EXPECT_DOUBLE_EQ(vo[1], 13.0);
}

TEST_F(ExpressionFixture, AgentPercentileReducesLabels) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Percentile, 0.5);
    out.save(path_out);

    EXPECT_EQ(out.metadata().labels[0], "percentile");
    auto vo = read_all_cells(path_out);
    // Median of two values (a, a+1) = a + 0.5 = 10r + c + 0.5
    EXPECT_DOUBLE_EQ(vo[0], 11.5);
}

TEST_F(ExpressionFixture, AgentSkipsNaNs) {
    auto md = make_simple_metadata();
    const double kNan = std::numeric_limits<double>::quiet_NaN();
    write_qvr(path_a, md, [kNan](const std::vector<int64_t>& dims, size_t k) {
        if (k == 0)
            return kNan;
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Sum).save(path_out);

    auto vo = read_all_cells(path_out);
    // Only label k=1 contributes; sum = single value = 10r + c + 1.
    EXPECT_DOUBLE_EQ(vo[0], 12.0);  // r=1, c=1
    EXPECT_DOUBLE_EQ(vo[1], 13.0);  // r=1, c=2
}

TEST_F(ExpressionFixture, AgentAllNaNProducesNaN) {
    auto md = make_simple_metadata();
    const double kNan = std::numeric_limits<double>::quiet_NaN();
    write_qvr(path_a, md, [kNan](const std::vector<int64_t>&, size_t) { return kNan; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Sum).save(path_out);

    auto vo = read_all_cells(path_out);
    for (double v : vo) {
        EXPECT_TRUE(std::isnan(v));
    }
}

TEST_F(ExpressionFixture, AgentPreservesDimensions) {
    auto md = BinaryMetadata::from_element(Element()
                                               .set("version", "1")
                                               .set("initial_datetime", "2025-01-01T00:00:00")
                                               .set("unit", "MW")
                                               .set("dimensions", {"year", "scenario"})
                                               .set("dimension_sizes", {2, 3})
                                               .set("time_dimensions", {"year"})
                                               .set("frequencies", {"yearly"})
                                               .set("labels", {"v1", "v2", "v3"}));
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Mean);
    const auto& m = out.metadata();

    EXPECT_EQ(m.unit, "MW");
    ASSERT_EQ(m.dimensions.size(), 2u);
    EXPECT_EQ(m.dimensions[0].name, "year");
    EXPECT_EQ(m.dimensions[0].size, 2);
    EXPECT_TRUE(m.dimensions[0].is_time_dimension());
    EXPECT_EQ(m.dimensions[1].name, "scenario");
    EXPECT_EQ(m.dimensions[1].size, 3);
    EXPECT_EQ(m.number_of_time_dimensions(), 1);
    EXPECT_EQ(m.labels.size(), 1u);
    EXPECT_EQ(m.labels[0], "mean");
}

TEST_F(ExpressionFixture, AgentPercentileMissingParamThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Percentile), std::runtime_error);
}

TEST_F(ExpressionFixture, AgentPercentileOutOfRangeThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Percentile, 1.5),
                 std::runtime_error);
    EXPECT_THROW(Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Percentile, -0.1),
                 std::runtime_error);
}

TEST_F(ExpressionFixture, AgentChainedAfterAggregate) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 3.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a)
        .aggregate("row", ExpressionAggregate::Operation::Sum)
        .aggregate_agents(ExpressionAggregateAgents::Operation::Mean)
        .save(path_out);

    // After reducing row(3) and agents(2): output dims=[col(2)], labels=["mean"] = 2 cells.
    // First sum over 3 rows of 3.0 → 9.0 in each (col, k). Then mean across 2 labels → 9.0.
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 2u);
    EXPECT_DOUBLE_EQ(vo[0], 9.0);
    EXPECT_DOUBLE_EQ(vo[1], 9.0);
}

TEST_F(ExpressionFixture, AgentSaveProducesReadableFile) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).aggregate_agents(ExpressionAggregateAgents::Operation::Sum).save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    const auto& m = reopened.get_metadata();
    EXPECT_EQ(m.labels.size(), 1u);
    EXPECT_EQ(m.labels[0], "sum");
    EXPECT_EQ(m.dimensions.size(), 2u);
    EXPECT_EQ(m.unit, "MW");
}

TEST_F(ExpressionFixture, SaveAcceptsClosedInputs) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });

    BinaryFile a(path_a);  // unopened; metadata loaded from disk at Expression construction
    BinaryFile b(path_b);
    EXPECT_FALSE(a.is_open());
    EXPECT_FALSE(b.is_open());

    Expression e = Expression(a) + Expression(b) * 2.0;
    e.save(path_out);

    // User-supplied BinaryFiles are untouched.
    EXPECT_FALSE(a.is_open());
    EXPECT_FALSE(b.is_open());

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i) {
        EXPECT_DOUBLE_EQ(vo[i], va[i] + vb[i] * 2.0);
    }
}

TEST_F(ExpressionFixture, SaveDoesNotMutatePreOpenedInputs) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>&, size_t) { return 7.0; });

    auto a = BinaryFile::open_file(path_a, 'r');  // pre-opened by caller
    BinaryFile b(path_b);                         // closed
    EXPECT_TRUE(a.is_open());
    EXPECT_FALSE(b.is_open());

    Expression e = Expression(a) + Expression(b);
    e.save(path_out);

    // save() reads from its own internal handles; it never touches the caller's BinaryFiles.
    EXPECT_TRUE(a.is_open());
    EXPECT_FALSE(b.is_open());

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i) {
        EXPECT_DOUBLE_EQ(vo[i], va[i] + vb[i]);
    }
}

TEST_F(ExpressionFixture, SaveReleasesInternalHandlesOnDestruction) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });

    {
        BinaryFile a(path_a);
        Expression e = Expression(a) + 1.0;
        e.save(path_out);
    }  // Expression goes out of scope here -> ExpressionFile destroyed -> internal BinaryFile closed.

    // path_a is now free to be re-opened in write mode.
    auto reopened_writer = BinaryFile::open_file(path_a, 'w', md);  // must not throw
    EXPECT_TRUE(reopened_writer.is_open());
}

TEST_F(ExpressionFixture, UnaryNegate) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = -Expression(a);
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], -va[i]);
}

TEST_F(ExpressionFixture, UnaryAbs) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        // Alternate sign so abs has work to do.
        const double v = static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
        return (dims[0] % 2 == 0) ? -v : v;
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = abs(Expression(a));
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], std::abs(va[i]));
}

TEST_F(ExpressionFixture, UnarySqrt) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k) + 1);  // > 0
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = sqrt(Expression(a));
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], std::sqrt(va[i]));
}

TEST_F(ExpressionFixture, UnarySqrtPropagatesNaNOnNegative) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return -1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = sqrt(Expression(a));
    e.save(path_out);

    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < vo.size(); ++i)
        EXPECT_TRUE(std::isnan(vo[i]));
}

TEST_F(ExpressionFixture, UnaryLog) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k) + 1);  // > 0
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = log(Expression(a));
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], std::log(va[i]));
}

TEST_F(ExpressionFixture, UnaryExp) {
    auto md = make_simple_metadata();
    // Small magnitudes keep exp() in a comfortable numerical range.
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k)) * 0.1;
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = exp(Expression(a));
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], std::exp(va[i]));
}

TEST_F(ExpressionFixture, UnaryMetadataPreserved) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression neg = -Expression(a);

    const auto& m = neg.metadata();
    EXPECT_EQ(m.unit, "MW");
    ASSERT_EQ(m.dimensions.size(), 2u);
    EXPECT_EQ(m.dimensions[0].name, "row");
    EXPECT_EQ(m.dimensions[0].size, 3);
    EXPECT_EQ(m.dimensions[1].name, "col");
    EXPECT_EQ(m.dimensions[1].size, 2);
    ASSERT_EQ(m.labels.size(), 2u);
    EXPECT_EQ(m.labels[0], "val1");
    EXPECT_EQ(m.labels[1], "val2");
}

TEST_F(ExpressionFixture, UnaryComposes) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression e = abs(-Expression(a));
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], std::abs(va[i]));
}

TEST_F(ExpressionFixture, UnaryComposesWithBinary) {
    // Spot-check that -(a + b) parses cleanly and produces the expected result.
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 2 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    Expression e = -(Expression(a) + Expression(b));
    e.save(path_out);

    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(va.size(), vo.size());
    for (size_t i = 0; i < va.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], -(va[i] + vb[i]));
}

TEST_F(ExpressionFixture, IfElseSelectsByCondition) {
    // cond: nonzero at row 1, zero at rows 2-3
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return (dims[0] == 1) ? 1.0 : 0.0;  // condition
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));  // then
    });
    write_qvr(path_c, md, [](const std::vector<int64_t>& dims, size_t k) {
        return -static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));  // else
    });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));
    e.save(path_out);

    auto vc = read_all_cells(path_a);
    auto vt = read_all_cells(path_b);
    auto ve = read_all_cells(path_c);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vc.size(), vo.size());
    for (size_t i = 0; i < vo.size(); ++i) {
        const double expected = (vc[i] != 0.0) ? vt[i] : ve[i];
        EXPECT_DOUBLE_EQ(vo[i], expected) << " at index " << i;
    }
}

TEST_F(ExpressionFixture, IfElsePropagatesNaNInCondition) {
    auto md = make_simple_metadata();
    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    // cond: NaN at (1,1), 1 elsewhere
    write_qvr(path_a, md, [&](const std::vector<int64_t>& dims, size_t /*k*/) {
        return (dims[0] == 1 && dims[1] == 1) ? nan_v : 1.0;
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>&, size_t) { return 7.0; });
    write_qvr(path_c, md, [](const std::vector<int64_t>&, size_t) { return -7.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));
    e.save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    auto cell_11 = reopened.read({{"row", 1}, {"col", 1}}, true);
    auto cell_22 = reopened.read({{"row", 2}, {"col", 2}}, true);
    EXPECT_TRUE(std::isnan(cell_11[0]));
    EXPECT_TRUE(std::isnan(cell_11[1]));
    EXPECT_DOUBLE_EQ(cell_22[0], 7.0);
    EXPECT_DOUBLE_EQ(cell_22[1], 7.0);
}

TEST_F(ExpressionFixture, IfElseUnselectedBranchNaNDoesNotPropagate) {
    auto md = make_simple_metadata();
    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    // cond is always 1 (true) -> only `then` matters; else NaN is irrelevant.
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md, [](const std::vector<int64_t>&, size_t) { return 42.0; });
    write_qvr(path_c, md, [&](const std::vector<int64_t>&, size_t) { return nan_v; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));
    e.save(path_out);

    auto vo = read_all_cells(path_out);
    for (double v : vo) {
        EXPECT_DOUBLE_EQ(v, 42.0);
    }
}

TEST_F(ExpressionFixture, IfElseBroadcastsConditionSizeOneDim) {
    auto md_cond = BinaryMetadata::from_element(Element()
                                                    .set("version", "1")
                                                    .set("initial_datetime", "2025-01-01T00:00:00")
                                                    .set("unit", "flag")
                                                    .set("dimensions", {"row", "col"})
                                                    .set("dimension_sizes", {1, 2})  // broadcast row
                                                    .set("labels", {"val1", "val2"}));
    auto md_full = make_simple_metadata();
    // cond at row=1 is [1, 0] for col=1, col=2 respectively (per-column mask)
    write_qvr(
        path_a, md_cond, [](const std::vector<int64_t>& dims, size_t /*k*/) { return (dims[1] == 1) ? 1.0 : 0.0; });
    write_qvr(path_b, md_full, [](const std::vector<int64_t>&, size_t) { return 100.0; });
    write_qvr(path_c, md_full, [](const std::vector<int64_t>&, size_t) { return -100.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));
    e.save(path_out);

    // Output row=3 (from full), col=2 (matches). For all rows: col=1 selects then (100),
    // col=2 selects else (-100). Condition's size-1 row dim broadcasts.
    auto reopened = BinaryFile::open_file(path_out, 'r');
    EXPECT_EQ(reopened.get_metadata().dimensions[0].size, 3);
    for (int64_t r = 1; r <= 3; ++r) {
        auto cell_r1 = reopened.read({{"row", r}, {"col", 1}}, true);
        auto cell_r2 = reopened.read({{"row", r}, {"col", 2}}, true);
        EXPECT_DOUBLE_EQ(cell_r1[0], 100.0);
        EXPECT_DOUBLE_EQ(cell_r2[0], -100.0);
    }
}

TEST_F(ExpressionFixture, IfElseBroadcastsLabels) {
    auto md_single = BinaryMetadata::from_element(Element()
                                                      .set("version", "1")
                                                      .set("initial_datetime", "2025-01-01T00:00:00")
                                                      .set("unit", "flag")
                                                      .set("dimensions", {"row", "col"})
                                                      .set("dimension_sizes", {3, 2})
                                                      .set("labels", {"only"}));  // 1 label
    auto md_full = make_simple_metadata();                                        // 2 labels
    write_qvr(
        path_a, md_single, [](const std::vector<int64_t>& dims, size_t /*k*/) { return (dims[0] == 1) ? 1.0 : 0.0; });
    write_qvr(path_b, md_full, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_c, md_full, [](const std::vector<int64_t>& dims, size_t k) {
        return -static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));
    e.save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    ASSERT_EQ(reopened.get_metadata().labels.size(), 2u);
    auto cell_11 = reopened.read({{"row", 1}, {"col", 1}}, true);
    auto cell_22 = reopened.read({{"row", 2}, {"col", 2}}, true);
    // row=1: cond=1 -> then (positive); row=2: cond=0 -> else (negative).
    EXPECT_DOUBLE_EQ(cell_11[0], 1.0 * 10 + 1.0 + 0);
    EXPECT_DOUBLE_EQ(cell_11[1], 1.0 * 10 + 1.0 + 1);
    EXPECT_DOUBLE_EQ(cell_22[0], -(2.0 * 10 + 2.0 + 0));
    EXPECT_DOUBLE_EQ(cell_22[1], -(2.0 * 10 + 2.0 + 1));
}

TEST_F(ExpressionFixture, IfElseUnitMismatchThenElseThrows) {
    auto md_t = make_simple_metadata();  // unit "MW"
    auto md_f = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "kWh")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {3, 2})
                                                 .set("labels", {"val1", "val2"}));
    write_qvr(path_a, md_t, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_t, [](const std::vector<int64_t>&, size_t) { return 2.0; });
    write_qvr(path_c, md_f, [](const std::vector<int64_t>&, size_t) { return 3.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    EXPECT_THROW({ auto e = ifelse(Expression(cond), Expression(then_v), Expression(else_v)); }, std::runtime_error);
}

TEST_F(ExpressionFixture, IfElseConditionUnitIgnored) {
    // cond has a different unit than then/else; should succeed.
    auto md_cond = BinaryMetadata::from_element(Element()
                                                    .set("version", "1")
                                                    .set("initial_datetime", "2025-01-01T00:00:00")
                                                    .set("unit", "flag")
                                                    .set("dimensions", {"row", "col"})
                                                    .set("dimension_sizes", {3, 2})
                                                    .set("labels", {"val1", "val2"}));
    auto md_branch = make_simple_metadata();  // "MW"
    write_qvr(path_a, md_cond, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_branch, [](const std::vector<int64_t>&, size_t) { return 10.0; });
    write_qvr(path_c, md_branch, [](const std::vector<int64_t>&, size_t) { return 20.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = ifelse(Expression(cond), Expression(then_v), Expression(else_v));
    e.save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    EXPECT_EQ(reopened.get_metadata().unit, "MW");  // output unit = then.unit (== else.unit)
    auto vo = read_all_cells(path_out);
    for (double v : vo) {
        EXPECT_DOUBLE_EQ(v, 10.0);  // all cells select then (cond=1)
    }
}

TEST_F(ExpressionFixture, IfElseShapeMismatchThrows) {
    auto md_t = make_simple_metadata();  // 3x2
    auto md_f = BinaryMetadata::from_element(Element()
                                                 .set("version", "1")
                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                 .set("unit", "MW")
                                                 .set("dimensions", {"row", "col"})
                                                 .set("dimension_sizes", {4, 2})  // size 4 vs 3
                                                 .set("labels", {"val1", "val2"}));
    write_qvr(path_a, md_t, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_t, [](const std::vector<int64_t>&, size_t) { return 2.0; });
    write_qvr(path_c, md_f, [](const std::vector<int64_t>&, size_t) { return 3.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    EXPECT_THROW({ auto e = ifelse(Expression(cond), Expression(then_v), Expression(else_v)); }, std::runtime_error);
}

TEST_F(ExpressionFixture, IfElseChainsWithBinary) {
    // Verify ifelse composes with binary ops: 2 * ifelse(cond, a, b) + 1
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t /*k*/) { return (dims[0] == 1) ? 1.0 : 0.0; });
    write_qvr(path_b, md, [](const std::vector<int64_t>&, size_t) { return 10.0; });
    write_qvr(path_c, md, [](const std::vector<int64_t>&, size_t) { return 20.0; });

    auto cond = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    Expression e = 2.0 * ifelse(Expression(cond), Expression(then_v), Expression(else_v)) + 1.0;
    e.save(path_out);

    auto vc = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    for (size_t i = 0; i < vo.size(); ++i) {
        const double base = (vc[i] != 0.0) ? 10.0 : 20.0;
        EXPECT_DOUBLE_EQ(vo[i], 2.0 * base + 1.0);
    }
}

// =============================================================================
// ExpressionSelectAgents — label-axis projection
// =============================================================================

TEST_F(ExpressionFixture, SelectAgentsSubset) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).select_agents({"val2"});
    out.save(path_out);

    const auto& m = out.metadata();
    ASSERT_EQ(m.labels.size(), 1u);
    EXPECT_EQ(m.labels[0], "val2");

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 6u);
    // val2 is k=1: cells = 10r + c + 1
    EXPECT_DOUBLE_EQ(vo[0], 12.0);  // r=1, c=1
    EXPECT_DOUBLE_EQ(vo[1], 13.0);  // r=1, c=2
    EXPECT_DOUBLE_EQ(vo[2], 22.0);  // r=2, c=1
    EXPECT_DOUBLE_EQ(vo[3], 23.0);
    EXPECT_DOUBLE_EQ(vo[4], 32.0);
    EXPECT_DOUBLE_EQ(vo[5], 33.0);
}

TEST_F(ExpressionFixture, SelectAgentsReorder) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).select_agents({"val2", "val1"});
    out.save(path_out);

    const auto& m = out.metadata();
    ASSERT_EQ(m.labels.size(), 2u);
    EXPECT_EQ(m.labels[0], "val2");
    EXPECT_EQ(m.labels[1], "val1");

    auto vo = read_all_cells(path_out);
    // Per cell pair: [val2 first (= 10r+c+1), val1 second (= 10r+c)]
    EXPECT_DOUBLE_EQ(vo[0], 12.0);  // r=1, c=1, val2
    EXPECT_DOUBLE_EQ(vo[1], 11.0);  // r=1, c=1, val1
}

TEST_F(ExpressionFixture, SelectAgentsDuplicateThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t k) { return static_cast<double>(k); });
    auto a = BinaryFile::open_file(path_a, 'r');

    // BinaryMetadata::validate requires unique labels, so duplicates are rejected at construction.
    EXPECT_THROW(Expression(a).select_agents({"val1", "val1"}), std::runtime_error);
}

TEST_F(ExpressionFixture, SelectAgentsMissingThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).select_agents({"nope"}), std::runtime_error);
}

TEST_F(ExpressionFixture, SelectAgentsAfterBinary) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t k) { return 10.0 + static_cast<double>(k); });
    write_qvr(path_b, md, [](const std::vector<int64_t>&, size_t k) { return 20.0 + static_cast<double>(k); });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    auto sum = Expression(a) + Expression(b);
    sum.select_agents({"val1"}).save(path_out);

    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), 6u);
    // val1 (k=0): 10 + 20 = 30 in every cell.
    for (double v : vo)
        EXPECT_DOUBLE_EQ(v, 30.0);
}

// =============================================================================
// ExpressionRenameAgents — label-axis rename
// =============================================================================

TEST_F(ExpressionFixture, RenameAgentsPartial) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).rename_agents({{"val1", "alpha"}});
    out.save(path_out);

    const auto& m = out.metadata();
    ASSERT_EQ(m.labels.size(), 2u);
    EXPECT_EQ(m.labels[0], "alpha");
    EXPECT_EQ(m.labels[1], "val2");

    auto orig = read_all_cells(path_a);
    auto renamed = read_all_cells(path_out);
    ASSERT_EQ(orig.size(), renamed.size());
    for (size_t i = 0; i < orig.size(); ++i)
        EXPECT_DOUBLE_EQ(orig[i], renamed[i]);
}

TEST_F(ExpressionFixture, RenameAgentsAll) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto out = Expression(a).rename_agents({{"val1", "alpha"}, {"val2", "beta"}});

    const auto& m = out.metadata();
    ASSERT_EQ(m.labels.size(), 2u);
    EXPECT_EQ(m.labels[0], "alpha");
    EXPECT_EQ(m.labels[1], "beta");
}

TEST_F(ExpressionFixture, RenameAgentsMissingThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).rename_agents({{"nope", "x"}}), std::runtime_error);
}

TEST_F(ExpressionFixture, RenameAgentsCollisionThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    // Rename val1 -> val2 leaves output labels = {"val2", "val2"} which validate() rejects.
    EXPECT_THROW(Expression(a).rename_agents({{"val1", "val2"}}), std::runtime_error);
}

TEST_F(ExpressionFixture, RenameAgentsDuplicateKeyThrows) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    EXPECT_THROW(Expression(a).rename_agents({{"val1", "alpha"}, {"val1", "beta"}}), std::runtime_error);
}

TEST_F(ExpressionFixture, ChainSelectThenRename) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).select_agents({"val2"}).rename_agents({{"val2", "renamed"}}).save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    const auto& m = reopened.get_metadata();
    ASSERT_EQ(m.labels.size(), 1u);
    EXPECT_EQ(m.labels[0], "renamed");

    auto vo = read_all_cells(path_out);
    // Same values as val2 column from the original (10r + c + 1).
    EXPECT_DOUBLE_EQ(vo[0], 12.0);
    EXPECT_DOUBLE_EQ(vo[1], 13.0);
}

TEST_F(ExpressionFixture, RenameAgentsSaveProducesReadableFile) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>&, size_t) { return 7.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    Expression(a).rename_agents({{"val1", "alpha"}}).save(path_out);

    auto reopened = BinaryFile::open_file(path_out, 'r');
    const auto& m = reopened.get_metadata();
    ASSERT_EQ(m.labels.size(), 2u);
    EXPECT_EQ(m.labels[0], "alpha");
    EXPECT_EQ(m.labels[1], "val2");
}

// ============================================================================
// Comparison operations (gt / lt / gte / lte / eq / neq) -> 1.0 / 0.0 per cell
// ============================================================================

TEST_F(ExpressionFixture, ComparisonAllOpsTwoFiles) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[1] * 10 + dims[0] + static_cast<int64_t>(k));
    });
    auto va = read_all_cells(path_a);
    auto vb = read_all_cells(path_b);

    struct Case {
        std::function<Expression(const Expression&, const Expression&)> build;
        std::function<bool(double, double)> ref;
    };
    const std::vector<Case> cases = {
        {[](const Expression& x, const Expression& y) { return gt(x, y); }, [](double x, double y) { return x > y; }},
        {[](const Expression& x, const Expression& y) { return lt(x, y); }, [](double x, double y) { return x < y; }},
        {[](const Expression& x, const Expression& y) { return gte(x, y); }, [](double x, double y) { return x >= y; }},
        {[](const Expression& x, const Expression& y) { return lte(x, y); }, [](double x, double y) { return x <= y; }},
        {[](const Expression& x, const Expression& y) { return eq(x, y); }, [](double x, double y) { return x == y; }},
        {[](const Expression& x, const Expression& y) { return neq(x, y); }, [](double x, double y) { return x != y; }},
    };

    for (const auto& c : cases) {
        auto a = BinaryFile::open_file(path_a, 'r');
        auto b = BinaryFile::open_file(path_b, 'r');
        c.build(Expression(a), Expression(b)).save(path_out);
        auto vo = read_all_cells(path_out);
        ASSERT_EQ(vo.size(), va.size());
        for (size_t i = 0; i < vo.size(); ++i) {
            EXPECT_DOUBLE_EQ(vo[i], c.ref(va[i], vb[i]) ? 1.0 : 0.0) << " at index " << i;
        }
        cleanup();
        write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
            return static_cast<double>(dims[0] * 10 + dims[1] + static_cast<int64_t>(k));
        });
        write_qvr(path_b, md, [](const std::vector<int64_t>& dims, size_t k) {
            return static_cast<double>(dims[1] * 10 + dims[0] + static_cast<int64_t>(k));
        });
    }
}

TEST_F(ExpressionFixture, ComparisonScalarBothSides) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });
    auto va = read_all_cells(path_a);

    {
        auto a = BinaryFile::open_file(path_a, 'r');
        gt(Expression(a), 100.0).save(path_out);
        auto vo = read_all_cells(path_out);
        ASSERT_EQ(vo.size(), va.size());
        for (size_t i = 0; i < vo.size(); ++i)
            EXPECT_DOUBLE_EQ(vo[i], (va[i] > 100.0) ? 1.0 : 0.0);
    }
    cleanup();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t k) {
        return static_cast<double>(dims[0] * 100 + dims[1] * 10 + static_cast<int64_t>(k));
    });
    {
        // scalar on the left: 100 < expr
        auto a = BinaryFile::open_file(path_a, 'r');
        lt(100.0, Expression(a)).save(path_out2);
        auto vo = read_all_cells(path_out2);
        ASSERT_EQ(vo.size(), va.size());
        for (size_t i = 0; i < vo.size(); ++i)
            EXPECT_DOUBLE_EQ(vo[i], (100.0 < va[i]) ? 1.0 : 0.0);
    }
}

TEST_F(ExpressionFixture, ComparisonPropagatesNaN) {
    auto md = make_simple_metadata();
    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    // NaN at (1,1), finite elsewhere
    write_qvr(path_a, md, [&](const std::vector<int64_t>& dims, size_t /*k*/) {
        return (dims[0] == 1 && dims[1] == 1) ? nan_v : 5.0;
    });
    auto a = BinaryFile::open_file(path_a, 'r');
    gt(Expression(a), 3.0).save(path_out);
    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), va.size());
    for (size_t i = 0; i < vo.size(); ++i) {
        if (std::isnan(va[i])) {
            EXPECT_TRUE(std::isnan(vo[i])) << " at index " << i;
        } else {
            EXPECT_DOUBLE_EQ(vo[i], 1.0) << " at index " << i;  // 5.0 > 3.0
        }
    }
}

TEST_F(ExpressionFixture, ComparisonDrivesIfElse) {
    auto md = make_simple_metadata();
    write_qvr(path_a, md, [](const std::vector<int64_t>& dims, size_t /*k*/) {
        return static_cast<double>(dims[0]);  // 1, 2, 3 across rows
    });
    write_qvr(path_b, md, [](const std::vector<int64_t>&, size_t) { return 100.0; });  // then
    write_qvr(path_c, md, [](const std::vector<int64_t>&, size_t) { return -100.0; });  // else

    auto a = BinaryFile::open_file(path_a, 'r');
    auto then_v = BinaryFile::open_file(path_b, 'r');
    auto else_v = BinaryFile::open_file(path_c, 'r');
    // where a > 1.5 pick then(100) else else(-100)
    ifelse(gt(Expression(a), 1.5), Expression(then_v), Expression(else_v)).save(path_out);

    auto va = read_all_cells(path_a);
    auto vo = read_all_cells(path_out);
    ASSERT_EQ(vo.size(), va.size());
    for (size_t i = 0; i < vo.size(); ++i)
        EXPECT_DOUBLE_EQ(vo[i], (va[i] > 1.5) ? 100.0 : -100.0) << " at index " << i;
}

TEST_F(ExpressionFixture, ComparisonUnitMismatchThrows) {
    auto md_a = BinaryMetadata::from_element(Element()
                                                .set("unit", "MW")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {2})
                                                .set("labels", {"v"}));
    auto md_b = BinaryMetadata::from_element(Element()
                                                .set("unit", "GWh")
                                                .set("dimensions", {"row"})
                                                .set("dimension_sizes", {2})
                                                .set("labels", {"v"}));
    write_qvr(path_a, md_a, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    write_qvr(path_b, md_b, [](const std::vector<int64_t>&, size_t) { return 1.0; });
    auto a = BinaryFile::open_file(path_a, 'r');
    auto b = BinaryFile::open_file(path_b, 'r');
    EXPECT_THROW({ auto e = eq(Expression(a), Expression(b)); }, std::runtime_error);
}
