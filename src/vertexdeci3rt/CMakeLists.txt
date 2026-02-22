#-----------------------------
#  Source Files
#-----------------------------
set(SOURCES
    main.cc
    init.cc
    process/close.cc
    process/is_valid.cc
    process/kill.cc
    process/open.cc
    process/open_new.cc
    process/get_list.cc
    process/get_modules_list.cc
    process/get_module_imports.cc
    process/get_module_exports.cc
    process/get_executable_extensions.cc
    process/get_library_extensions.cc
    process/get_injection_methods.cc
        ${CMAKE_SOURCE_DIR}/include/vertexdeci3rt/init.hh
    ${CMAKE_SOURCE_DIR}/include/vertexdeci3rt/config.hh
    ${CMAKE_SOURCE_DIR}/include/sdk/api.h
)

#-----------------------------
#  Library Target
#-----------------------------
add_library(vertexdeci3rt SHARED ${SOURCES})

target_include_directories(vertexdeci3rt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

configure_target(vertexdeci3rt SUBDIR Plugins)

#-----------------------------
#  TMAPI SDK
#-----------------------------
if(WIN32)
    set(SN_PS3_PATH "$ENV{SN_PS3_PATH}")
    if(NOT SN_PS3_PATH)
        message(FATAL_ERROR "SN_PS3_PATH environment variable is not set. Install the TMAPI SDK.")
    endif()

    target_include_directories(vertexdeci3rt PRIVATE
        "${SN_PS3_PATH}/sdk/include"
    )

    set(TMAPI_LIB "${SN_PS3_PATH}/sdk/lib/PS3TMAPIx64.lib")

    target_link_libraries(vertexdeci3rt PRIVATE "${TMAPI_LIB}")
endif()

set_target_properties(vertexdeci3rt PROPERTIES
    OUTPUT_NAME "vertexdeci3rt"
    FOLDER "Runtime"
)
