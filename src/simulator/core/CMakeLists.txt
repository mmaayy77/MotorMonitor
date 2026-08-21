add_library(motor_simulator_core STATIC
    motor_model.cpp
)
target_include_directories(motor_simulator_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(motor_simulator_core PUBLIC motor_common Qt6::Core)
