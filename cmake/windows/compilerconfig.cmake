# CMake Windows compiler configuration module

include_guard(GLOBAL)

include(compiler_common)

set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)

message(DEBUG "Current Windows API version: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION_MAXIMUM)
  message(DEBUG "Maximum Windows API version: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION_MAXIMUM}")
endif()

if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS 10.0.20348)
  message(
    FATAL_ERROR
    "OBS requires Windows 10 SDK version 10.0.20348.0 or more recent.\n"
    "Please download and install the most recent Windows platform SDK."
  )
endif()

set(_obs_msvc_c_options /MP /Zc:__cplusplus /Zc:preprocessor)
set(_obs_msvc_cpp_options /MP /Zc:__cplusplus /Zc:preprocessor)

if(CMAKE_CXX_STANDARD GREATER_EQUAL 20)
  list(APPEND _obs_msvc_cpp_options /Zc:char8_t-)
endif()

add_compile_options(
  /W3
  /utf-8
  /Brepro
  /permissive-
  "$<$<COMPILE_LANG_AND_ID:C,MSVC>:${_obs_msvc_c_options}>"
  "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${_obs_msvc_cpp_options}>"
  "$<$<COMPILE_LANG_AND_ID:C,Clang>:${_obs_clang_c_options}>"
  "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:${_obs_clang_cxx_options}>"
  $<$<NOT:$<CONFIG:Debug>>:/Gy>
  $<$<NOT:$<CONFIG:Debug>>:/GL>
  $<$<NOT:$<CONFIG:Debug>>:/Oi>
)

add_compile_definitions(
  UNICODE
  _UNICODE
  _CRT_SECURE_NO_WARNINGS
  _CRT_NONSTDC_NO_WARNINGS
  $<$<CONFIG:DEBUG>:DEBUG>
  $<$<CONFIG:DEBUG>:_DEBUG>
)

add_link_options(
  $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
  $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>
  $<$<NOT:$<CONFIG:Debug>>:/LTCG>
  $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>
  /DEBUG
  /Brepro
)

if(CMAKE_COMPILE_WARNING_AS_ERROR)
  add_link_options(/WX)
endif()
