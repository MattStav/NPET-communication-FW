include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

# Tests for the NPET comm FW framework
file(GLOB_RECURSE NPET_FRAMEWORK_TESTS_FILES "tests_framework/*.cpp")
add_executable(NPET_comm_FW_framework_tests
        ${NPET_FRAMEWORK_TESTS_FILES}
)
target_compile_definitions(NPET_comm_FW_framework_tests PRIVATE
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF
)
target_link_libraries(NPET_comm_FW_framework_tests PRIVATE
        NPET_comm_FW_framework_silent
        GTest::gmock
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(NPET_comm_FW_framework_tests)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    function(enable_coverage target)
        target_compile_options(${target} PRIVATE
                -O0
                -g
                --coverage
        )
        target_link_options(${target} PRIVATE
                --coverage
        )
    endfunction()
    enable_coverage(NPET_comm_FW_framework_tests)
    enable_coverage(NPET_comm_FW_framework_silent)
endif()