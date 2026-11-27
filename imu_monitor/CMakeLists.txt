cmake_minimum_required(VERSION 3.8)
project(imu_monitor)

find_package(ament_cmake REQUIRED)

install(PROGRAMS scripts/imu_monitor.py DESTINATION lib/${PROJECT_NAME})

ament_package()
