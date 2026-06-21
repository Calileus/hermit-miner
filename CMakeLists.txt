cmake_minimum_required(VERSION 3.16)
project(i_mine_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

add_custom_target(init_prod_configs
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/scripts/init-prod-configs.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Create local production config stubs if missing"
)

add_custom_target(preflight_prod_configs
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${CMAKE_SOURCE_DIR}
            -DCONFIG_PATHS=config/miner-prod-cp1.local.json\;config/miner-prod-cp2.local.json\;config/miner-prod-cp3.local.json
            -P ${CMAKE_SOURCE_DIR}/scripts/preflight-prod-configs.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Validate production config readiness"
)

add_custom_target(phase2_cert
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${CMAKE_SOURCE_DIR}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_SOURCE_DIR}/scripts/phase2-cert.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Run Phase 2 final certification checks"
)

add_custom_target(pre_go_live_check
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${CMAKE_SOURCE_DIR}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_SOURCE_DIR}/scripts/pre-go-live.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Run unified pre-go-live validation (all phases with detailed logs)"
)

# Library components
add_library(i_mine_lib
    src/lib/job.cpp
    src/lib/logger.cpp
    src/lib/stratum.cpp
    src/sha256.cpp
)

target_include_directories(i_mine_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Standalone independent miner executable
add_executable(i_mine
    src/miner.cpp
)
target_link_libraries(i_mine i_mine_lib)

add_executable(i_mine_fake_pool
    src/fake_pool.cpp
)

if(WIN32)
    target_link_libraries(i_mine ws2_32)
    target_link_libraries(i_mine_fake_pool ws2_32)
endif()

if(MSVC)
    target_compile_options(i_mine_lib PRIVATE /W4)
    target_compile_options(i_mine PRIVATE /W4)
    target_compile_options(i_mine_fake_pool PRIVATE /W4)
else()
    target_compile_options(i_mine_lib PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(i_mine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(i_mine_fake_pool PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(i_mine_tests
        src/test/local_cert_tests.cpp
    )
    target_link_libraries(i_mine_tests PRIVATE i_mine_lib GTest::gtest_main)
    target_compile_definitions(i_mine_tests PRIVATE IMINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

    if(WIN32)
        target_link_libraries(i_mine_tests PRIVATE ws2_32)
    endif()

    if(MSVC)
        target_compile_options(i_mine_tests PRIVATE /W4)
    else()
        target_compile_options(i_mine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    include(GoogleTest)
    gtest_discover_tests(i_mine_tests)
endif()
