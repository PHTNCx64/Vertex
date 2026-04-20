message(STATUS "Building healthgame")

add_executable(healthgame main.cc)

target_include_directories(healthgame PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

configure_target(healthgame)

set_target_properties(healthgame PROPERTIES
    FOLDER "Tools"
)
