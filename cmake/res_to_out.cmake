# CMake script to copy the CONTENTS of a directory (not the directory itself) if it exists
# Usage: cmake -P res_to_out.cmake -Dsrc=<src> -Ddst=<dst>
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

