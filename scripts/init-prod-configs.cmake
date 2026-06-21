if(NOT DEFINED PROJECT_ROOT)
    get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

set(template_path "${PROJECT_ROOT}/config/miner-production.template.json")
if(NOT EXISTS "${template_path}")
    message(FATAL_ERROR "Missing template config: ${template_path}")
endif()

file(READ "${template_path}" template_json)

set(config_specs
    "CP-1,cp1,miner-prod-cp1.local.json"
    "CP-2,cp2,miner-prod-cp2.local.json"
    "CP-3,cp3,miner-prod-cp3.local.json"
)

foreach(spec IN LISTS config_specs)
    string(REGEX MATCH "^([^,]+),([^,]+),([^,]+)$" _ "${spec}")
    set(node_id "${CMAKE_MATCH_1}")
    set(worker_id "${CMAKE_MATCH_2}")
    set(filename "${CMAKE_MATCH_3}")

    set(out_path "${PROJECT_ROOT}/config/${filename}")
    if(EXISTS "${out_path}")
        message(STATUS "Keeping existing config: ${out_path}")
        continue()
    endif()

    set(content "${template_json}")
    string(REPLACE "\"CP-PROD-01\"" "\"${node_id}\"" content "${content}")
    string(REPLACE "\"prod01\"" "\"${worker_id}\"" content "${content}")
    string(REPLACE "REPLACE_WITH_REAL_WALLET_ADDRESS.prod01" "REPLACE_WITH_REAL_WALLET_ADDRESS.${worker_id}" content "${content}")
    string(REPLACE "miner-prod01.log" "miner-${worker_id}.log" content "${content}")

    file(WRITE "${out_path}" "${content}")
    message(STATUS "Created config stub: ${out_path}")
endforeach()

message(STATUS "Production config initialization complete.")