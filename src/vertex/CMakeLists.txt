set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
        ApplicationName="Vertex"
        ApplicationVendor="PHTNC"
        ApplicationVersion="${PROJECT_VERSION}"
)

#-----------------------------
#  Dependencies
#-----------------------------

find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base richtext stc aui)
find_package(mimalloc CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(NanoSVG CONFIG REQUIRED)
find_package(Angelscript CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(hwy CONFIG REQUIRED)


#-----------------------------
#  Plugin Function Auto-Generation
#-----------------------------
set(PLUGIN_FUNCTIONS_HEADER "${CMAKE_BINARY_DIR}/generated/plugin_functions.hh")
set(PLUGIN_MOVE_SEMANTICS_HEADER "${CMAKE_BINARY_DIR}/generated/plugin_move_semantics.hh")
set(PLUGIN_REGISTRATION_HEADER "${CMAKE_BINARY_DIR}/generated/plugin_function_registration.hh")

add_custom_command(
        OUTPUT "${PLUGIN_FUNCTIONS_HEADER}" "${PLUGIN_MOVE_SEMANTICS_HEADER}" "${PLUGIN_REGISTRATION_HEADER}"
        COMMAND ${CMAKE_COMMAND}
        -Dapi_header="${CMAKE_SOURCE_DIR}/include/sdk/api.h"
        -Doutput_header="${PLUGIN_FUNCTIONS_HEADER}"
        -Doutput_move_semantics="${PLUGIN_MOVE_SEMANTICS_HEADER}"
        -Doutput_impl="${PLUGIN_REGISTRATION_HEADER}"
        -P "${CMAKE_SOURCE_DIR}/cmake/gen_plugin_functions.cmake"
        DEPENDS
        "${CMAKE_SOURCE_DIR}/include/sdk/api.h"
        "${CMAKE_SOURCE_DIR}/cmake/gen_plugin_functions.cmake"
        COMMENT "Generating plugin API wrappers (function pointers, move semantics, registry)"
)

add_custom_target(generate_plugin_functions
        DEPENDS "${PLUGIN_FUNCTIONS_HEADER}" "${PLUGIN_MOVE_SEMANTICS_HEADER}" "${PLUGIN_REGISTRATION_HEADER}")

#-----------------------------
#  SVG Icon Generation
#-----------------------------
function(convert_svg_to_header svg output var)
    add_custom_command(
            OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/svg_to_header.cmake" "${svg}" "${output}" "${var}"
            DEPENDS "${svg}"
            COMMENT "Converting ${svg} -> ${output}"
    )
endfunction()

set(GENERATED_HEADERS)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/icons")

foreach (mode lightmode darkmode)
    file(GLOB SVG_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/${mode}/*.svg")
    foreach (svg ${SVG_FILES})
        get_filename_component(name "${svg}" NAME_WE)
        set(out "${CMAKE_BINARY_DIR}/generated/icons/${mode}_${name}.hh")
        convert_svg_to_header("${svg}" "${out}" "icon_${mode}_${name}_svg")
        list(APPEND GENERATED_HEADERS "${out}")
    endforeach ()
endforeach ()

set(ICONS_WRAPPER_HEADER "${CMAKE_BINARY_DIR}/generated/Icons.hh")
add_custom_command(
        OUTPUT "${ICONS_WRAPPER_HEADER}"
        COMMAND ${CMAKE_COMMAND}
        -Dout="${ICONS_WRAPPER_HEADER}"
        -Dsrc_dir="${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/gen_icon_wrapper.cmake"
        DEPENDS ${GENERATED_HEADERS}
        COMMENT "Generating Icons.hh wrapper header"
)
list(APPEND GENERATED_HEADERS "${ICONS_WRAPPER_HEADER}")

set(ICONMAP_HEADER "${CMAKE_BINARY_DIR}/generated/IconMap.hh")
add_custom_command(
        OUTPUT "${ICONMAP_HEADER}"
        COMMAND ${CMAKE_COMMAND}
        -Dout="${ICONMAP_HEADER}"
        -Dsrc_dir="${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/gen_icon_map.cmake"
        DEPENDS ${GENERATED_HEADERS}
        COMMENT "Generating IconMap.hh mapping header"
)
list(APPEND GENERATED_HEADERS "${ICONMAP_HEADER}")

add_custom_target(generate_icon_headers DEPENDS ${GENERATED_HEADERS})
list(LENGTH GENERATED_HEADERS num_icons)
message(STATUS "Embedding ${num_icons} SVG icons")

#-----------------------------
#  Logo Icon Embedding
#-----------------------------
set(LOGO_ICO_PATH "${CMAKE_SOURCE_DIR}/resources/logo/logo.ico")
set(LOGO_HEADER "${CMAKE_BINARY_DIR}/generated/logo.hh")

add_custom_command(
        OUTPUT "${LOGO_HEADER}"
        COMMAND ${CMAKE_COMMAND}
        -DLOGO_ICO_PATH="${LOGO_ICO_PATH}"
        -DOUTPUT_PATH="${LOGO_HEADER}"
        -P "${CMAKE_SOURCE_DIR}/cmake/gen_logo_header.cmake"
        DEPENDS "${LOGO_ICO_PATH}"
        COMMENT "Embedding logo.ico -> logo.hh"
)

add_custom_target(generate_logo_header DEPENDS "${LOGO_HEADER}")

#-----------------------------
#  Source Collection
#-----------------------------
file(GLOB_RECURSE VERTEX_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_SOURCE_DIR}/include/vertex/**/*.hh"
        "${CMAKE_SOURCE_DIR}/include/sdk/*.h"
)

# Exclude all platform-specific directories first, then add back selected platform.
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Ww]indows/.*")
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Ll]inux/.*")
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Dd]arwin/.*")
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Mm]acos/.*")
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/plugin_runtime/.*")

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/**/windows/*.cc"
            "${CMAKE_CURRENT_SOURCE_DIR}/**/Windows/*.cc"
            "${CMAKE_SOURCE_DIR}/include/vertex/**/windows/*.hh"
            "${CMAKE_SOURCE_DIR}/include/vertex/**/Windows/*.hh"
    )
    list(APPEND VERTEX_SOURCES ${PLATFORM_SOURCES})

    set(VERTEX_ICON_PATH "${LOGO_ICO_PATH}")
    configure_file(
            "${CMAKE_SOURCE_DIR}/resources/vertex.rc.in"
            "${CMAKE_BINARY_DIR}/generated/vertex.rc"
    )
    list(APPEND VERTEX_SOURCES "${CMAKE_BINARY_DIR}/generated/vertex.rc")
    message(STATUS "Building for Windows - including Windows-specific sources")
elseif (UNIX AND NOT APPLE)
    file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/**/linux/*.cc"
            "${CMAKE_CURRENT_SOURCE_DIR}/**/Linux/*.cc"
            "${CMAKE_SOURCE_DIR}/include/vertex/**/linux/*.hh"
            "${CMAKE_SOURCE_DIR}/include/vertex/**/Linux/*.hh"
    )
    list(APPEND VERTEX_SOURCES ${PLATFORM_SOURCES})
    message(STATUS "Building for Linux - including Linux-specific sources")
elseif (APPLE)
    file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/**/darwin/*.mm"
            "${CMAKE_CURRENT_SOURCE_DIR}/**/darwin/*.cc"
            "${CMAKE_SOURCE_DIR}/include/vertex/**/macos/*.hh"
            "${CMAKE_SOURCE_DIR}/include/vertex/**/darwin/*.hh"
    )
    list(APPEND VERTEX_SOURCES ${PLATFORM_SOURCES})
    message(STATUS "Building for macOS - including macOS-specific sources")
endif ()

#-----------------------------
#  Main Executable
#-----------------------------
add_executable(Vertex WIN32 ${VERTEX_SOURCES})
add_dependencies(Vertex generate_icon_headers generate_plugin_functions generate_logo_header)

target_include_directories(Vertex PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_BINARY_DIR}/generated"
)

target_link_libraries(Vertex PRIVATE
        mimalloc
        wx::core
        wx::base
        wx::richtext
        wx::stc
        wx::aui
        fmt::fmt
        NanoSVG::nanosvg
        NanoSVG::nanosvgrast
        Angelscript::angelscript
        absl::flat_hash_map
        hwy::hwy
)

if (UNIX AND NOT APPLE)
    target_compile_options(Vertex PRIVATE -fno-omit-frame-pointer)
endif ()

configure_target(Vertex)

if (GENERATED_HEADERS)
    target_compile_definitions(Vertex PRIVATE VERTEX_HAS_GENERATED_ICONS)
endif ()

set_target_properties(Vertex PROPERTIES
        OUTPUT_NAME "Vertex"
        FOLDER "Applications"
)

#-----------------------------
#  Testing
#-----------------------------
if (BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_SOURCE_DIR}/tests/**/*.cc"
            "${CMAKE_SOURCE_DIR}/tests/**/*.hh"
    )

    set(VERTEX_TEST_SOURCES
            ${VERTEX_SOURCES}
            "${CMAKE_SOURCE_DIR}/include/vertexusrrt/util.hh"
            "${CMAKE_SOURCE_DIR}/include/sdk/feature.h"
            "${CMAKE_SOURCE_DIR}/include/vertex/thread/vertexmpscthread.hh"
            "${CMAKE_SOURCE_DIR}/src/vertex/thread/vertexmpscthread.cc"
            "${CMAKE_SOURCE_DIR}/include/vertex/thread/vertexprioritythread.hh"
            "${CMAKE_SOURCE_DIR}/src/vertex/thread/vertexprioritythread.cc"
            "${CMAKE_SOURCE_DIR}/include/vertex/thread/threaddispatcher.hh"
            "${CMAKE_SOURCE_DIR}/src/vertex/thread/threaddispatcher.cc"
    )

    if (BUILD_VERTEXDECI3RT)
        list(APPEND VERTEX_TEST_SOURCES "${CMAKE_SOURCE_DIR}/include/vertexdeci3rt/init.hh")
    endif ()

    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/main\\.cc$")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/view/.*")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/viewmodel/.*")
    list(APPEND VERTEX_TEST_SOURCES
            "${CMAKE_SOURCE_DIR}/src/vertex/viewmodel/settingsviewmodel.cc"
            "${CMAKE_SOURCE_DIR}/src/vertex/view/debugger/disassemblycontrol.cc"
            "${CMAKE_SOURCE_DIR}/src/vertex/view/debugger/registerspanel.cc"
    )
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/customwidgets/.*")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/gui/.*")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/factory\\.cc$")
    list(APPEND VERTEX_TEST_SOURCES "${CMAKE_SOURCE_DIR}/src/vertex/gui/theme/themeprovider.cc")

    if (NOT WIN32)
        list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/io/io_test\\.cc$")
    endif ()

    add_executable(VertexTests ${TEST_SOURCES} ${VERTEX_TEST_SOURCES})
    add_dependencies(VertexTests generate_plugin_functions)

    target_include_directories(VertexTests PRIVATE
            "${CMAKE_SOURCE_DIR}/include"
            "${CMAKE_SOURCE_DIR}/tests"
            "${CMAKE_BINARY_DIR}/generated"
    )

    target_link_libraries(VertexTests PRIVATE
            mimalloc
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
            fmt::fmt
            wx::core
            wx::base
            wx::aui
            Angelscript::angelscript
            hwy::hwy
    )

    if (UNIX AND NOT APPLE)
        target_compile_options(VertexTests PRIVATE -fno-omit-frame-pointer)
    endif ()

    configure_target(VertexTests)

    set_target_properties(VertexTests PROPERTIES
            OUTPUT_NAME "VertexTests"
            FOLDER "Tests"
    )

    gtest_discover_tests(VertexTests)
    message(STATUS "Testing enabled - VertexTests target created")
endif ()

#-----------------------------
#  Resource Copying (Post-Build)
#-----------------------------
set(RESOURCE_COPY_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/res_to_out.cmake")
set(OUTPUT_DIR "$<TARGET_FILE_DIR:Vertex>")

file(GLOB LANGUAGE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/languages/*.json")
set(LANGUAGE_STAMP "${CMAKE_BINARY_DIR}/language_copy.stamp")

add_custom_command(
        OUTPUT "${LANGUAGE_STAMP}"
        DEPENDS ${LANGUAGE_FILES}
        COMMAND ${CMAKE_COMMAND}
        -Dsrc="${CMAKE_SOURCE_DIR}/resources/languages"
        -Ddst="${OUTPUT_DIR}/Language"
        -P "${RESOURCE_COPY_SCRIPT}"
        COMMAND ${CMAKE_COMMAND} -E touch "${LANGUAGE_STAMP}"
        COMMENT "Copying language files to output"
)

add_custom_target(CopyLanguageFiles ALL DEPENDS "${LANGUAGE_STAMP}")
add_dependencies(Vertex CopyLanguageFiles)
