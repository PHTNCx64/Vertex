cmake_minimum_required(VERSION 3.30)

#-----------------------------
#  VCPKG Integration
#-----------------------------
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(Vertex VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
        ApplicationName="Vertex"
        ApplicationVendor="PHTNC"
        ApplicationVersion="${PROJECT_VERSION}"
)

if(WIN32)
    add_compile_definitions(
            UNICODE                 # Use Unicode Windows API
            _UNICODE                # Use Unicode C runtime
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            VC_EXTRALEAN
    )
endif()

#-----------------------------
#  Compiler Configuration
#-----------------------------
function(set_output_directories target)
    set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endfunction()

function(configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
                /W4
                /permissive-
                /utf-8
                /FC
                /arch:AVX2
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        target_compile_options(${target} PRIVATE
                /utf-8
                -Wextra
                -fdiagnostics-absolute-paths
                /clang:-fno-caret-diagnostics
                /clang:-fno-diagnostics-fixit-info
                -Wno-c++98-compat
                -Wno-pre-c++17-compat
                -Wno-c++98-compat-pedantic
                -mavx2 -msse4.2
        )
    else()
    endif()

    set_output_directories(${target})
    set_target_properties(${target} PROPERTIES DEBUG_POSTFIX "_dbg")
endfunction()

#-----------------------------
#  Dependencies
#-----------------------------
find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base richtext stc aui)
find_package(mimalloc CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(NanoSVG CONFIG REQUIRED)
find_package(Lua REQUIRED)

#-----------------------------
#  Plugin Function Auto-Generation
#-----------------------------
set(PLUGIN_FUNCTIONS_HEADER "${CMAKE_BINARY_DIR}/generated/plugin_functions.hh")
set(PLUGIN_MOVE_SEMANTICS_HEADER "${CMAKE_BINARY_DIR}/generated/plugin_move_semantics.hh")
set(PLUGIN_REGISTRATION_HEADER "${CMAKE_BINARY_DIR}/generated/plugin_function_registration.hh")

add_custom_command(
        OUTPUT ${PLUGIN_FUNCTIONS_HEADER} ${PLUGIN_MOVE_SEMANTICS_HEADER} ${PLUGIN_REGISTRATION_HEADER}
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
        DEPENDS ${PLUGIN_FUNCTIONS_HEADER} ${PLUGIN_MOVE_SEMANTICS_HEADER} ${PLUGIN_REGISTRATION_HEADER})

#-----------------------------
#  SVG Icon Generation
#-----------------------------
function(convert_svg_to_header svg output var)
    add_custom_command(
            OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/svg_to_header.cmake ${svg} ${output} ${var}
            DEPENDS ${svg}
            COMMENT "Converting ${svg} → ${output}"
    )
endfunction()

set(GENERATED_HEADERS)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/icons")

foreach(mode lightmode darkmode)
    file(GLOB SVG_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/${mode}/*.svg")
    foreach(svg ${SVG_FILES})
        get_filename_component(name ${svg} NAME_WE)
        set(out "${CMAKE_BINARY_DIR}/generated/icons/${mode}_${name}.hh")
        convert_svg_to_header(${svg} ${out} "icon_${mode}_${name}_svg")
        list(APPEND GENERATED_HEADERS ${out})
    endforeach()
endforeach()

set(ICONS_WRAPPER_HEADER "${CMAKE_BINARY_DIR}/generated/Icons.hh")
add_custom_command(
        OUTPUT ${ICONS_WRAPPER_HEADER}
        COMMAND ${CMAKE_COMMAND} -Dout="${ICONS_WRAPPER_HEADER}" -Dsrc_dir="${CMAKE_SOURCE_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/gen_icon_wrapper.cmake"
        DEPENDS ${GENERATED_HEADERS}
        COMMENT "Generating Icons.hh wrapper header"
)

list(APPEND GENERATED_HEADERS ${ICONS_WRAPPER_HEADER})

set(ICONMAP_HEADER "${CMAKE_BINARY_DIR}/generated/IconMap.hh")
add_custom_command(
        OUTPUT ${ICONMAP_HEADER}
        COMMAND ${CMAKE_COMMAND} -Dout="${ICONMAP_HEADER}" -Dsrc_dir="${CMAKE_SOURCE_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/gen_icon_map.cmake"
        DEPENDS ${GENERATED_HEADERS}
        COMMENT "Generating IconMap.hh mapping header"
)
list(APPEND GENERATED_HEADERS ${ICONMAP_HEADER})

add_custom_target(generate_icon_headers DEPENDS ${GENERATED_HEADERS})
list(LENGTH GENERATED_HEADERS num_icons)
message(STATUS "Embedding ${num_icons} SVG icons")

#-----------------------------
#  Source Collection
#-----------------------------
# Collect all sources first
file(GLOB_RECURSE VERTEX_SOURCES CONFIGURE_DEPENDS
        "src/vertex/**/*.cc"
        "include/vertex/**/*.hh"
        "include/sdk/*.h"
)

file(GLOB VERTEX_ROOT_SOURCES CONFIGURE_DEPENDS
        "src/vertex/*.cc"
)
list(APPEND VERTEX_SOURCES ${VERTEX_ROOT_SOURCES})

# Exclude ALL platform-specific directories
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Ww]indows/.*")
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Ll]inux/.*")
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Dd]arwin/.*")
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/[Mm]acos/.*")

# Exclude vertexusrrt sources from main Vertex build
list(FILTER VERTEX_SOURCES EXCLUDE REGEX ".*/plugin_runtime/.*")

if(WIN32)
    file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
            "src/vertex/**/windows/*.cc"
            "src/vertex/**/Windows/*.cc"
            "include/vertex/**/windows/*.hh"
            "include/vertex/**/Windows/*.hh"
    )
    list(APPEND VERTEX_SOURCES ${PLATFORM_SOURCES})
    message(STATUS "Building for Windows - including Windows-specific sources")
elseif(UNIX AND NOT APPLE)
    file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
            "src/vertex/**/linux/*.cc"
            "src/vertex/**/Linux/*.cc"
            "include/vertex/**/linux/*.hh"
            "include/vertex/**/Linux/*.hh"
    )
    list(APPEND VERTEX_SOURCES ${PLATFORM_SOURCES})
    message(STATUS "Building for Linux - including Linux-specific sources")
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
            "src/vertex/**/darwin/*.mm"
            "src/vertex/**/darwin/*.cc"
            "include/vertex/**/macos/*.hh"
            "include/vertex/**/darwin/*.hh"
    )
    list(APPEND VERTEX_SOURCES ${PLATFORM_SOURCES})
    message(STATUS "Building for macOS - including macOS-specific sources")
endif()

#-----------------------------
#  Main Executable
#-----------------------------
add_executable(Vertex WIN32 ${VERTEX_SOURCES})
add_dependencies(Vertex generate_icon_headers generate_plugin_functions)
target_include_directories(Vertex PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(Vertex PRIVATE
        mimalloc
        wx::core wx::base wx::richtext wx::stc wx::aui
        fmt::fmt
        NanoSVG::nanosvg NanoSVG::nanosvgrast
        lua
)

configure_target(Vertex)

if(GENERATED_HEADERS)
    target_compile_definitions(Vertex PRIVATE VERTEX_HAS_GENERATED_ICONS)
endif()

set_target_properties(Vertex PROPERTIES
        OUTPUT_NAME "Vertex"
        FOLDER "Applications"
)

#-----------------------------
#  Testing
#-----------------------------
option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    # Collect all test files
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "tests/**/*.cc" "tests/**/*.hh")

    # Reuse production sources (exclude main.cc AND GUI files for tests)
    set(VERTEX_TEST_SOURCES ${VERTEX_SOURCES}
            include/vertexusrrt/util.hh)
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/main\\.cc$")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/view/.*")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/viewmodel/.*")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/customwidgets/.*")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/gui/.*")
    list(FILTER VERTEX_TEST_SOURCES EXCLUDE REGEX ".*/factory\\.cc$")

    add_executable(VertexTests ${TEST_SOURCES} ${VERTEX_TEST_SOURCES})
    add_dependencies(VertexTests generate_plugin_functions)

    target_include_directories(VertexTests PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_BINARY_DIR}/generated
    )

    target_link_libraries(VertexTests PRIVATE
            mimalloc
            GTest::gtest GTest::gtest_main
            GTest::gmock GTest::gmock_main
            fmt::fmt
            lua
            wx::core wx::base
    )

    configure_target(VertexTests)

    set_target_properties(VertexTests PROPERTIES
            OUTPUT_NAME "VertexTests"
            FOLDER "Tests"
    )

    gtest_discover_tests(VertexTests)
    message(STATUS "Testing enabled - VertexTests target created")
endif()

#-----------------------------
#  Plugin Runtime (separate DLL)
#-----------------------------
add_subdirectory(src/vertexusrrt)

# Ensure plugin runtime is built when Vertex is built
add_dependencies(Vertex vertexusrrt)

#-----------------------------
#  Resource Copying (Post-Build)
#-----------------------------
set(RESOURCE_COPY_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/res_to_out.cmake")
set(OUTPUT_DIR $<TARGET_FILE_DIR:Vertex>)

add_custom_command(TARGET Vertex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/Plugins"
        COMMENT "Creating Plugins directory"
)

add_custom_command(TARGET Vertex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -Dsrc="${CMAKE_SOURCE_DIR}/resources/plugins" -Ddst="${OUTPUT_DIR}/Plugins" -P "${RESOURCE_COPY_SCRIPT}"
)

add_custom_command(TARGET Vertex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -Dsrc="${CMAKE_SOURCE_DIR}/resources/languages" -Ddst="${OUTPUT_DIR}/Language" -P "${RESOURCE_COPY_SCRIPT}"
)

add_custom_command(TARGET Vertex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:vertexusrrt>
                "${OUTPUT_DIR}/Plugins/"
        COMMENT "Copying vertexusrrt plugin to Plugins folder"
)

if(WIN32 AND MSVC)
    add_custom_command(TARGET Vertex POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_PDB_FILE:vertexusrrt>
                    "${OUTPUT_DIR}/Plugins/"
            COMMENT "Copying vertexusrrt debug symbols to Plugins folder"
    )
endif()

