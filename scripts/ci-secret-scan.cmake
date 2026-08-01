if(NOT DEFINED PROJECT_ROOT)
    get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
elseif(NOT IS_ABSOLUTE "${PROJECT_ROOT}")
    get_filename_component(_default_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    get_filename_component(PROJECT_ROOT "${_default_root}/${PROJECT_ROOT}" ABSOLUTE)
endif()

set(findings "")

function(add_finding msg)
    set(findings "${findings};${msg}" PARENT_SCOPE)
endfunction()

function(should_skip rel_path out_var)
    if(rel_path MATCHES "^\\.git/" OR
       rel_path MATCHES "^build/" OR
       rel_path MATCHES "^logs/" OR
       rel_path MATCHES "^CMakeFiles/" OR
       rel_path MATCHES "^Testing/")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    if(rel_path MATCHES "^config/.*\\.local\\.json$")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    if(rel_path MATCHES "\\.(exe|dll|lib|obj|o|pdb|ilk|zip|png|jpg|jpeg|gif|ico|pdf)$")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    set(${out_var} FALSE PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE all_files RELATIVE "${PROJECT_ROOT}" "${PROJECT_ROOT}/*")

foreach(rel_path IN LISTS all_files)
    should_skip("${rel_path}" skip)
    if(skip)
        continue()
    endif()

    set(abs_path "${PROJECT_ROOT}/${rel_path}")
    if(IS_DIRECTORY "${abs_path}")
        continue()
    endif()

    file(READ "${abs_path}" content)

    if(content MATCHES "-----BEGIN (RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----")
        add_finding("${rel_path}: private key material detected")
    endif()

    if(content MATCHES "AKIA[0-9A-Z]{16}")
        add_finding("${rel_path}: AWS access key pattern detected")
    endif()

    if(rel_path MATCHES "^config/.*\\.json$")
        string(REGEX MATCHALL "\\\"password\\\"[ \t\r\n]*:[ \t\r\n]*\\\"[^\\\"]*\\\"" password_pairs "${content}")
        foreach(pair IN LISTS password_pairs)
            string(REGEX MATCH "\\\"password\\\"[ \t\r\n]*:[ \t\r\n]*\\\"([^\\\"]*)\\\"" _ "${pair}")
            set(password_value "${CMAKE_MATCH_1}")
            if(NOT password_value STREQUAL "" AND
               NOT password_value STREQUAL "x" AND
               NOT password_value STREQUAL "***" AND
               NOT password_value MATCHES "^REPLACE_WITH")
                add_finding("${rel_path}: non-placeholder password value committed")
            endif()
        endforeach()
    endif()
endforeach()

list(LENGTH findings finding_count)
if(finding_count GREATER 0)
    message(STATUS "Secret scan findings (${finding_count}):")
    foreach(item IN LISTS findings)
        if(NOT item STREQUAL "")
            message(STATUS " - ${item}")
        endif()
    endforeach()
    message(FATAL_ERROR "Secret scan failed")
endif()

message(STATUS "Secret scan passed: no high-confidence secrets detected.")
