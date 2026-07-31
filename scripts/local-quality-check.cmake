if(NOT DEFINED PROJECT_ROOT)
    get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

if(NOT DEFINED BUILD_DIR)
    set(BUILD_DIR "${PROJECT_ROOT}/build")
endif()

if(NOT DEFINED CFG)
    set(CFG "Release")
endif()

message(STATUS "Local quality check: secret scan")
execute_process(
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${PROJECT_ROOT}
            -P ${PROJECT_ROOT}/scripts/ci-secret-scan.cmake
    WORKING_DIRECTORY ${PROJECT_ROOT}
    RESULT_VARIABLE scan_rc
)
if(NOT scan_rc EQUAL 0)
    message(FATAL_ERROR "Local quality check failed at secret scan")
endif()

message(STATUS "Local quality check: build (${CFG})")
execute_process(
    COMMAND ${CMAKE_COMMAND}
            --build ${BUILD_DIR}
            --config ${CFG}
    WORKING_DIRECTORY ${PROJECT_ROOT}
    RESULT_VARIABLE build_rc
)
if(NOT build_rc EQUAL 0)
    message(FATAL_ERROR "Local quality check failed at build")
endif()

message(STATUS "Local quality check: tests (${CFG})")
execute_process(
    COMMAND ${CMAKE_CTEST_COMMAND}
            --test-dir ${BUILD_DIR}
            -C ${CFG}
            --output-on-failure
            --timeout 180
    WORKING_DIRECTORY ${PROJECT_ROOT}
    RESULT_VARIABLE test_rc
)
if(NOT test_rc EQUAL 0)
    message(FATAL_ERROR "Local quality check failed at tests")
endif()

message(STATUS "Local quality check passed")
