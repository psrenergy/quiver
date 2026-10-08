#include <arrow/api.h>
#include <arrow/io/api.h>
#include <gtest/gtest.h>
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>
#include <quiver/binary/binary_file.h>
#include <quiver/binary/parquet.h>
#include <quiver/element.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>

namespace fs = std::filesystem;
using namespace quiver;

class ParquetTest : public ::testing::Test {
protected:
    fs::path directory;
    std::string path;

    void SetUp() override {
        directory = fs::temp_directory_path() / ("quiver_parquet_" + std::to_string(std::random_device{}()));
        fs::create_directory(directory);
        path = (directory / "data").string();
    }

    void TearDown() override {
        fs::remove_all(directory);
    }

    static BinaryMetadata metadata(int64_t rows = 3) {
        return BinaryMetadata::from_element(
            Element()
                .set("version", "1")
                .set("initial_datetime", "2024-01-01T00:00:00")
                .set("unit", "MW")
                .set("dimensions", {"row"})
                .set("dimension_sizes", std::vector<int64_t>{rows})
                .set("labels", {"a", "b"})
        );
    }

    std::shared_ptr<arrow::Table> read() {
        auto input = arrow::io::ReadableFile::Open(path + ".parquet").ValueOrDie();
        auto reader = parquet::arrow::OpenFile(input, arrow::default_memory_pool()).ValueOrDie();
        auto table = reader->ReadTable();
        EXPECT_TRUE(table.ok());
        return table.ValueOrDie()->CombineChunks().ValueOrDie();
    }

    static std::string bytes(const fs::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
};

TEST_F(ParquetTest, PreservesFinalValuesNullsSchemaAndMetadata) {
    auto meta = metadata();
    auto writer = BinaryFile::open_file(path, 'w', meta);
    writer.write({3, std::numeric_limits<double>::infinity()}, {{"row", 3}});
    writer.write({99, 99}, {{"row", 1}});
    writer.write({-0.0, -std::numeric_limits<double>::infinity()}, {{"row", 1}});
    writer.close();
    bin_to_parquet(path);
    const auto table = read();
    ASSERT_EQ(table->num_rows(), 3);
    EXPECT_EQ(table->ColumnNames(), (std::vector<std::string>{"row", "a", "b"}));
    EXPECT_FALSE(table->schema()->field(0)->nullable());
    EXPECT_TRUE(table->schema()->field(1)->nullable());
    auto rows = std::static_pointer_cast<arrow::Int64Array>(table->column(0)->chunk(0));
    EXPECT_EQ(rows->Value(0), 1);
    EXPECT_EQ(rows->Value(2), 3);
    auto a = std::static_pointer_cast<arrow::DoubleArray>(table->column(1)->chunk(0));
    EXPECT_TRUE(std::signbit(a->Value(0)));
    EXPECT_TRUE(a->IsNull(1));
    EXPECT_EQ(a->Value(2), 3);
    auto b = std::static_pointer_cast<arrow::DoubleArray>(table->column(2)->chunk(0));
    EXPECT_EQ(b->Value(0), -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(b->IsNull(1));
    EXPECT_EQ(b->Value(2), std::numeric_limits<double>::infinity());
    auto file = parquet::ParquetFileReader::OpenFile(path + ".parquet");
    EXPECT_EQ(file->metadata()->key_value_metadata()->Get("quiver:metadata").ValueOrDie(), meta.to_toml());
    EXPECT_EQ(file->metadata()->key_value_metadata()->Get("quiver:parquet_version").ValueOrDie(), "1");
    EXPECT_EQ(file->metadata()->RowGroup(0)->ColumnChunk(1)->compression(), parquet::Compression::ZSTD);
    file.reset();
    bin_to_parquet(path);
    EXPECT_EQ(read()->num_rows(), 3);
}

TEST_F(ParquetTest, WritesMultipleRowGroupsAndPartialTail) {
    constexpr int64_t count = 2 * 65536 + 3;
    auto writer = BinaryFile::open_file(path, 'w', metadata(count));
    writer.write({7, 8}, {{"row", count}});
    writer.close();
    bin_to_parquet(path);
    auto file = parquet::ParquetFileReader::OpenFile(path + ".parquet");
    ASSERT_EQ(file->metadata()->num_row_groups(), 3);
    EXPECT_EQ(file->metadata()->RowGroup(2)->num_rows(), 3);
    auto table = read();
    EXPECT_EQ(table->num_rows(), count);
    auto a = std::static_pointer_cast<arrow::DoubleArray>(table->column(1)->chunk(0));
    EXPECT_EQ(a->null_count(), count - 1);
    EXPECT_EQ(a->Value(count - 1), 7);
}

TEST_F(ParquetTest, TimestampIdentifiesLeapYearCalendarCells) {
    auto meta = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2024-02-28T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"month", "day"})
            .set("dimension_sizes", {3, 31})
            .set("time_dimensions", {"month", "day"})
            .set("frequencies", {"monthly", "daily"})
            .set("labels", {"a"})
    );
    auto writer = BinaryFile::open_file(path, 'w', meta);
    writer.close();
    bin_to_parquet(path);
    auto table = read();
    auto timestamp = std::static_pointer_cast<arrow::TimestampArray>(table->GetColumnByName("datetime")->chunk(0));
    EXPECT_EQ(timestamp->Value(1) - timestamp->Value(0), 86400LL * 1000000);
    EXPECT_EQ(timestamp->Value(2) - timestamp->Value(1), 86400LL * 1000000);
    EXPECT_EQ(timestamp->type()->ToString(), "timestamp[us, tz=UTC]");
    auto months = std::static_pointer_cast<arrow::Int64Array>(table->column(0)->chunk(0));
    auto days = std::static_pointer_cast<arrow::Int64Array>(table->column(1)->chunk(0));
    EXPECT_EQ(months->Value(0), 1);
    EXPECT_EQ(days->Value(0), 28);
    EXPECT_EQ(days->Value(1), 29);
    EXPECT_EQ(months->Value(2), 2);
    EXPECT_EQ(days->Value(2), 1);
}

TEST_F(ParquetTest, FailuresPreservePreviousExportAndCleanTemporaryFiles) {
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    auto writer = BinaryFile::open_file(path, 'w', metadata());
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    writer.close();
    bin_to_parquet(path);
    const auto previous = bytes(path + ".parquet");
    fs::resize_file(path + ".qvr", 1);
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    EXPECT_EQ(bytes(path + ".parquet"), previous);
    auto reader = BinaryFile::open_file(path, 'r');
    EXPECT_THROW(reader.read({{"row", 1}}, true), std::runtime_error);
    reader.close();
    writer = BinaryFile::open_file(path, 'w', metadata());
    writer.close();
    fs::remove(path + ".parquet");
    fs::create_directory(path + ".parquet");
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    EXPECT_TRUE(fs::is_directory(path + ".parquet"));
    for (const auto& entry : fs::directory_iterator(directory)) {
        EXPECT_EQ(entry.path().filename().string().find(".tmp-"), std::string::npos);
    }
}

TEST_F(ParquetTest, RejectsDimensionAgentNameCollision) {
    auto meta = metadata();
    meta.labels[0] = "row";
    auto writer = BinaryFile::open_file(path, 'w', meta);
    writer.close();
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    EXPECT_FALSE(fs::exists(path + ".parquet"));
}

TEST_F(ParquetTest, RejectsGeneratedTimestampNameCollision) {
    auto meta = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2024-01-01T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"period"})
            .set("dimension_sizes", {1})
            .set("time_dimensions", {"period"})
            .set("frequencies", {"monthly"})
            .set("labels", {"datetime"})
    );
    auto writer = BinaryFile::open_file(path, 'w', meta);
    writer.close();
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    EXPECT_FALSE(fs::exists(path + ".parquet"));
}

TEST_F(ParquetTest, WideSchemasReduceBatchRows) {
    auto meta = metadata(10000);
    meta.labels.clear();
    for (int i = 0; i < 512; ++i) {
        meta.labels.push_back("agent_" + std::to_string(i));
    }
    auto writer = BinaryFile::open_file(path, 'w', meta);
    writer.close();
    bin_to_parquet(path);
    auto file = parquet::ParquetFileReader::OpenFile(path + ".parquet");
    EXPECT_EQ(file->metadata()->num_rows(), 10000);
    EXPECT_GE(file->metadata()->num_row_groups(), 2);
    EXPECT_EQ(file->metadata()->num_columns(), 513);
}

TEST_F(ParquetTest, RejectsOverflowingSizeAndMalformedMetadataBeforeCreatingOutput) {
    auto meta = metadata();
    meta.dimensions[0].size = std::numeric_limits<int64_t>::max();
    std::ofstream(path + ".toml") << meta.to_toml();
    std::ofstream(path + ".qvr") << "x";
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    EXPECT_FALSE(fs::exists(path + ".parquet"));
    std::ofstream(path + ".toml") << "dimensions = [";
    EXPECT_THROW(bin_to_parquet(path), std::runtime_error);
    EXPECT_FALSE(fs::exists(path + ".parquet"));
}

TEST_F(ParquetTest, TimestampUsesTheExistingCalendarRulesForEveryParentChildPair) {
    struct Pair {
        std::string parent;
        std::string child;
        int64_t child_size;
        std::chrono::sys_days first_day;
    };
    using namespace std::chrono;
    const auto february_28 = sys_days{2024y / February / 28};
    for (
        const auto& pair : std::vector<Pair>{
            {"yearly", "monthly", 12, sys_days{2024y / February / 1}},
            {"yearly", "daily", 366, february_28},
            {"yearly", "hourly", 8784, february_28},
            {"monthly", "daily", 31, february_28},
            {"monthly", "hourly", 744, february_28},
            {"weekly", "daily", 7, february_28},
            {"weekly", "hourly", 168, february_28},
            {"daily", "hourly", 24, february_28}
        }
    ) {
        SCOPED_TRACE(pair.parent + "/" + pair.child);
        const auto meta = BinaryMetadata::from_element(
            Element()
                .set("version", "1")
                .set("initial_datetime", "2024-02-28T00:00:00")
                .set("unit", "MW")
                .set("dimensions", {"period", "cell"})
                .set("dimension_sizes", std::vector<int64_t>{2, pair.child_size})
                .set("time_dimensions", {"period", "cell"})
                .set("frequencies", std::vector<std::string>{pair.parent, pair.child})
                .set("labels", {"a"})
        );
        auto writer = BinaryFile::open_file(path, 'w', meta);
        writer.close();
        bin_to_parquet(path);
        auto table = read();
        auto times = std::static_pointer_cast<arrow::TimestampArray>(table->GetColumnByName("datetime")->chunk(0));
        EXPECT_EQ(times->Value(0), duration_cast<microseconds>(pair.first_day.time_since_epoch()).count());
        for (int64_t i = 1; i < times->length(); ++i) {
            EXPECT_GT(times->Value(i), times->Value(i - 1));
        }
    }
}

TEST_F(ParquetTest, WeeklyGridCrossesTheYearWithoutRestarting) {
    const auto meta = BinaryMetadata::from_element(
        Element()
            .set("version", "1")
            .set("initial_datetime", "2024-12-29T00:00:00")
            .set("unit", "MW")
            .set("dimensions", {"week", "day"})
            .set("dimension_sizes", {2, 7})
            .set("time_dimensions", {"week", "day"})
            .set("frequencies", {"weekly", "daily"})
            .set("labels", {"a"})
    );
    auto writer = BinaryFile::open_file(path, 'w', meta);
    writer.close();
    bin_to_parquet(path);
    auto table = read();
    ASSERT_EQ(table->num_rows(), 14);
    auto times = std::static_pointer_cast<arrow::TimestampArray>(table->GetColumnByName("datetime")->chunk(0));
    for (int64_t i = 1; i < 14; ++i) {
        EXPECT_EQ(times->Value(i) - times->Value(i - 1), 86400LL * 1000000);
    }
}
