include(GoogleTest)

add_executable(quiver_tests
    test_binary_file.cpp
    test_csv_converter.cpp
    test_binary_metadata.cpp
    test_iteration.cpp
    test_binary_time_properties.cpp
    test_expression.cpp
    test_database_create.cpp
    test_database_csv_export.cpp
    test_database_csv_import.cpp
    test_database_delete.cpp
    test_database_errors.cpp
    test_database_lifecycle.cpp
    test_database_query.cpp
    test_database_read.cpp
    test_database_time_series.cpp
    test_database_transaction.cpp
    test_database_update.cpp
    test_element.cpp
    test_issues.cpp
    test_lua_runner_all_types.cpp
    test_lua_runner_create.cpp
    test_lua_runner_csv_export.cpp
    test_lua_runner_csv_import.cpp
    test_lua_runner_delete.cpp
    test_lua_runner_errors.cpp
    test_lua_runner_fk.cpp
    test_lua_runner_query.cpp
    test_lua_runner_read.cpp
    test_lua_runner_time_series.cpp
    test_lua_runner_transaction.cpp
    test_lua_runner_update.cpp
    test_migrations.cpp
    test_row_result.cpp
    test_schema_validator.cpp
)

target_link_libraries(quiver_tests
    PRIVATE
        quiver
        quiver_compiler_options
        GTest::gtest_main
        GTest::gmock
)

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
        test_c_api_database_read.cpp
        test_c_api_database_time_series.cpp
        test_c_api_database_transaction.cpp
        test_c_api_database_update.cpp
        test_c_api_element.cpp
        test_c_api_lua_runner.cpp
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
