if (NOT DEFINED VCPKG_INSTALLED_DIR)
    message(FATAL_ERROR "VCPKG_INSTALLED_DIR must be provided")
endif ()

if (NOT DEFINED OUTPUT_FILE)
    set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/THIRD_PARTY_NOTICES.txt")
endif ()

file(WRITE ${OUTPUT_FILE} "THIRD PARTY SOFTWARE LICENSES\n\n")

file(GLOB_RECURSE LICENSE_FILES "${VCPKG_INSTALLED_DIR}/*/copyright")

if (NOT LICENSE_FILES)
    message(FATAL_ERROR "No copyright files found under ${VCPKG_INSTALLED_DIR} — did vcpkg install run?")
endif ()

foreach(LICENSE ${LICENSE_FILES})
    get_filename_component(PACKAGE_DIR ${LICENSE} DIRECTORY)
    get_filename_component(PACKAGE_NAME ${PACKAGE_DIR} NAME)
    file(READ ${LICENSE} CONTENT)
    file(APPEND ${OUTPUT_FILE} "\n==============================\n${PACKAGE_NAME}\n==============================\n\n${CONTENT}\n")
endforeach()

message(STATUS "Generated ${OUTPUT_FILE}")