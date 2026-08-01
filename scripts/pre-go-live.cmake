if(NOT DEFINED PROJECT_ROOT)
    get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
elseif(NOT IS_ABSOLUTE "${PROJECT_ROOT}")
    get_filename_component(_default_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    get_filename_component(PROJECT_ROOT "${_default_root}/${PROJECT_ROOT}" ABSOLUTE)
endif()

if(NOT DEFINED BUILD_DIR)
    set(BUILD_DIR "${PROJECT_ROOT}/build")
endif()

# Create logs directory if it doesn't exist
file(MAKE_DIRECTORY "${PROJECT_ROOT}/logs")

# Generate timestamped log filename
string(TIMESTAMP timestamp "%Y%m%d_%H%M%S")
set(log_file "${PROJECT_ROOT}/logs/pre_go_live_${timestamp}.log")

# Helper macro to log message to both console and file
macro(log_msg msg)
    message(STATUS "${msg}")
    file(APPEND "${log_file}" "${msg}\n")
endmacro()

# Header
log_msg("===============================================================================")
log_msg("PRE-GO-LIVE UNIFIED VALIDATION")
log_msg("Timestamp: ${timestamp}")
log_msg("Project root: ${PROJECT_ROOT}")
log_msg("Build dir: ${BUILD_DIR}")
log_msg("Log file: ${log_file}")
log_msg("===============================================================================")

# Initialize tracking
set(phase_results "")
set(total_passed 0)
set(total_failed 0)
set(total_pending 0)

# ============================================================================
# PHASE 1: Initialize production configs
# ============================================================================
log_msg("")
log_msg("PHASE 1: Initialize production config stubs")
log_msg("----------")

execute_process(
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${PROJECT_ROOT}
            -P ${PROJECT_ROOT}/scripts/init-prod-configs.cmake
    WORKING_DIRECTORY ${PROJECT_ROOT}
    RESULT_VARIABLE phase1_rc
    OUTPUT_VARIABLE phase1_out
    ERROR_VARIABLE phase1_err
)

if(phase1_rc EQUAL 0)
    log_msg("Phase 1 Result: PASS")
    math(EXPR total_passed "${total_passed}+1")
else()
    log_msg("Phase 1 Result: FAIL (rc=${phase1_rc})")
    math(EXPR total_failed "${total_failed}+1")
endif()

foreach(line IN LISTS phase1_out phase1_err)
    if(NOT line STREQUAL "")
        log_msg("  ${line}")
    endif()
endforeach()

# ============================================================================
# PHASE 2: Preflight production config validation
# ============================================================================
log_msg("")
log_msg("PHASE 2: Validate production config readiness")
log_msg("----------")

execute_process(
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${PROJECT_ROOT}
            -DCONFIG_PATHS=config/miner-prod-cp1.local.json\;config/miner-prod-cp2.local.json\;config/miner-prod-cp3.local.json
            -P ${PROJECT_ROOT}/scripts/preflight-prod-configs.cmake
    WORKING_DIRECTORY ${PROJECT_ROOT}
    RESULT_VARIABLE phase2_rc
    OUTPUT_VARIABLE phase2_out
    ERROR_VARIABLE phase2_err
)

if(phase2_rc EQUAL 0)
    log_msg("Phase 2 Result: PASS")
    math(EXPR total_passed "${total_passed}+1")
else()
    log_msg("Phase 2 Result: FAIL (rc=${phase2_rc})")
    math(EXPR total_failed "${total_failed}+1")
endif()

foreach(line IN LISTS phase2_out phase2_err)
    if(NOT line STREQUAL "")
        log_msg("  ${line}")
    endif()
endforeach()

# ============================================================================
# PHASE 3: Final certification checks
# ============================================================================
log_msg("")
log_msg("PHASE 3: Final certification checks (build + tests + manual items)")
log_msg("----------")

execute_process(
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${PROJECT_ROOT}
            -DBUILD_DIR=${BUILD_DIR}
            -P ${PROJECT_ROOT}/scripts/phase2-cert.cmake
    WORKING_DIRECTORY ${PROJECT_ROOT}
    RESULT_VARIABLE phase3_rc
    OUTPUT_VARIABLE phase3_out
    ERROR_VARIABLE phase3_err
)

# Parse Phase 3 output for individual test results
set(phase3_passed 0)
set(phase3_failed 0)
set(phase3_pending 0)

foreach(line IN LISTS phase3_out phase3_err)
    if(line MATCHES "^.*PASS.*$")
        math(EXPR phase3_passed "${phase3_passed}+1")
    elseif(line MATCHES "^.*FAIL.*$")
        math(EXPR phase3_failed "${phase3_failed}+1")
    elseif(line MATCHES "^.*PENDING.*$")
        math(EXPR phase3_pending "${phase3_pending}+1")
    endif()
    if(NOT line STREQUAL "")
        log_msg("  ${line}")
    endif()
endforeach()

# Phase 3 status depends on whether automated checks passed
if(phase3_rc EQUAL 0)
    log_msg("Phase 3 Result: PASS (automated mandatory checks passed)")
    math(EXPR total_passed "${total_passed}+1")
    math(EXPR total_pending "${total_pending}+${phase3_pending}")
else()
    log_msg("Phase 3 Result: FAIL (rc=${phase3_rc})")
    math(EXPR total_failed "${total_failed}+1")
endif()

# ============================================================================
# OVERALL SUMMARY
# ============================================================================
log_msg("")
log_msg("===============================================================================")
log_msg("OVERALL RESULTS")
log_msg("===============================================================================")
log_msg("Phases passed:  ${total_passed}")
log_msg("Phases failed:  ${total_failed}")
log_msg("Items pending:  ${total_pending}")

if(total_failed EQUAL 0)
    if(total_pending GREATER 0)
        log_msg("")
        log_msg("STATUS: READY FOR GO-LIVE (with manual validation)")
        log_msg("ACTION: Complete remaining pending validation items before deployment")
    else()
        log_msg("")
        log_msg("STATUS: READY FOR GO-LIVE")
        log_msg("ACTION: Proceed with deployment")
    endif()
    log_msg("")
    log_msg("Log file saved to: ${log_file}")
    log_msg("===============================================================================")
else()
    log_msg("")
    log_msg("STATUS: NOT READY FOR GO-LIVE")
    log_msg("ACTION: Fix failures above and re-run this validation")
    log_msg("")
    log_msg("Log file saved to: ${log_file}")
    log_msg("===============================================================================")
    message(FATAL_ERROR "Pre-go-live validation FAILED")
endif()
