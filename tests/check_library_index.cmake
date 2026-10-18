execute_process(
    COMMAND ${PYTHON} ${INDEXER} --music ${MUSIC} --output ${OUTPUT}
    RESULT_VARIABLE indexed
    OUTPUT_QUIET
)
if(NOT indexed EQUAL 0)
    message(FATAL_ERROR "the indexer failed on ${MUSIC}")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${OUTPUT} ${EXPECTED}
    RESULT_VARIABLE different
)
if(NOT different EQUAL 0)
    message(FATAL_ERROR "out of date: ${EXPECTED}")
endif()
