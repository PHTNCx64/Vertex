cmake_minimum_required(VERSION 3.30)

# ---------------------------------------------------------------------------
# vcpkg toolchain
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    else()
        message(WARNING
            "vcpkg toolchain not found. Either:\n"
            "  1. Set the VCPKG_ROOT environment variable to your vcpkg installation, or\n"
            "  2. Pass -DCMAKE_TOOLCHAIN_FILE=<path>/scripts/buildsystems/vcpkg.cmake, or\n"
            "  3. Clone vcpkg into ${CMAKE_SOURCE_DIR}/vcpkg\n"
            "  4. Use the build.ps1 script: ./build.ps1 -VcpkgRoot <path>")
    endif()
endif()

set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets")
set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_SOURCE_DIR}/overlay-ports")

project(VertexWorkspace VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared output layout:
#   executables -> <build>/bin
#   plugins     -> <build>/bin/Plugins
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")

include("${CMAKE_SOURCE_DIR}/cmake/target_config.cmake")

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(BUILD_ALL "Enable all components" OFF)
option(BUILD_VERTEX "Build the Vertex GUI application" ON)
option(BUILD_VERTEXUSRRT "Build vertexusrrt plugin" ON)
option(BUILD_VERTEXDECI3RT "Build vertexdeci3rt plugin" OFF)
option(BUILD_LIBTEST "Build libtest plugin" ON)
option(BUILD_RIP "Build RIP remote server" ON)
option(BUILD_TESTING "Build unit tests" ON)
option(BUILD_ONLY_RUNTIME "Build runtime plugins only" OFF)
option(VERTEX_BUNDLE_LLDB "Bundle liblldb and its transitive LLVM deps next to vertexusrrt (Linux only)" OFF)

if(BUILD_ALL)
    set(BUILD_VERTEX ON CACHE BOOL "Build the Vertex GUI application" FORCE)
    set(BUILD_VERTEXUSRRT ON CACHE BOOL "Build vertexusrrt plugin" FORCE)
    set(BUILD_VERTEXDECI3RT ON CACHE BOOL "Build vertexdeci3rt plugin" FORCE)
    set(BUILD_LIBTEST ON CACHE BOOL "Build libtest plugin" FORCE)
    set(BUILD_RIP ON CACHE BOOL "Build RIP remote server" FORCE)
    set(BUILD_TESTING ON CACHE BOOL "Build unit tests" FORCE)
endif()

if(BUILD_ONLY_RUNTIME)
    set(BUILD_VERTEX OFF CACHE BOOL "Build the Vertex GUI application" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Build unit tests" FORCE)
endif()

message(STATUS "Build configuration:")
message(STATUS "  vertex:        ${BUILD_VERTEX}")
message(STATUS "  vertexusrrt:   ${BUILD_VERTEXUSRRT}")
message(STATUS "  vertexdeci3rt: ${BUILD_VERTEXDECI3RT}")
message(STATUS "  libtest:       ${BUILD_LIBTEST}")
message(STATUS "  rip:           ${BUILD_RIP}")
message(STATUS "  tests:         ${BUILD_TESTING}")

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
if(BUILD_VERTEX)
    add_subdirectory(src/vertex)
endif()

if(BUILD_VERTEXUSRRT)
    add_subdirectory(src/vertexusrrt)
endif()

if(BUILD_VERTEXDECI3RT)
    add_subdirectory(src/vertexdeci3rt)
endif()

if(BUILD_LIBTEST)
    add_subdirectory(src/libtest)
endif()

if(BUILD_RIP)
    add_subdirectory(src/rip)
endif()

# ---------------------------------------------------------------------------
# Plugin output directory and resource setup
# ---------------------------------------------------------------------------
set(_any_plugin OFF)
if(BUILD_VERTEXUSRRT OR BUILD_VERTEXDECI3RT OR BUILD_LIBTEST)
    set(_any_plugin ON)
endif()

if(_any_plugin OR BUILD_VERTEX)
    set(PLUGINS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/Plugins")
    file(MAKE_DIRECTORY "${PLUGINS_OUTPUT_DIR}")

    set(_plugin_resource_script "${CMAKE_SOURCE_DIR}/cmake/res_to_out.cmake")
    add_custom_target(CopyPluginResources ALL
        COMMAND ${CMAKE_COMMAND}
            -Dsrc="${CMAKE_SOURCE_DIR}/resources/plugins"
            -Ddst="${PLUGINS_OUTPUT_DIR}"
            -P "${_plugin_resource_script}"
        COMMENT "Copying plugin resources to output"
    )
    unset(_plugin_resource_script)
endif()
unset(_any_plugin)

# ---------------------------------------------------------------------------
# Ensure building the main application also builds its plugins
# ---------------------------------------------------------------------------
if(BUILD_VERTEX)
    set(_plugin_deps)
    if(BUILD_VERTEXUSRRT)
        list(APPEND _plugin_deps vertexusrrt)
    endif()
    if(BUILD_VERTEXDECI3RT)
        list(APPEND _plugin_deps vertexdeci3rt)
    endif()
    if(BUILD_LIBTEST)
        list(APPEND _plugin_deps libtest)
    endif()
    if(_plugin_deps)
        add_dependencies(Vertex ${_plugin_deps})
    endif()
    unset(_plugin_deps)
endif()
