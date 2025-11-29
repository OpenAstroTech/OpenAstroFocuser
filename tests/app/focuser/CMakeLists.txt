cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(focuser_app_test)

set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

target_sources(app PRIVATE
  src/main.cpp
  ${APP_ROOT}/app/src/Focuser.cpp
  ${APP_ROOT}/app/src/ZephyrStepper.cpp
)

target_include_directories(app PRIVATE
  ${APP_ROOT}/app/src
  ${CMAKE_CURRENT_LIST_DIR}/include
)

zephyr_compile_definitions(FFF_ARG_HISTORY_LEN=4)
