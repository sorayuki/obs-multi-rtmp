# CMake Linux compiler configuration module

include_guard(GLOBAL)

include(ccache)
include(compiler_common)

option(ENABLE_COMPILER_TRACE "Enable Clang time-trace (required Clang and Ninja)" OFF)
mark_as_advanced(ENABLE_COMPILER_TRACE)

# gcc options for C
set(_obs_gcc_c_options
    # cmake-format: sortable
    -fno-strict-aliasing
    -fopenmp-simd
    -Wdeprecated-declarations
    -Wempty-body
    -Wenum-conversion
    -Werror=return-type
    -Wextra
    -Wformat
    -Wformat-security
    -Wno-conversion
    -Wno-float-conversion
    -Wno-implicit-fallthrough
    -Wno-missing-braces
    -Wno-missing-field-initializers
    -Wno-shadow
    -Wno-sign-conversion
    -Wno-trigraphs
    -Wno-unknown-pragmas
    -Wno-unused-function
    -Wno-unused-label
    -Wparentheses
    -Wshadow
    -Wuninitialized
    -Wunreachable-code
    -Wunused-parameter
    -Wunused-value
    -Wunused-variable
    -Wvla)

# gcc options for C++
set(_obs_gcc_cxx_options
    # cmake-format: sortable
    ${_obs_gcc_c_options} -Wconversion -Wfloat-conversion -Winvalid-offsetof -Wno-overloaded-virtual)

add_compile_options(
  -fopenmp-simd
  "$<$<COMPILE_LANG_AND_ID:C,GNU>:${_obs_gcc_c_options}>"
  "$<$<COMPILE_LANG_AND_ID:C,GNU>:-Wint-conversion;-Wno-missing-prototypes;-Wno-strict-prototypes;-Wpointer-sign>"
  "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:${_obs_gcc_cxx_options}>"
  "$<$<COMPILE_LANG_AND_ID:C,Clang>:${_obs_clang_c_options}>"
  "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:${_obs_clang_cxx_options}>")

# Add support for color diagnostics and CMake switch for warnings as errors to CMake < 3.24
if(CMAKE_VERSION VERSION_LESS 3.24.0)
  add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics> $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>)
  if(CMAKE_COMPILE_WARNING_AS_ERROR)
    add_compile_options(-Werror)
  endif()
else()
  set(CMAKE_COLOR_DIAGNOSTICS ON)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  # Disable false-positive warning in GCC 12.1.0 and later
  add_compile_options(-Wno-error=maybe-uninitialized)

  # Add warning for infinite recursion (added in GCC 12)
  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0.0)
    add_compile_options(-Winfinite-recursion)
  endif()
endif()

# Enable compiler and build tracing (requires Ninja generator)
if(ENABLE_COMPILER_TRACE AND CMAKE_GENERATOR STREQUAL "Ninja")
  add_compile_options($<$<COMPILE_LANG_AND_ID:C,Clang>:-ftime-trace> $<$<COMPILE_LANG_AND_ID:CXX,Clang>:-ftime-trace>)
else()
  set(ENABLE_COMPILER_TRACE
      OFF
      CACHE STRING "Enable Clang time-trace (required Clang and Ninja)" FORCE)
endif()

add_compile_definitions($<$<CONFIG:DEBUG>:DEBUG> $<$<CONFIG:DEBUG>:_DEBUG> SIMDE_ENABLE_OPENMP)
