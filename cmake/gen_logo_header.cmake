#
# gen_logo_header.cmake
#
# Reads logo.ico and generates a C++ header containing the binary data as a
# compile-time byte array, allowing the icon to be embedded directly into the
# executable without runtime file I/O.
#
# Variables (passed via -D):
#   LOGO_ICO_PATH  - absolute path to the source .ico file
#   OUTPUT_PATH    - absolute path for the generated .hh file
#

if(NOT EXISTS "${LOGO_ICO_PATH}")
    message(FATAL_ERROR "Logo ICO file not found: ${LOGO_ICO_PATH}")
endif()

file(READ "${LOGO_ICO_PATH}" rawHex HEX)

string(LENGTH "${rawHex}" hexLen)
math(EXPR dataLen "${hexLen} / 2")

string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," cppBytes "${rawHex}")
string(REGEX REPLACE ",+$" "" cppBytes "${cppBytes}")

get_filename_component(OUTPUT_DIR "${OUTPUT_PATH}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

file(WRITE "${OUTPUT_PATH}"
"#pragma once
#include <cstddef>

namespace Vertex::Gui
{
    inline constexpr unsigned char logo_ico[] = { ${cppBytes} };
    inline constexpr std::size_t logo_ico_size = ${dataLen};
}
")

message(STATUS "Generated logo header: ${OUTPUT_PATH} (${dataLen} bytes)")
