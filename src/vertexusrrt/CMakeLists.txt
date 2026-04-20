#-----------------------------
#  Dependencies
#-----------------------------
find_package(capstone CONFIG REQUIRED)

#-----------------------------
#  Source Files (Platform-independent)
#-----------------------------
set(SOURCES
    main.cc
    arch_registers.cc
    disassembler.cc
    watchpoint_throttle.cc
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/arch_registers.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/debugloopcontext.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/disassembler.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/native_handle.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/process_internal.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/memory_internal.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/watchpoint_throttle.hh
    ${CMAKE_SOURCE_DIR}/include/sdk/memory.h
    ${CMAKE_SOURCE_DIR}/include/sdk/api.h
)

#-----------------------------
#  Platform Configuration
#-----------------------------
if(WIN32)
    list(APPEND SOURCES
            windows/event/event_helpers.cc
            windows/event/process_opened.cc
            windows/event/debugger_attached.cc
            windows/disassembler/disassemble_range.cc
            windows/memory/memory_helpers.cc
            windows/memory/read_process.cc
            windows/memory/write_process.cc
            windows/memory/allocate.cc
            windows/memory/free.cc
            windows/memory/get_pointer_size.cc
            windows/memory/construct_attribute_filters.cc
            windows/memory/get_min_address.cc
            windows/memory/get_max_address.cc
            windows/memory/change_protection.cc
            windows/memory/query_regions.cc
            windows/process/process_helpers.cc
            windows/process/open.cc
            windows/process/close.cc
            windows/process/kill.cc
            windows/process/is_valid.cc
            windows/process/open_new.cc
            windows/process/get_executable_extensions.cc
            windows/process/get_list.cc
            windows/process/get_modules_list.cc
            windows/process/get_injection_methods.cc
            windows/process/get_module_imports.cc
            windows/process/get_module_exports.cc
            windows/thread/thread_helpers.cc
            windows/thread/get_threads.cc
            windows/thread/get_current_thread.cc
            windows/thread/suspend_thread.cc
            windows/thread/resume_thread.cc
            windows/thread/get_registers.cc
            windows/thread/get_call_stack.cc
            windows/thread/get_exception_info.cc
            windows/thread/read_register.cc
            windows/thread/write_register.cc
            windows/thread/priority_value_to_string.cc
            windows/wow64/architecture_detection.cc
            windows/debugger/debuggerstate.cc
            windows/debugger/debugloop.cc
            windows/debugger/helpers/debughelpers.cc
            windows/debugger/commands/continue_command.cc
            windows/debugger/commands/step_into_command.cc
            windows/debugger/commands/step_over_command.cc
            windows/debugger/commands/step_out_command.cc
            windows/debugger/commands/run_to_address_command.cc
            windows/debugger/exceptions/breakpoint_exception.cc
            windows/debugger/exceptions/single_step_exception.cc
            windows/debugger/exceptions/general_exception.cc
            windows/debugger/events/process_create.cc
            windows/debugger/events/process_exit.cc
            windows/debugger/events/thread_create.cc
            windows/debugger/events/thread_exit.cc
            windows/debugger/events/dll_load.cc
            windows/debugger/events/dll_unload.cc
            windows/debugger/events/output_string.cc
            windows/debugger/breakpoints/breakpoint_manager.cc
            windows/debugger/breakpoints/software_breakpoints.cc
            windows/debugger/breakpoints/hardware_breakpoints.cc
            windows/debugger/breakpoints/watchpoints.cc
            windows/debugger/breakpoints/breakpoint_api.cc
            windows/process/injector/detect_dll_arch.cc
            windows/process/get_library_extensions.cc
            windows/process/injector/remote_thread.cc
            windows/process/injector/manual_map.cc
            vertexusrrt.def
    )
elseif(APPLE)
    list(APPEND SOURCES
        macos/events.cc
        macos/process.cc
    )
elseif(UNIX)
    list(APPEND SOURCES
        linux/process/close.cc
        linux/process/get_executable_extensions.cc
        linux/process/get_injection_methods.cc
        linux/process/get_list.cc
        linux/process/get_module_exports.cc
        linux/process/get_module_imports.cc
        linux/process/get_modules_list.cc
        linux/process/injector/detect_dll_arch.cc
        linux/process/is_valid.cc
        linux/process/kill.cc
        linux/process/open.cc
        linux/process/open_new.cc
        linux/process/process_helpers.cc
        linux/process/architecture_detection.cc
        linux/memory/memory_helpers.cc
        linux/memory/read_process.cc
        linux/memory/write_process.cc
        linux/memory/allocate.cc
        linux/memory/free.cc
        linux/memory/get_pointer_size.cc
        linux/memory/construct_attribute_filters.cc
        linux/memory/get_min_address.cc
        linux/memory/get_max_address.cc
        linux/memory/change_protection.cc
        linux/memory/query_regions.cc
        linux/event/event_helpers.cc
        linux/event/process_opened.cc
        linux/event/debugger_attached.cc
        linux/disassembler/disassemble_range.cc
        linux/debugger/debugger_options.cc
        linux/debugger/lldb/lldb_backend.cc
        linux/debugger/lldb/lldb_breakpoints.cc
        linux/debugger/lldb/lldb_state.cc
        linux/debugger/lldb/lldb_watchpoints.cc
        linux/thread/thread_helpers.cc
        linux/thread/get_threads.cc
        linux/thread/get_current_thread.cc
        linux/thread/suspend_thread.cc
        linux/thread/resume_thread.cc
        linux/thread/get_registers.cc
        linux/thread/get_call_stack.cc
        linux/thread/get_exception_info.cc
        linux/thread/read_register.cc
        linux/thread/write_register.cc
        linux/thread/priority_value_to_string.cc
    )
endif()

#-----------------------------
#  Library Target
#-----------------------------
add_library(vertexusrrt SHARED ${SOURCES})

target_include_directories(vertexusrrt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(vertexusrrt PRIVATE capstone::capstone)

if(UNIX AND NOT APPLE)
    find_package(LLDB QUIET)
    if(LLDB_FOUND)
        target_link_libraries(vertexusrrt PRIVATE lldb)
    else()
        find_library(LLDB_LIBRARY NAMES lldb lldb-22 lldb-21 lldb-20 lldb-19 lldb-18 lldb-17 lldb-16 lldb-15 lldb-14
            PATHS
                /usr/lib
                /usr/lib64
                /usr/lib/llvm/lib
                /usr/lib/llvm-18/lib
                /usr/lib/llvm-17/lib
                /usr/lib/llvm-16/lib
                /usr/lib/llvm-15/lib
                /usr/lib/llvm-14/lib
        )
        find_path(LLDB_INCLUDE_DIR NAMES lldb/API/LLDB.h
            PATHS
                /usr/include
                /usr/lib/llvm/include
                /usr/lib/llvm-18/include
                /usr/lib/llvm-17/include
                /usr/lib/llvm-16/include
                /usr/lib/llvm-15/include
                /usr/lib/llvm-14/include
        )
        if(LLDB_LIBRARY AND LLDB_INCLUDE_DIR)
            target_link_libraries(vertexusrrt PRIVATE ${LLDB_LIBRARY})
            target_include_directories(vertexusrrt PRIVATE ${LLDB_INCLUDE_DIR})
        else()
            message(FATAL_ERROR
                "liblldb not found. Install LLDB development files:\n"
                "  Arch Linux: pacman -S lldb\n"
                "  Debian/Ubuntu: apt install liblldb-dev\n"
                "  Fedora: dnf install lldb-devel"
            )
        endif()
    endif()
endif()

configure_target(vertexusrrt SUBDIR Plugins)

set_target_properties(vertexusrrt PROPERTIES
    OUTPUT_NAME "vertexusrrt"
    FOLDER "Runtime"
)

if(UNIX AND NOT APPLE)
    set_target_properties(vertexusrrt PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

#-----------------------------
#  Post-Build: Copy Dependencies
#-----------------------------
if(WIN32)
    add_custom_command(TARGET vertexusrrt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:capstone::capstone>
            "${CMAKE_BINARY_DIR}"
        COMMENT "Copying Capstone DLL to build directory"
    )
endif()

if(UNIX AND NOT APPLE AND VERTEX_BUNDLE_LLDB)
    add_custom_command(TARGET vertexusrrt POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_FILE=$<TARGET_FILE:vertexusrrt>
            -DOUTPUT_DIR=$<TARGET_FILE_DIR:vertexusrrt>
            -P "${CMAKE_SOURCE_DIR}/cmake/bundle_runtime_deps.cmake"
        COMMENT "Bundling LLDB/LLVM runtime dependencies next to vertexusrrt"
        VERBATIM
    )
endif()
