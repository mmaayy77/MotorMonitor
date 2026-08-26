add_library(motor_client_network STATIC
    communication_worker.cpp
    connection_manager.cpp
    device_connection.cpp
    storage_worker.cpp
)
target_include_directories(motor_client_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(motor_client_network PUBLIC motor_common Qt6::Core Qt6::Network)