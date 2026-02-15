# This script generates the Icons.hh wrapper header for all SVG icons.
# It is called from CMakeLists.txt as a custom command.

# Get output file from command line
if(NOT DEFINED out)
    message(FATAL_ERROR "gen_icon_wrapper.cmake: 'out' variable not set!")
endif()

set(ICON_INCLUDE_LINES "")

foreach(mode lightmode darkmode)
    file(GLOB SVG_FILES "${src_dir}/resources/${mode}/*.svg")
    foreach(svg ${SVG_FILES})
        get_filename_component(name ${svg} NAME_WE)
        string(APPEND ICON_INCLUDE_LINES "#include \"icons/${mode}_${name}.hh\"\n")
    endforeach()
endforeach()

set(WRAPPER_CONTENT "// Auto-generated icon wrapper header\n#pragma once\n\n${ICON_INCLUDE_LINES}")

file(WRITE "${out}" "${WRAPPER_CONTENT}")
