add_library(motor_client_ui STATIC
    device_config_dialog.cpp
    device_list_model.cpp
    history_page.cpp
    log_page.cpp
    main_window.cpp
    performance_panel.cpp
    realtime_chart.cpp
)
target_include_directories(motor_client_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(motor_client_ui PUBLIC motor_client_network motor_common Qt6::Core Qt6::Widgets Qt6::Sql)