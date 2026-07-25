cmake_minimum_required(VERSION 3.20)
project(obs-multi-rtmp-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(obs-multi-rtmp-tests
  main.cpp
  sanity_test.cpp
  # pure-logic sources and their tests are appended here in later tasks
  ../src/stream-format.cpp
  stream_format_test.cpp
  ../src/target-order.cpp
  target_order_test.cpp
  ../src/config-serialization.cpp
  config_serialization_test.cpp
  ../src/stream-health.cpp
  stream_health_test.cpp
  ../src/platform-presets.cpp
  platform_presets_test.cpp
  ../src/watchdog.cpp
  watchdog_test.cpp
)

target_include_directories(obs-multi-rtmp-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../dep/doctest
  ${CMAKE_CURRENT_SOURCE_DIR}/../dep/nlohmann-json
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME unit COMMAND obs-multi-rtmp-tests)
