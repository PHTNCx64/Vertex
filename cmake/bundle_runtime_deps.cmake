cmake_minimum_required(VERSION 3.30)

if(NOT DEFINED TARGET_FILE)
    message(FATAL_ERROR "bundle_runtime_deps.cmake: TARGET_FILE is required")
endif()

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "bundle_runtime_deps.cmake: OUTPUT_DIR is required")
endif()

set(PRE_EXCLUDE
    "^ld-linux.*\\.so.*"
    "^libc\\.so.*"
    "^libm\\.so.*"
    "^libdl\\.so.*"
    "^libpthread\\.so.*"
    "^librt\\.so.*"
    "^libstdc\\+\\+\\.so.*"
    "^libgcc_s\\.so.*"
    "^libresolv\\.so.*"
    "^libutil\\.so.*"
    "^libnsl\\.so.*"
    "^libcrypt\\.so.*"
    "^libanl\\.so.*"
    "^libpython.*\\.so.*"
    "^linux-vdso.*"
)

set(POST_EXCLUDE
    "^/lib/.*"
    "^/lib64/.*"
    "^/usr/lib/x86_64-linux-gnu/lib(c|m|dl|pthread|rt|stdc\\+\\+|gcc_s|resolv|util|nsl|crypt|anl|python).*"
)

file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${TARGET_FILE}"
    RESOLVED_DEPENDENCIES_VAR RESOLVED
    UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED
    CONFLICTING_DEPENDENCIES_PREFIX CONFLICTS
    PRE_EXCLUDE_REGEXES ${PRE_EXCLUDE}
    POST_EXCLUDE_REGEXES ${POST_EXCLUDE}
)

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

foreach(dep IN LISTS RESOLVED)
    get_filename_component(depReal "${dep}" REALPATH)
    get_filename_component(depName "${dep}" NAME)
    get_filename_component(realName "${depReal}" NAME)

    file(COPY "${depReal}" DESTINATION "${OUTPUT_DIR}" FOLLOW_SYMLINK_CHAIN)

    if(NOT depName STREQUAL realName)
        set(linkDest "${OUTPUT_DIR}/${depName}")
        if(NOT EXISTS "${linkDest}")
            file(CREATE_LINK "${realName}" "${linkDest}" SYMBOLIC)
        endif()
    endif()

    message(STATUS "[bundle] ${depName} -> ${OUTPUT_DIR}")
endforeach()

foreach(u IN LISTS UNRESOLVED)
    message(WARNING "[bundle] unresolved dependency: ${u}")
endforeach()

foreach(prefix IN LISTS CONFLICTS_FILENAMES)
    message(WARNING "[bundle] conflicting dependency filename: ${prefix}")
endforeach()
