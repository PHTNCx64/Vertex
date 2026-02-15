#
# gen_plugin_functions.cmake
#
# Generates plugin system boilerplate code by parsing the C API header file and extracting function declarations.
# This script automates the creation of:
#
# 1. plugin_functions.hh - Macros for:
#    - Function pointer declarations for each API function
#    - Function registration logic that loads functions from plugin DLLs
#
# 2. plugin_move_semantics.hh - Macros for:
#    - Move constructor initialization list
#    - Move constructor nullification
#    - Move assignment operator copy operations
#    - Move assignment operator nullification
#
# 3. plugin_function_registration.hh - Helper function to register all plugin functions
#
# The script extracts VERTEX_EXPORT/VERTEX_API function declarations from the API header,
# distinguishes between required (vertex_init, vertex_exit) and optional functions,
# and generates type-safe C++ wrappers for dynamic loading.
#
# Required variables:
# - api_header: Path to the C API header file (sdk/api.h)
# - output_header: Path for plugin_functions.hh output
# - output_move_semantics: Path for plugin_move_semantics.hh output
# - output_impl: Path for plugin_function_registration.hh output
#

if(NOT api_header)
    message(FATAL_ERROR "api_header parameter is required")
endif()

if(NOT output_header)
    message(FATAL_ERROR "output_header parameter is required")
endif()

if(NOT output_move_semantics)
    message(FATAL_ERROR "output_move_semantics parameter is required")
endif()

if(NOT output_impl)
    message(FATAL_ERROR "output_impl parameter is required")
endif()

if(NOT EXISTS ${api_header})
    message(FATAL_ERROR "API header file does not exist: ${api_header}")
endif()

# Read the API header file
file(READ ${api_header} API_CONTENT)

# Extract function declarations
string(REGEX MATCHALL "VERTEX_EXPORT[^;]+VERTEX_API[^;]+;" FUNCTION_DECLARATIONS "${API_CONTENT}")

set(FUNCTION_POINTERS_MACRO "")
set(FUNCTION_REGISTRATION_MACRO "")
set(MOVE_CONSTRUCTOR_INIT "")
set(MOVE_CONSTRUCTOR_NULLIFY "")
set(MOVE_ASSIGN_COPY "")
set(MOVE_ASSIGN_NULLIFY "")

foreach(DECLARATION ${FUNCTION_DECLARATIONS})
    # Extract function name and signature
    string(REGEX MATCH "VERTEX_API ([^(]+)\\(([^)]*)\\)" MATCH_RESULT "${DECLARATION}")

    if(CMAKE_MATCH_1)
        set(FUNCTION_NAME ${CMAKE_MATCH_1})
        set(FUNCTION_PARAMS ${CMAKE_MATCH_2})

        # Map C typedef names to their full names to avoid C++ namespace conflicts
        string(REPLACE "Runtime*" "VertexRuntime*" FUNCTION_PARAMS "${FUNCTION_PARAMS}")
        # Replace 'Event event' with 'VertexEvent event' to avoid conflict with Vertex::Event namespace
        string(REPLACE "Event event" "VertexEvent event" FUNCTION_PARAMS "${FUNCTION_PARAMS}")

        # Determine if function is required or optional
        # Only vertex_init and vertex_exit are required, all others are optional
        set(REQUIREMENT "FunctionRequirement::Optional")
        if(FUNCTION_NAME MATCHES "^vertex_(init|exit)$")
            set(REQUIREMENT "FunctionRequirement::Required")
        endif()

        # Build function pointer type and declaration with proper C++ syntax
        string(REPLACE " " "" CLEAN_PARAMS "${FUNCTION_PARAMS}")
        if(CLEAN_PARAMS STREQUAL "")
            set(FUNCTION_TYPE "StatusCode(VERTEX_API*)()")
            set(DECL_PARAMS "")
        else()
            set(FUNCTION_TYPE "StatusCode(VERTEX_API*)(${FUNCTION_PARAMS})")
            set(DECL_PARAMS "${FUNCTION_PARAMS}")
        endif()

        # Append to function pointer macro body
        if(CLEAN_PARAMS STREQUAL "")
            string(APPEND FUNCTION_POINTERS_MACRO "        StatusCode(VERTEX_API *internal_${FUNCTION_NAME})() = nullptr; \\\n")
        else()
            string(APPEND FUNCTION_POINTERS_MACRO "        StatusCode(VERTEX_API *internal_${FUNCTION_NAME})(${DECL_PARAMS}) = nullptr; \\\n")
        endif()

        # Append to registration macro body
        string(APPEND FUNCTION_REGISTRATION_MACRO "        registry.register_function<${FUNCTION_TYPE}>(\"${FUNCTION_NAME}\", ${REQUIREMENT}, &plugin.internal_${FUNCTION_NAME}); \\\n")

        # Generate move constructor initialization (comma prefix for proper list formatting)
        if(MOVE_CONSTRUCTOR_INIT STREQUAL "")
            set(MOVE_INIT_LINE "internal_${FUNCTION_NAME}(other.internal_${FUNCTION_NAME}), \\\n")
        else()
            set(MOVE_INIT_LINE "              internal_${FUNCTION_NAME}(other.internal_${FUNCTION_NAME}), \\\n")
        endif()
        string(APPEND MOVE_CONSTRUCTOR_INIT ${MOVE_INIT_LINE})

        # Generate move constructor nullification
        string(APPEND MOVE_CONSTRUCTOR_NULLIFY "            other.internal_${FUNCTION_NAME} = nullptr; \\\n")

        # Generate move assignment copy
        string(APPEND MOVE_ASSIGN_COPY "                internal_${FUNCTION_NAME} = other.internal_${FUNCTION_NAME}; \\\n")

        # Generate move assignment nullification
        string(APPEND MOVE_ASSIGN_NULLIFY "                other.internal_${FUNCTION_NAME} = nullptr; \\\n")
    endif()
endforeach()

# Remove trailing comma, backslash and newline from last move constructor init entry
string(REGEX REPLACE ", \\\\[ \t]*[\r\n]+$" "" MOVE_CONSTRUCTOR_INIT "${MOVE_CONSTRUCTOR_INIT}")

# Write header file with function pointer and registration macros
file(WRITE ${output_header} "//\n")
file(APPEND ${output_header} "// Auto-generated Plugin function pointer and registration macros\n")
file(APPEND ${output_header} "// Generated by cmake/gen_plugin_functions.cmake\n")
file(APPEND ${output_header} "// DO NOT EDIT MANUALLY\n")
file(APPEND ${output_header} "//\n\n")
file(APPEND ${output_header} "#pragma once\n\n")
file(APPEND ${output_header} "// ===============================================================================================================//\n")
file(APPEND ${output_header} "// AUTO-GENERATED API FUNCTION POINTERS FROM C API                                                                //\n")
file(APPEND ${output_header} "// ===============================================================================================================//\n")
file(APPEND ${output_header} "#define VERTEX_PLUGIN_FUNCTION_POINTERS \\\n")
file(APPEND ${output_header} "${FUNCTION_POINTERS_MACRO}\n\n")

file(APPEND ${output_header} "// ===============================================================================================================//\n")
file(APPEND ${output_header} "// AUTO-GENERATED FUNCTION REGISTRATION                                                                           //\n")
file(APPEND ${output_header} "// ===============================================================================================================//\n")
file(APPEND ${output_header} "#define VERTEX_PLUGIN_REGISTER_FUNCTIONS(registry, plugin) \\\n")
file(APPEND ${output_header} "    \\\n")
file(APPEND ${output_header} "${FUNCTION_REGISTRATION_MACRO}")
file(APPEND ${output_header} "    \n")

# Write separate file for move semantics macros
file(WRITE ${output_move_semantics} "//\n")
file(APPEND ${output_move_semantics} "// Auto-generated Plugin move semantics\n")
file(APPEND ${output_move_semantics} "// Generated by cmake/gen_plugin_functions.cmake\n")
file(APPEND ${output_move_semantics} "// DO NOT EDIT MANUALLY\n")
file(APPEND ${output_move_semantics} "//\n\n")
file(APPEND ${output_move_semantics} "#pragma once\n\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "// AUTO-GENERATED MOVE CONSTRUCTOR INITIALIZER LIST                                                               //\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "#define VERTEX_PLUGIN_MOVE_INIT_LIST \\\n")
file(APPEND ${output_move_semantics} "${MOVE_CONSTRUCTOR_INIT}\n\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "// AUTO-GENERATED MOVE CONSTRUCTOR NULLIFICATION                                                                  //\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "#define VERTEX_PLUGIN_MOVE_NULLIFY \\\n")
file(APPEND ${output_move_semantics} "${MOVE_CONSTRUCTOR_NULLIFY}\n\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "// AUTO-GENERATED MOVE ASSIGNMENT COPY                                                                            //\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "#define VERTEX_PLUGIN_MOVE_ASSIGN_COPY \\\n")
file(APPEND ${output_move_semantics} "${MOVE_ASSIGN_COPY}\n\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "// AUTO-GENERATED MOVE ASSIGNMENT NULLIFICATION                                                                   //\n")
file(APPEND ${output_move_semantics} "// ===============================================================================================================//\n")
file(APPEND ${output_move_semantics} "#define VERTEX_PLUGIN_MOVE_ASSIGN_NULLIFY \\\n")
file(APPEND ${output_move_semantics} "${MOVE_ASSIGN_NULLIFY}\n")

# Generate the function registration implementation header as a thin wrapper around the macro
set(IMPL_CONTENT "//
// Auto-generated Plugin function registration helper
// Generated by cmake/gen_plugin_functions.cmake
// DO NOT EDIT MANUALLY
//

#pragma once

#include <vertex/runtime/function_registry.hh>
#include <vertex/runtime/plugin.hh>
#include <sdk/api.h>
#include \"plugin_functions.hh\"

namespace Vertex::Runtime {

inline void register_all_plugin_functions(FunctionRegistry& registry, Plugin& plugin) {
    VERTEX_PLUGIN_REGISTER_FUNCTIONS(registry, plugin);
}

} // namespace Vertex::Runtime
")

file(WRITE ${output_impl} "${IMPL_CONTENT}")

message(STATUS "Generated Plugin function declarations: ${output_header}")
message(STATUS "Generated Plugin move semantics: ${output_move_semantics}")
message(STATUS "Generated Plugin function registrations: ${output_impl}")
