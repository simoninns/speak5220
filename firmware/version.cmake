# Writes version.h with the firmware's git description, at every build
# (the file is only rewritten when the description changes)

execute_process(
    COMMAND git describe --always --dirty --abbrev=7
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_DESCRIBE)
    set(GIT_DESCRIBE "unknown")
endif()

set(CONTENT "#define FIRMWARE_VERSION \"${GIT_DESCRIBE}\"\n")
if(EXISTS ${OUTPUT})
    file(READ ${OUTPUT} OLD)
endif()
if(NOT "${OLD}" STREQUAL "${CONTENT}")
    file(WRITE ${OUTPUT} "${CONTENT}")
endif()
