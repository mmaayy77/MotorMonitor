enable_testing()

add_executable(network_integration_test network_integration_test.cpp)
target_link_libraries(network_integration_test
    motor_client_network
    motor_simulator_network
    motor_simulator_core
    motor_common
    GTest::gtest_main
    Qt6::Core
    Qt6::Network
)
add_test(NAME NetworkIntegration.all COMMAND network_integration_test)
