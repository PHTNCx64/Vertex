#
# gen_icon_wrapper.cmake
#
# Generates Icons.hh header file that serves as a central include point for all individual icon headers.
# This script scans the resources/lightmode and resources/darkmode directories for SVG files
# and generates #include directives for each corresponding icon header file.
#
# This allows the main codebase to include a single header to access all icons instead of
# manually including each icon header separately.
#
# Required variables:
# - out: Path to the output wrapper header file
# - src_dir: Path to the source directory containing resources/
#

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
