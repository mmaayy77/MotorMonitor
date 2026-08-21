add_library(motor_simulator_network STATIC
    device_session.cpp
    device_server.cpp
)
target_include_directories(motor_simulator_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(motor_simulator_network PUBLIC motor_simulator_core Qt6::Core Qt6::Network)
