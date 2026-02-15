#
# res_to_out.cmake
#
# Copies all contents from a source directory to a destination directory if the source exists.
# This is a utility script for copying resource files during the build process.
#
# If the source directory doesn't exist, the script exits silently without error.
# If the source exists, creates the destination directory (if needed) and copies all items.
#
# Required variables:
# - src: Source directory path
# - dst: Destination directory path
#

if(NOT DEFINED src OR NOT DEFINED dst)
    message(FATAL_ERROR "Usage: cmake -P copy_contents_if_exists.cmake -Dsrc=<src> -Ddst=<dst>")
endif()
if(EXISTS "${src}")
    file(GLOB items "${src}/*")
    file(MAKE_DIRECTORY "${dst}")
    foreach(item ${items})
        get_filename_component(name "${item}" NAME)
        file(COPY "${item}" DESTINATION "${dst}")
    endforeach()
endif()

