include(GoogleTest)

add_executable(psr_database_tests
    test_database.cpp
)

target_link_libraries(psr_database_tests
    PRIVATE
        psr_database
        psr_compiler_options
        GTest::gtest_main
        GTest::gmock
)

# Use PRE_TEST mode to avoid DLL path issues during build on Windows
gtest_discover_tests(psr_database_tests
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DISCOVERY_MODE PRE_TEST
)

# C API tests
if(PSR_BUILD_C_API)
    add_executable(psr_database_c_tests
        test_c_api.cpp
    )

    target_link_libraries(psr_database_c_tests
        PRIVATE
            psr_database_c
            psr_compiler_options
            GTest::gtest_main
    )

    gtest_discover_tests(psr_database_c_tests
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        DISCOVERY_MODE PRE_TEST
    )
endif()
