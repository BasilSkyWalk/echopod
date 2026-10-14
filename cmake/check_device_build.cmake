foreach(step "--preset;device" "--build;--preset;device")
    execute_process(COMMAND ${CMAKE_COMMAND} ${step}
        WORKING_DIRECTORY ${SOURCE_DIR}
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "cmake ${step} failed")
    endif()
endforeach()
