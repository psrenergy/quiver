include(GoogleTest)

add_executable(quiver_tests
    test_database_create.cpp
    test_database_csv.cpp
    test_database_delete.cpp
    test_database_errors.cpp
    test_database_lifecycle.cpp
    test_database_query.cpp
    test_database_read.cpp
    test_database_relations.cpp
    test_database_time_series.cpp
    test_database_transaction.cpp
    test_database_update.cpp
    test_element.cpp
    test_issues.cpp
    test_lua_runner.cpp
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

# Copy DLLs to output directory for test discovery
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET quiver_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:quiver>
            $<TARGET_FILE_DIR:quiver_tests>
    )
endif()

gtest_discover_tests(quiver_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:quiver_tests>
)

# C API tests
if(QUIVER_BUILD_C_API)
    add_executable(quiver_c_tests
        test_c_api_database_create.cpp
        test_c_api_database_delete.cpp
        test_c_api_database_lifecycle.cpp
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

    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET quiver_c_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:quiver_c>
                $<TARGET_FILE_DIR:quiver_c_tests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:quiver>
                $<TARGET_FILE_DIR:quiver_c_tests>
        )
    endif()

    gtest_discover_tests(quiver_c_tests
        WORKING_DIRECTORY $<TARGET_FILE_DIR:quiver_c_tests>
    )
endif()

# Benchmark executable (not part of test suite)
add_executable(quiver_benchmark
    benchmark/benchmark.cpp
)

target_link_libraries(quiver_benchmark
    PRIVATE
        quiver
        quiver_compiler_options
)

# Copy DLLs for benchmark
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET quiver_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:quiver>
            $<TARGET_FILE_DIR:quiver_benchmark>
    )
endif()
