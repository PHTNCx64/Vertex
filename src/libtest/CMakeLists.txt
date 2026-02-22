#-----------------------------
#  Source Files (Platform-independent)
#-----------------------------
set(SOURCES)

#-----------------------------
#  Platform Configuration
#-----------------------------
if(WIN32)
    list(APPEND SOURCES
        windows/main.cc
    )
elseif(APPLE)
    # list(APPEND SOURCES macos/main.cc)
elseif(UNIX)
    # list(APPEND SOURCES linux/main.cc)
endif()

message(STATUS "Building libtest (x64)")

#-----------------------------
#  Library Target
#-----------------------------
add_library(libtest SHARED ${SOURCES})

target_include_directories(libtest PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

configure_target(libtest)

set_target_properties(libtest PROPERTIES
    OUTPUT_NAME "libtest"
    FOLDER "Libraries"
)

#-----------------------------
#  Post-Build: Copy to Output
#-----------------------------
add_custom_command(TARGET libtest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:libtest>
        "${CMAKE_BINARY_DIR}"
    COMMENT "Copying libtest DLL to build directory"
)

#-----------------------------
#  Post-Build: Copy to Plugins
#-----------------------------
set(PLUGINS_DIR "${CMAKE_BINARY_DIR}/Plugins")

add_custom_command(TARGET libtest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libtest>
            "${PLUGINS_DIR}/"
    COMMENT "Copying libtest DLL to Plugins folder"
)

if(WIN32 AND MSVC)
    add_custom_command(TARGET libtest POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSRC=$<TARGET_PDB_FILE:libtest>
            -DDST=${PLUGINS_DIR}/
            -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_exists.cmake
        COMMENT "Copying libtest debug symbols to Plugins folder"
    )
endif()
