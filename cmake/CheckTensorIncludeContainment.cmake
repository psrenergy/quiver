# cmake/CheckTensorIncludeContainment.cmake
# BLD-04: enforces include containment -- no public Quiver header outside
# include/quiver/tensor/ may transitively include a quiver/tensor/*.h header.
# Mitigates Pitfall #4 (header-only template heaviness leaking library-wide).
#
# Run via: cmake -P cmake/CheckTensorIncludeContainment.cmake
# Wired by: tests/CMakeLists.txt as add_test(NAME tensor_no_leakage COMMAND ${CMAKE_COMMAND} -P ...).
#
# Requires CMake 3.17+ for continue() inside foreach (project minimum is 3.26
# per top-level CMakeLists.txt cmake_minimum_required(VERSION 3.26)).

# CMake scripts run with no project context; reconstruct the include scope from
# the script's path. CMAKE_CURRENT_LIST_FILE is the absolute path of THIS .cmake file.
get_filename_component(SCRIPT_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
set(INCLUDE_DIR "${SCRIPT_DIR}/../include/quiver")

# D-17: scope is include/quiver/ recursive, EXCLUDING include/quiver/tensor/.
file(GLOB_RECURSE ALL_HEADERS RELATIVE "${INCLUDE_DIR}" "${INCLUDE_DIR}/*.h")

set(VIOLATIONS "")
foreach(HEADER ${ALL_HEADERS})
    # D-17: exclude the tensor subdirectory itself.
    if(HEADER MATCHES "^tensor/")
        continue()
    endif()

    set(FULL_PATH "${INCLUDE_DIR}/${HEADER}")

    # D-16: regex matches both <quiver/tensor/...> and "quiver/tensor/..." forms.
    # Note: file(STRINGS REGEX) returns matching lines, not a count; we use it
    # as a presence check, then re-iterate for line numbers if matched.
    file(STRINGS "${FULL_PATH}" MATCHED_LINES REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]quiver/tensor/")

    if(MATCHED_LINES)
        # Re-read with line numbers to produce a useful diagnostic.
        file(STRINGS "${FULL_PATH}" ALL_LINES)
        set(LINE_NUM 0)
        foreach(LINE ${ALL_LINES})
            math(EXPR LINE_NUM "${LINE_NUM}+1")
            if(LINE MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]quiver/tensor/")
                list(APPEND VIOLATIONS "  ${HEADER}:${LINE_NUM}: ${LINE}")
            endif()
        endforeach()
    endif()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n" VIOLATIONS_STR "${VIOLATIONS}")
    message(FATAL_ERROR
        "BLD-04 violation: non-tensor public header(s) in include/quiver/ pull in quiver/tensor/.\n"
        "Tensor headers MUST stay opt-in for users who include them directly.\n"
        "Allowing transitive pull-in causes header-only template heaviness to spread to every translation\n"
        "unit that touches database.h, element.h, binary/*.h, etc. (Pitfall #4).\n"
        "Offending lines:\n${VIOLATIONS_STR}\n")
endif()

message(STATUS "BLD-04: include containment OK -- no public header transitively includes quiver/tensor/.")
