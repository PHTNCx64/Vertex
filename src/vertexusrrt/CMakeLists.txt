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
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/arch_registers.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/debugloopcontext.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/disassembler.hh
    ${CMAKE_SOURCE_DIR}/include/vertexusrrt/native_handle.hh
    ${CMAKE_SOURCE_DIR}/include/sdk/memory.h
    ${CMAKE_SOURCE_DIR}/include/sdk/api.h
)

#-----------------------------
#  Platform Configuration
#-----------------------------
if(WIN32)
    list(APPEND SOURCES
            windows/memory.cc
            windows/events.cc
            windows/disassembler.cc
            windows/thread.cc
            windows/wow64.cc
            windows/process.cc
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
            vertexusrrt.def
    )
elseif(APPLE)
    list(APPEND SOURCES
        macos/events.cc
        macos/process.cc
    )
elseif(UNIX)
    list(APPEND SOURCES
        linux/events.cc
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

configure_target(vertexusrrt)

set_target_properties(vertexusrrt PROPERTIES
    OUTPUT_NAME "vertexusrrt"
    FOLDER "Runtime"
)

#-----------------------------
#  Post-Build: Copy Dependencies
#-----------------------------
add_custom_command(TARGET vertexusrrt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:capstone::capstone>
        "${CMAKE_BINARY_DIR}"
    COMMENT "Copying Capstone DLL to build directory"
)
