#
# gen_icon_map.cmake
#
# Generates IconMap.hh header file containing:
# - Extern declarations for all SVG icon variables (both lightmode and darkmode)
# - Static unordered_maps (LightIconMap and DarkIconMap) that map icon names to their SVG content
#
# This script scans the resources/lightmode and resources/darkmode directories for SVG files
# and creates C++ data structures for runtime icon lookup by name.
#
# Required variables:
# - out: Path to the output header file
# - src_dir: Path to the source directory containing resources/
#

if(NOT DEFINED out)
    message(FATAL_ERROR "gen_icon_map.cmake: 'out' variable not set!")
endif()

set(EXTERNS "")
set(LIGHTMAP "")
set(DARKMAP "")

foreach(mode lightmode darkmode)
    file(GLOB SVG_FILES "${src_dir}/resources/${mode}/*.svg")
    foreach(svg ${SVG_FILES})
        get_filename_component(name ${svg} NAME_WE)
        string(APPEND EXTERNS "    extern const char icon_${mode}_${name}_svg[];\n")
        if(mode STREQUAL "lightmode")
            string(APPEND LIGHTMAP "        {\"${name}\", icon_lightmode_${name}_svg},\n")
        else()
            string(APPEND DARKMAP "        {\"${name}\", icon_darkmode_${name}_svg},\n")
        endif()
    endforeach()
endforeach()

set(MAP_HEADER "// Auto-generated icon map header\n#pragma once\n\n#include <unordered_map>\n#include <string>\n\nnamespace Vertex::Gui {\n${EXTERNS}\n    static const std::unordered_map<std::string, const char*> LightIconMap = {\n${LIGHTMAP}    };\n    static const std::unordered_map<std::string, const char*> DarkIconMap = {\n${DARKMAP}    };\n}\n")

file(WRITE "${out}" "${MAP_HEADER}")
