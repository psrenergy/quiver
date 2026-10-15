#include <gtest/gtest.h>
#include <quiver/binary/binary_file.h>
#include <quiver/c/binary/parquet.h>
#include <quiver/c/common.h>
#include <quiver/element.h>

#include <filesystem>
#include <random>
#include <string>

TEST(ParquetCApi, ExportsClosedBinaryFile) {
    namespace fs = std::filesystem;
    const auto path =
        (fs::temp_directory_path() / ("quiver_c_parquet_" + std::to_string(std::random_device{}()))).string();
    auto meta = quiver::BinaryMetadata::from_element(
        quiver::Element()
            .set("version", "1")
            .set("initial_datetime", "2024-01-01T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"row"})
            .set("dimension_sizes", {1})
            .set("labels", {"value"})
    );
    auto writer = quiver::BinaryFile::open_file(path, 'w', meta);
    writer.write({1.5}, {{"row", 1}});
    writer.close();
    EXPECT_EQ(quiver_bin_to_parquet(path.c_str()), QUIVER_OK);
    EXPECT_GT(fs::file_size(path + ".parquet"), 8);
    for (const auto* extension : {".qvr", ".toml", ".parquet"}) {
        fs::remove(path + extension);
    }
}

TEST(ParquetCApi, ReportsMissingInputThroughSharedErrorChannel) {
    EXPECT_EQ(quiver_bin_to_parquet("quiver_missing_parquet_input"), QUIVER_ERROR);
    EXPECT_NE(std::string(quiver_get_last_error()).find("Cannot bin_to_parquet"), std::string::npos);
}

TEST(ParquetCApi, RejectsNullPath) {
    EXPECT_EQ(quiver_bin_to_parquet(nullptr), QUIVER_ERROR);
    EXPECT_NE(std::string(quiver_get_last_error()).find("Null argument"), std::string::npos);
}
