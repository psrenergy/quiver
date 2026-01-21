include(GoogleTest)

add_executable(margaux_tests
    test_database_create.cpp
    test_database_delete.cpp
    test_database_errors.cpp
    test_database_lifecycle.cpp
    test_database_read.cpp
    test_database_relations.cpp
    test_database_update.cpp
    test_element.cpp
    test_lua_runner.cpp
    test_migrations.cpp
    test_row_result.cpp
    test_schema_validator.cpp
)

target_link_libraries(margaux_tests
    PRIVATE
        margaux
        margaux_compiler_options
        GTest::gtest_main
        GTest::gmock
)

# Copy DLLs to output directory for test discovery
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET margaux_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:margaux>
            $<TARGET_FILE_DIR:margaux_tests>
    )
endif()

gtest_discover_tests(margaux_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:margaux_tests>
)

# C API tests
if(MARGAUX_BUILD_C_API)
    add_executable(margaux_c_tests
        test_c_api_database_create.cpp
        test_c_api_database_delete.cpp
        test_c_api_database_lifecycle.cpp
        test_c_api_database_read.cpp
        test_c_api_database_update.cpp
        test_c_api_element.cpp
        test_c_api_lua_runner.cpp
    )

    target_link_libraries(margaux_c_tests
        PRIVATE
            margaux_c
            margaux
            margaux_compiler_options
            GTest::gtest_main
    )

    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET margaux_c_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:margaux_c>
                $<TARGET_FILE_DIR:margaux_c_tests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:margaux>
                $<TARGET_FILE_DIR:margaux_c_tests>
        )
    endif()

    gtest_discover_tests(margaux_c_tests
        WORKING_DIRECTORY $<TARGET_FILE_DIR:margaux_c_tests>
    )
endif()
