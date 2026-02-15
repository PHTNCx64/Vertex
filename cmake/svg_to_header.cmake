# Convert SVG files into C++ header containing the svg data as a string.

set(SVG_FILE ${CMAKE_ARGV3})
set(OUTPUT_HEADER ${CMAKE_ARGV4})
set(VAR_NAME ${CMAKE_ARGV5})

if(NOT EXISTS ${SVG_FILE})
    message(FATAL_ERROR "SVG file does not exist: ${SVG_FILE}")
endif()

file(READ ${SVG_FILE} SVG_CONTENT)

# Escape quotes, backslashes, and newlines for C++ string literals
string(REPLACE "\\" "\\\\" SVG_CONTENT "${SVG_CONTENT}")
string(REPLACE "\"" "\\\"" SVG_CONTENT "${SVG_CONTENT}")
string(REPLACE "\r\n" "\\n" SVG_CONTENT "${SVG_CONTENT}")
string(REPLACE "\n" "\\n" SVG_CONTENT "${SVG_CONTENT}")
string(REPLACE "\r" "\\n" SVG_CONTENT "${SVG_CONTENT}")

get_filename_component(OUTPUT_DIR ${OUTPUT_HEADER} DIRECTORY)
file(MAKE_DIRECTORY ${OUTPUT_DIR})

get_filename_component(HEADER_NAME ${OUTPUT_HEADER} NAME_WE)
string(TOUPPER ${HEADER_NAME} INCLUDE_GUARD)
string(REPLACE "-" "_" INCLUDE_GUARD ${INCLUDE_GUARD})

file(WRITE ${OUTPUT_HEADER}
        "#pragma once

  namespace Vertex::Gui
  {
      const char ${VAR_NAME}[] = \"${SVG_CONTENT}\";
  }
  ")

message("Generated icon header: ${OUTPUT_HEADER}")