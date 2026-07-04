set(GAME_NAME "network_test")

#### Includes ####

add_library(${GAME_NAME}_include INTERFACE)
target_include_directories(${GAME_NAME}_include INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)

#### Client ####

add_library(${GAME_NAME}_client_lib STATIC
    Client/NetworkTestClient.cpp
)

target_link_libraries(${GAME_NAME}_client_lib PUBLIC
    engine_options
    engine_client
    ${GAME_NAME}_include
)

add_executable(${GAME_NAME}_client
    main_client.cpp
)

target_link_libraries(${GAME_NAME}_client PRIVATE
    ${GAME_NAME}_client_lib
)

#### Server ####

add_library(${GAME_NAME}_server_lib STATIC
    Server/NetworkTestServer.cpp
)

target_link_libraries(${GAME_NAME}_server_lib PUBLIC
    engine_options
    engine_server
    ${GAME_NAME}_include
)

add_executable(${GAME_NAME}_server
    main_server.cpp
)

target_link_libraries(${GAME_NAME}_server PRIVATE
    ${GAME_NAME}_server_lib
)
