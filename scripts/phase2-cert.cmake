if(NOT DEFINED PROJECT_ROOT)
    get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
elseif(NOT IS_ABSOLUTE "${PROJECT_ROOT}")
    get_filename_component(_default_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    get_filename_component(PROJECT_ROOT "${_default_root}/${PROJECT_ROOT}" ABSOLUTE)
endif()

if(NOT DEFINED BUILD_DIR)
    set(BUILD_DIR "${PROJECT_ROOT}/build")
endif()

set(CFG "Release")
set(FAILED 0)

function(report_result key status details)
    message(STATUS "${key}=${status} ${details}")
endfunction()

function(run_step name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs COMMAND)
    cmake_parse_arguments(STEP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    execute_process(
        COMMAND ${STEP_COMMAND}
        WORKING_DIRECTORY "${PROJECT_ROOT}"
        RESULT_VARIABLE step_rc
        OUTPUT_VARIABLE step_out
        ERROR_VARIABLE step_err
    )

    if(NOT step_rc EQUAL 0)
        set(FAILED 1 PARENT_SCOPE)
        string(REPLACE "\n" " | " step_out_inline "${step_out}")
        string(REPLACE "\n" " | " step_err_inline "${step_err}")
        report_result(${name} "FAIL" "rc=${step_rc} out=${step_out_inline} err=${step_err_inline}")
    else()
        report_result(${name} "PASS" "")
    endif()
endfunction()

message(STATUS "PHASE2_RESULT_BEGIN")

# LC-001 Build integrity
run_step("LC-001" COMMAND ${CMAKE_COMMAND} -S . -B "${BUILD_DIR}" -DBUILD_TESTING=ON)
run_step("LC-001" COMMAND ${CMAKE_COMMAND} --build "${BUILD_DIR}" --config ${CFG})

# LC-003/LC-004 baseline via automated tests
run_step("LC-003_LC-004" COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${BUILD_DIR}" -C ${CFG} --output-on-failure)

# Phase-2 mandatory manual checks not fully automated in CMake script.
# We intentionally mark these as PENDING so go/no-go remains strict.
report_result("LC-006" "PENDING" "manual reconnect backoff + recovery still required")
report_result("LC-009" "PENDING" "manual secret redaction sanity still required")

if(FAILED)
    report_result("MANDATORY_ALL" "FAIL" "one or more automated checks failed")
    message(STATUS "PHASE2_RESULT_END")
    message(FATAL_ERROR "Phase 2 certification failed")
endif()

report_result("MANDATORY_ALL" "PENDING" "complete LC-006 and LC-009 manually")
message(STATUS "PHASE2_RESULT_END")