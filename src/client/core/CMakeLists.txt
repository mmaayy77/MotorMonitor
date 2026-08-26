add_library(motor_client_core STATIC
    app_config.cpp
)
target_include_directories(motor_client_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(motor_client_core PUBLIC motor_common Qt6::Core)