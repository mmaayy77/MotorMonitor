add_library(motor_common STATIC
    protocol/crc32.cpp
    protocol/frame_codec.cpp
    protocol/message_codec.cpp
    protocol/custom_protocol_adapter.cpp
    protocol/modbus_tcp_adapter.cpp
    alarm_engine/alarm_engine.cpp
    alarm_engine/standard_rules.cpp
    persistence/sqlite_repository.cpp
)
target_link_libraries(motor_common PUBLIC project_options Qt6::Core Qt6::Network Qt6::Sql)
target_include_directories(motor_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})