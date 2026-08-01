if(NOT DEFINED PROJECT_ROOT)
    get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
elseif(NOT IS_ABSOLUTE "${PROJECT_ROOT}")
    get_filename_component(_default_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    get_filename_component(PROJECT_ROOT "${_default_root}/${PROJECT_ROOT}" ABSOLUTE)
endif()

if(NOT DEFINED BUILD_DIR)
    set(BUILD_DIR "${PROJECT_ROOT}/build")
endif()

message(STATUS "Release readiness gate: starting unified checks")

execute_process(
    COMMAND ${CMAKE_COMMAND}
            --build ${BUILD_DIR}
            --target pre_go_live_check
            --config Release
    WORKING_DIRECTORY ${PROJECT_ROOT}
    RESULT_VARIABLE gate_rc
)

if(NOT gate_rc EQUAL 0)
    message(FATAL_ERROR "Release readiness gate failed (pre_go_live_check did not pass)")
endif()

message(STATUS "Release readiness gate: automated checks passed")
message(STATUS "Release readiness gate: mandatory certification checks satisfied")
