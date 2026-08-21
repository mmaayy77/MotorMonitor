add_library(motor_client_network STATIC
    device_connection.cpp
    connection_manager.cpp
)
target_include_directories(motor_client_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(motor_client_network PUBLIC motor_common Qt6::Core Qt6::Network)
