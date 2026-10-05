include(GoogleTest)

add_executable(quiver_tests
    test_binary_file.cpp
    test_binary_metadata.cpp
    test_binary_time_properties.cpp
    test_csv_converter.cpp
    test_database_create.cpp
    test_database_csv_export.cpp
    test_database_csv_import.cpp
    test_database_delete.cpp
    test_database_describe.cpp
    test_database_errors.cpp
    test_database_lifecycle.cpp
    test_database_metadata.cpp
    test_database_query.cpp
    test_database_read_scalar.cpp
    test_database_read_set.cpp
    test_database_read_vector.cpp
    test_database_time_series_files.cpp
    test_database_time_series_group.cpp
    test_database_time_series_metadata.cpp
    test_database_time_series_row.cpp
    test_database_transaction.cpp
    test_database_ui_metadata.cpp
    test_database_update.cpp
    test_element.cpp
    test_expression.cpp
    test_issues.cpp
    test_iteration.cpp
    test_migrations.cpp
    test_row_result.cpp
    test_sandbox_all_types.cpp
    test_sandbox_binary.cpp
    test_sandbox_create.cpp
    test_sandbox_csv_export.cpp
    test_sandbox_csv_import.cpp
    test_sandbox_delete.cpp
    test_sandbox_describe.cpp
    test_sandbox_errors.cpp
    test_sandbox_expression.cpp
    test_sandbox_fk.cpp
    test_sandbox_lifecycle.cpp
    test_sandbox_migrations.cpp
    test_sandbox_query.cpp
    test_sandbox_read_csv.cpp
    test_sandbox_read.cpp
    test_sandbox_return.cpp
    test_sandbox_time_series.cpp
    test_sandbox_transaction.cpp
    test_sandbox_update.cpp
    test_sandbox_write_csv.cpp
    test_sandboxed_path.cpp
    test_schema_validator.cpp
    # resolve_sandboxed_path is internal to quiver (hidden, not exported), so SandboxedPathTest
    # compiles its own copy. Keep path_policy.cpp to that one function: anything else the library
    # also uses would be defined twice in a static (QUIVER_BUILD_SHARED=OFF) link.
    ${CMAKE_SOURCE_DIR}/src/sandbox/path_policy.cpp
)

target_link_libraries(quiver_tests
    PRIVATE
        quiver
        quiver_compiler_options
        GTest::gtest_main
        GTest::gmock
)

# path_policy.cpp and the test include "sandbox/path_policy.h".
target_include_directories(quiver_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

gtest_discover_tests(quiver_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:quiver_tests>
)

# C API tests
if(QUIVER_BUILD_C_API)
    add_executable(quiver_c_tests
        test_c_api_binary_file.cpp
        test_c_api_csv_converter.cpp
        test_c_api_binary_metadata.cpp
        test_c_api_expression.cpp
        test_c_api_database_create.cpp
        test_c_api_database_csv_export.cpp
        test_c_api_database_csv_import.cpp
        test_c_api_database_delete.cpp
        test_c_api_database_lifecycle.cpp
        test_c_api_database_metadata.cpp
        test_c_api_database_query.cpp
        test_c_api_database_read_scalar.cpp
        test_c_api_database_read_set.cpp
        test_c_api_database_read_vector.cpp
        test_c_api_database_time_series_files.cpp
        test_c_api_database_time_series_group.cpp
        test_c_api_database_time_series_metadata.cpp
        test_c_api_database_time_series_nulls.cpp
        test_c_api_database_time_series_row.cpp
        test_c_api_database_transaction.cpp
        test_c_api_database_update.cpp
        test_c_api_element.cpp
        test_c_api_sandbox.cpp
    )

    target_link_libraries(quiver_c_tests
        PRIVATE
            quiver_c
            quiver
            quiver_compiler_options
            GTest::gtest_main
    )

    gtest_discover_tests(quiver_c_tests
        WORKING_DIRECTORY $<TARGET_FILE_DIR:quiver_c_tests>
        DISCOVERY_TIMEOUT 60
    )
endif()

# Benchmark executable (not part of test suite)
add_executable(quiver_benchmark
    benchmark/benchmark.cpp
)

# Sandbox executable (not part of test suite)
add_executable(quiver_sandbox
    sandbox/sandbox.cpp
)

target_link_libraries(quiver_sandbox
    PRIVATE
        quiver
        quiver_compiler_options
)

target_link_libraries(quiver_benchmark
    PRIVATE
        quiver
        quiver_compiler_options
)
