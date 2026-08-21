add_subdirectory(core)
add_subdirectory(network)

add_executable(motor_simulator main.cpp)
target_link_libraries(motor_simulator PRIVATE motor_simulator_core motor_simulator_network Qt6::Core Qt6::Network)
