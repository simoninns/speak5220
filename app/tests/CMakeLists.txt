# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# Every test target (TESTING.md, "Repository layout"). A test's lane is the
# directory its source is in: a file under unit/ is built into a unit binary
# and labelled "unit", one under functional/ is labelled "functional". There
# is no list of files to keep in step.

find_package(GTest 1.14 REQUIRED)
include(GoogleTest)

add_custom_target(speak5220_tests)

# speak5220_add_test_lane(<target> <directory> LIBRARIES <lib>...)
#
# Builds every .cpp under <directory> into <target>, labels its tests with the
# directory's name (unit or functional), and adds the target to
# speak5220_tests.
function(speak5220_add_test_lane target directory)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "LIBRARIES")
    cmake_path(GET directory FILENAME label)
    if(NOT label MATCHES "^(unit|functional)$")
        message(
            FATAL_ERROR
            "Test directory ${directory} must be named unit or functional"
        )
    endif()

    file(
        GLOB_RECURSE sources
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/*.cpp"
    )
    if(NOT sources)
        message(FATAL_ERROR "Test directory ${directory} has no .cpp files")
    endif()

    add_executable(${target} ${sources})
    target_link_libraries(
        ${target}
        PRIVATE
            ${arg_LIBRARIES}
            speak5220_test_support
            speak5220_compile_options
    )
    add_dependencies(speak5220_tests ${target})

    # PRE_TEST discovery lists the tests when CTest runs, not at build time,
    # so sanitizer and offscreen settings apply to the listing too.
    gtest_discover_tests(
        ${target}
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS ${label}
    )
endfunction()

add_subdirectory(support)

speak5220_add_test_lane(
    speak5220_unit_tests
    unit
    LIBRARIES s5core GTest::gmock_main
)

speak5220_add_test_lane(
    speak5220_functional_tests
    functional
    LIBRARIES s5core Qt6::Core speak5220::sndfile libzip::zip GTest::gmock_main
)
# The functional lane reads the committed reference data in app/golden/ and
# the fixture projects in app/projects/, and runs the CLI end to end
# (TESTING.md).
target_compile_definitions(
    speak5220_functional_tests
    PRIVATE
        SPEAK5220_CLI_PATH="$<TARGET_FILE:speak5220-cli>"
        SPEAK5220_GOLDEN_DIR="${PROJECT_SOURCE_DIR}/golden"
        SPEAK5220_PROJECTS_DIR="${PROJECT_SOURCE_DIR}/projects"
)
add_dependencies(speak5220_functional_tests speak5220-cli)

add_subdirectory(gui)
