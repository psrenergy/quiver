#include "../test_utils.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <map>
#include <numeric>
#include <quiver/c/options.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int ELEMENT_COUNT = 5000;
static constexpr int TS_ROWS_PER_ELEMENT = 10;
static constexpr int ITERATIONS = 5;

static std::string schema_file() {
    return quiver::test::path_from(__FILE__, "../schemas/valid/collections.sql");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string temp_db_path(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() / ("quiver_bench_" + suffix + ".db")).string();
}

static void remove_if_exists(const std::string& path) {
    std::filesystem::remove(path);
}

static quiver::Element make_element(int index) {
    quiver::Element e;
    e.set("label", std::string("Item " + std::to_string(index)));
    e.set("some_integer", static_cast<int64_t>(index * 10));
    e.set("some_float", static_cast<double>(index) * 1.1);

    std::vector<int64_t> ints(5);
    std::vector<double> floats(5);
    for (int i = 0; i < 5; ++i) {
        ints[i] = static_cast<int64_t>(index * (i + 1));
        floats[i] = index * (i + 1) * 0.1;
    }
    e.set("value_int", ints);
    e.set("value_float", floats);

    return e;
}

static std::vector<std::map<std::string, quiver::Value>> make_time_series_rows(int element_index) {
    std::vector<std::map<std::string, quiver::Value>> rows;
    rows.reserve(TS_ROWS_PER_ELEMENT);
    for (int r = 0; r < TS_ROWS_PER_ELEMENT; ++r) {
        char ts[32];
        std::snprintf(ts, sizeof(ts), "2024-01-01T%02d:00:00", r);
        rows.push_back(
            {{"date_time", std::string(ts)}, {"value", static_cast<double>((element_index * 10 + r) * 0.5)}});
    }
    return rows;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

struct Stats {
    double median_ms;
    double mean_ms;
    double per_element_ms;
    double ops_per_sec;
};

static Stats compute_stats(std::vector<double>& times_ms, int element_count) {
    std::sort(times_ms.begin(), times_ms.end());

    double median = times_ms[times_ms.size() / 2];
    double mean = std::accumulate(times_ms.begin(), times_ms.end(), 0.0) / static_cast<double>(times_ms.size());

    return {.median_ms = median,
            .mean_ms = mean,
            .per_element_ms = median / element_count,
            .ops_per_sec = element_count / (median / 1000.0)};
}

// ---------------------------------------------------------------------------
// Benchmark variants
// ---------------------------------------------------------------------------

static double run_individual(const std::string& schema_path, int element_count) {
    auto db_path = temp_db_path("individual");
    remove_if_exists(db_path);
    double elapsed_ms;

    {
        auto db = quiver::Database::from_schema(
            db_path, schema_path, {.read_only = false, .console_level = quiver::LogLevel::Off});

        // Configuration element (outside timed region)
        quiver::Element config;
        config.set("label", std::string("Default"));
        db.create_element("Configuration", config);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 1; i <= element_count; ++i) {
            auto elem = make_element(i);
            auto id = db.create_element("Collection", elem);
            auto rows = make_time_series_rows(i);
            db.update_time_series_group("Collection", "data", id, rows);
        }

        auto end = std::chrono::high_resolution_clock::now();
        elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    }

    remove_if_exists(db_path);
    return elapsed_ms;
}

static double run_batched(const std::string& schema_path, int element_count) {
    auto db_path = temp_db_path("batched");
    remove_if_exists(db_path);
    double elapsed_ms;

    {
        auto db = quiver::Database::from_schema(
            db_path, schema_path, {.read_only = false, .console_level = quiver::LogLevel::Off});

        // Configuration element (outside timed region)
        quiver::Element config;
        config.set("label", std::string("Default"));
        db.create_element("Configuration", config);

        auto start = std::chrono::high_resolution_clock::now();

        db.begin_transaction();
        for (int i = 1; i <= element_count; ++i) {
            auto elem = make_element(i);
            auto id = db.create_element("Collection", elem);
            auto rows = make_time_series_rows(i);
            db.update_time_series_group("Collection", "data", id, rows);
        }
        db.commit();

        auto end = std::chrono::high_resolution_clock::now();
        elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    }

    remove_if_exists(db_path);
    return elapsed_ms;
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

static void print_results(const Stats& individual,
                          const Stats& batched,
                          int element_count,
                          int ts_rows,
                          int iterations,
                          const std::string& schema_name) {
    std::printf("\n");
    std::printf("========================================================\n");
    std::printf("  Quiver Transaction Benchmark\n");
    std::printf("========================================================\n");
    std::printf("  Elements:       %d\n", element_count);
    std::printf("  TS rows/elem:   %d\n", ts_rows);
    std::printf("  Schema:         %s\n", schema_name.c_str());
    std::printf("  Iterations:     %d\n", iterations);
    std::printf("========================================================\n");
    std::printf("\n");
    std::printf("%-20s %12s %14s %12s %10s\n", "Variant", "Total (ms)", "Per-elem (ms)", "Ops/sec", "Speedup");
    std::printf("%-20s %12s %14s %12s %10s\n",
                "--------------------",
                "------------",
                "--------------",
                "------------",
                "----------");

    char speedup_buf[32];

    std::snprintf(speedup_buf, sizeof(speedup_buf), "1.00x");
    std::printf("%-20s %12.1f %14.3f %12.1f %10s\n",
                "Individual",
                individual.median_ms,
                individual.per_element_ms,
                individual.ops_per_sec,
                speedup_buf);

    double ratio = individual.median_ms / batched.median_ms;
    std::snprintf(speedup_buf, sizeof(speedup_buf), "%.2fx", ratio);
    std::printf("%-20s %12.1f %14.3f %12.1f %10s\n",
                "Batched",
                batched.median_ms,
                batched.per_element_ms,
                batched.ops_per_sec,
                speedup_buf);

    std::printf("\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    auto schema_path = schema_file();

    // Extract schema filename for display
    std::string schema_name = std::filesystem::path(schema_path).filename().string();

    // Warm-up (results discarded)
    std::printf("Running warm-up: individual...\n");
    std::fflush(stdout);
    run_individual(schema_path, ELEMENT_COUNT);

    std::printf("Running warm-up: batched...\n");
    std::fflush(stdout);
    run_batched(schema_path, ELEMENT_COUNT);

    // Collect individual times
    std::vector<double> individual_times;
    for (int i = 1; i <= ITERATIONS; ++i) {
        std::printf("Running: individual [%d/%d]...\n", i, ITERATIONS);
        std::fflush(stdout);
        individual_times.push_back(run_individual(schema_path, ELEMENT_COUNT));
    }

    // Collect batched times
    std::vector<double> batched_times;
    for (int i = 1; i <= ITERATIONS; ++i) {
        std::printf("Running: batched [%d/%d]...\n", i, ITERATIONS);
        std::fflush(stdout);
        batched_times.push_back(run_batched(schema_path, ELEMENT_COUNT));
    }

    // Compute and print results
    auto individual_stats = compute_stats(individual_times, ELEMENT_COUNT);
    auto batched_stats = compute_stats(batched_times, ELEMENT_COUNT);

    print_results(individual_stats, batched_stats, ELEMENT_COUNT, TS_ROWS_PER_ELEMENT, ITERATIONS, schema_name);

    return 0;
}
