if(NOT DEFINED SOURCE OR NOT DEFINED DEST)
    message(FATAL_ERROR "SOURCE and DEST must be provided")
endif()

get_filename_component(DEST_DIR "${DEST}" DIRECTORY)
file(MAKE_DIRECTORY "${DEST_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SOURCE}" "${DEST}"
    RESULT_VARIABLE COPY_RESULT
    OUTPUT_VARIABLE COPY_OUTPUT
    ERROR_VARIABLE COPY_ERROR
)

if(COPY_RESULT EQUAL 0)
    message(STATUS "Copied ${SOURCE} to ${DEST}")
endif()
