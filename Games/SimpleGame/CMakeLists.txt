set(GAME_NAME "simple_game")

#### Includes ####

add_library(${GAME_NAME}_include INTERFACE)
target_include_directories(${GAME_NAME}_include INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)

#### Client ####

add_library(${GAME_NAME}_client_lib STATIC
    Example.cpp
)

target_link_libraries(${GAME_NAME}_client_lib PUBLIC
    engine_options
    engine_client
    ${GAME_NAME}_include
)

add_executable(${GAME_NAME}_client
    main.cpp
)

target_link_libraries(${GAME_NAME}_client PRIVATE
    ${GAME_NAME}_client_lib
)
