add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wconversion>)

option(MOTOR_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)
if(MOTOR_ENABLE_SANITIZERS AND NOT MSVC)
  target_compile_options(project_options INTERFACE -fsanitize=address,undefined)
  target_link_options(project_options INTERFACE -fsanitize=address,undefined)
endif()
