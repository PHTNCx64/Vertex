function(set_output_directories target)
    cmake_parse_arguments(ARG "" "SUBDIR" "" ${ARGN})
    if(ARG_SUBDIR)
        set(out_dir "${CMAKE_BINARY_DIR}/bin/${ARG_SUBDIR}")
    else()
        set(out_dir "${CMAKE_BINARY_DIR}/bin")
    endif()

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${out_dir}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${out_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${out_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${out_dir}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${out_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${out_dir}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${out_dir}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${out_dir}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${out_dir}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${out_dir}"
        ARCHIVE_OUTPUT_DIRECTORY "${out_dir}"
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${out_dir}"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${out_dir}"
        ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${out_dir}"
        ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${out_dir}"
        PDB_OUTPUT_DIRECTORY "${out_dir}"
        COMPILE_PDB_OUTPUT_DIRECTORY "${out_dir}"
    )
endfunction()

function(configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /FC
            /arch:AVX2
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        target_compile_options(${target} PRIVATE
            /utf-8
            -Wextra
            -fdiagnostics-absolute-paths
            /clang:-fno-caret-diagnostics
            /clang:-fno-diagnostics-fixit-info
            -Wno-c++98-compat
            -Wno-pre-c++17-compat
            -Wno-c++98-compat-pedantic
            -mavx2
            -msse4.2
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -mavx2
            -msse4.2
            -finput-charset=UTF-8
            -fexec-charset=UTF-8
        )
    endif()

    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            UNICODE
            _UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            VC_EXTRALEAN
        )
    endif()

    if(MINGW)
        target_compile_definitions(${target} PRIVATE
            _WIN32_WINNT=0x0A00
        )
        target_link_libraries(${target} PRIVATE stdc++exp)
    endif()

    cmake_parse_arguments(CFG "" "SUBDIR" "" ${ARGN})
    if(CFG_SUBDIR)
        set_output_directories(${target} SUBDIR "${CFG_SUBDIR}")
    else()
        set_output_directories(${target})
    endif()

    set_target_properties(${target} PROPERTIES DEBUG_POSTFIX "_dbg")
endfunction()
