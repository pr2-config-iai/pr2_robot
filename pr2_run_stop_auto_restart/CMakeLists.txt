cmake_minimum_required(VERSION 3.16)
project(pr2_run_stop_auto_restart)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(pr2_msgs REQUIRED)
find_package(pr2_power_board REQUIRED)
find_package(std_srvs REQUIRED)

add_executable(run_stop_auto_restart src/run_stop_auto_restart.cpp)
target_link_libraries(run_stop_auto_restart
  rclcpp::rclcpp
  ${pr2_msgs_TARGETS}
  ${pr2_power_board_TARGETS}
  ${std_srvs_TARGETS})

install(TARGETS run_stop_auto_restart
  DESTINATION lib/${PROJECT_NAME})

ament_package()
