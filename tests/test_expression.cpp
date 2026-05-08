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

TEST_F(ExpressionFixture, ExpressionFromWriteModeBinaryFileThrows) {
    auto md = make_simple_metadata();
    auto writer = BinaryFile::open_file(path_a, 'w', md);

    EXPECT_THROW(
        {
            try {
                Expression e(writer);
                (void)e;
            } catch (const std::runtime_error& err) {
                EXPECT_STREQ(err.what(), "Cannot create_expression: BinaryFile must be opened in read mode");
                throw;
            }
        },
        std::runtime_error);
}

TEST_F(ExpressionFixture, IsReadModeAccessor) {
    auto md = make_simple_metadata();
    {
        auto writer = BinaryFile::open_file(path_a, 'w', md);
        EXPECT_FALSE(writer.is_read_mode());
    }
    auto reader = BinaryFile::open_file(path_a, 'r');
    EXPECT_TRUE(reader.is_read_mode());
}
