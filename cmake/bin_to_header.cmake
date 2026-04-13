#
# bin_to_header.cmake
#
# Converts a binary file into a C++ header containing a constexpr uint8_t array.
# The data is embedded directly in the executable without runtime file I/O.
#
# Command line arguments (passed via CMAKE_ARGV):
# - CMAKE_ARGV3: Path to input binary file
# - CMAKE_ARGV4: Path to output header file
# - CMAKE_ARGV5: Variable name for the data array
#

set(INPUT_FILE ${CMAKE_ARGV3})
set(OUTPUT_HEADER ${CMAKE_ARGV4})
set(VAR_NAME ${CMAKE_ARGV5})

if(NOT EXISTS ${INPUT_FILE})
    message(FATAL_ERROR "Input file does not exist: ${INPUT_FILE}")
endif()

file(READ ${INPUT_FILE} FILE_HEX HEX)
string(LENGTH "${FILE_HEX}" HEX_LENGTH)
math(EXPR BYTE_COUNT "${HEX_LENGTH} / 2")

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BYTE_LIST "${FILE_HEX}")
string(REGEX REPLACE ",$" "" BYTE_LIST "${BYTE_LIST}")

set(FORMATTED "")
set(POS 0)
string(REGEX MATCHALL "0x[0-9a-f][0-9a-f]" BYTES "${BYTE_LIST}")
list(LENGTH BYTES TOTAL)
math(EXPR LAST "${TOTAL} - 1")

set(LINE "")
set(COL 0)
foreach(IDX RANGE ${LAST})
    list(GET BYTES ${IDX} B)
    if(IDX LESS LAST)
        string(APPEND LINE "${B},")
    else()
        string(APPEND LINE "${B}")
    endif()
    math(EXPR COL "${COL} + 1")
    if(COL EQUAL 16)
        string(APPEND FORMATTED "        ${LINE}\n")
        set(LINE "")
        set(COL 0)
    endif()
endforeach()

if(NOT LINE STREQUAL "")
    string(APPEND FORMATTED "        ${LINE}\n")
endif()

get_filename_component(OUTPUT_DIR ${OUTPUT_HEADER} DIRECTORY)
file(MAKE_DIRECTORY ${OUTPUT_DIR})

file(WRITE ${OUTPUT_HEADER}
"#pragma once

#include <cstddef>
#include <cstdint>

namespace Vertex::Gui
{
    inline constexpr std::uint8_t ${VAR_NAME}[] = {
${FORMATTED}    };
    inline constexpr std::size_t ${VAR_NAME}_size = ${BYTE_COUNT};
}
")

message("Generated binary header: ${OUTPUT_HEADER} (${BYTE_COUNT} bytes)")
