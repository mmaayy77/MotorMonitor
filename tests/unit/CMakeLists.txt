enable_testing()

add_executable(domain_types_test domain_types_test.cpp)
target_link_libraries(domain_types_test motor_common GTest::gtest_main Qt6::Core)
add_test(NAME DomainTypes.all COMMAND domain_types_test)

add_executable(frame_codec_test frame_codec_test.cpp)
target_link_libraries(frame_codec_test motor_common GTest::gtest_main Qt6::Core Qt6::Network)
add_test(NAME FrameCodec.all COMMAND frame_codec_test)

add_executable(message_codec_test message_codec_test.cpp)
target_link_libraries(message_codec_test motor_common GTest::gtest_main Qt6::Core Qt6::Network)
add_test(NAME MessageCodec.all COMMAND message_codec_test)

add_executable(motor_model_test motor_model_test.cpp)
target_link_libraries(motor_model_test motor_simulator_core GTest::gtest_main Qt6::Core)
add_test(NAME MotorModel.all COMMAND motor_model_test)

add_executable(alarm_engine_test alarm_engine_test.cpp)
target_link_libraries(alarm_engine_test motor_common GTest::gtest_main Qt6::Core Qt6::Test)
add_test(NAME AlarmEngine.all COMMAND alarm_engine_test)

add_executable(persistence_test persistence_test.cpp)
target_link_libraries(persistence_test motor_common GTest::gtest_main Qt6::Core Qt6::Sql)
add_test(NAME Persistence.all COMMAND persistence_test)
