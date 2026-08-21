add_subdirectory(core)
add_subdirectory(network)
add_subdirectory(ui)

add_executable(motor_monitor_client main.cpp)
target_link_libraries(motor_monitor_client PUBLIC motor_common motor_client_core motor_client_network motor_client_ui Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql)
