cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/libobs)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/deps/w32-pthreads)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-studio-31.0.0/build_x64/UI/obs-frontend-api)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  ./src/pch.h
  ./src/obs-multi-rtmp.h
  ./src/obs-multi-rtmp.cpp 
  ./src/edit-widget.h
  ./src/edit-widget.cpp
  ./src/push-widget.h
  ./src/push-widget.cpp
  ./src/json-util.hpp
  ./src/output-config.h
  ./src/output-config.cpp
  ./src/output-config-legacy.cpp
  ./src/obs-properties-widget.h
  ./src/obs-properties-widget.cpp
  ./src/protocols.h
  ./src/protocols.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ./dep/nlohmann-json
  .deps/obs-studio-30.0.0/libobs
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if (WIN32)
  install(FILES archive-readme.txt DESTINATION . RENAME Readme.txt)
  install(FILES archive-copybat.txt DESTINATION . RENAME Copy-Into-Here-To-Install.bat)
endif ()