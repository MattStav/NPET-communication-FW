execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Current git tag: ${GIT_TAG}")
file(WRITE ${OUTPUT_FILE}
        "#pragma once
#define GIT_TAG \"${GIT_TAG}\"
")