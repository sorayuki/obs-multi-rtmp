# CMake common helper functions module

# cmake-format: off
# cmake-lint: disable=C0103
# cmake-format: on

include_guard(GLOBAL)

# * Use QT_VERSION value as a hint for desired Qt version
# * If "AUTO" was specified, prefer Qt6 over Qt5
# * Creates versionless targets of desired component if none had been created by Qt itself (Qt versions < 5.15)
if(NOT QT_VERSION)
  set(QT_VERSION
      AUTO
      CACHE STRING "OBS Qt version [AUTO, 5, 6]" FORCE)
  set_property(CACHE QT_VERSION PROPERTY STRINGS AUTO 5 6)
endif()

# find_qt: Macro to find best possible Qt version for use with the project:
macro(find_qt)
  set(multiValueArgs COMPONENTS COMPONENTS_WIN COMPONENTS_MAC COMPONENTS_LINUX)
  cmake_parse_arguments(find_qt "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Do not use versionless targets in the first step to avoid Qt::Core being clobbered by later opportunistic
  # find_package runs
  set(QT_NO_CREATE_VERSIONLESS_TARGETS TRUE)

  message(DEBUG "Start Qt version discovery...")
  # Loop until _QT_VERSION is set or FATAL_ERROR aborts script execution early
  while(NOT _QT_VERSION)
    message(DEBUG "QT_VERSION set to ${QT_VERSION}")
    if(QT_VERSION STREQUAL AUTO AND NOT qt_test_version)
      set(qt_test_version 6)
    elseif(NOT QT_VERSION STREQUAL AUTO)
      set(qt_test_version ${QT_VERSION})
    endif()
    message(DEBUG "Attempting to find Qt${qt_test_version}")

    find_package(
      Qt${qt_test_version}
      COMPONENTS Core
      QUIET)

    if(TARGET Qt${qt_test_version}::Core)
      set(_QT_VERSION
          ${qt_test_version}
          CACHE INTERNAL "")
      message(STATUS "Qt version found: ${_QT_VERSION}")
      unset(qt_test_version)
      break()
    elseif(QT_VERSION STREQUAL AUTO)
      if(qt_test_version EQUAL 6)
        message(WARNING "Qt6 was not found, falling back to Qt5")
        set(qt_test_version 5)
        continue()
      endif()
    endif()
    message(FATAL_ERROR "Neither Qt6 nor Qt5 found.")
  endwhile()

  # Enable versionless targets for the remaining Qt components
  set(QT_NO_CREATE_VERSIONLESS_TARGETS FALSE)

  set(qt_components ${find_qt_COMPONENTS})
  if(OS_WINDOWS)
    list(APPEND qt_components ${find_qt_COMPONENTS_WIN})
  elseif(OS_MACOS)
    list(APPEND qt_components ${find_qt_COMPONENTS_MAC})
  else()
    list(APPEND qt_components ${find_qt_COMPONENTS_LINUX})
  endif()
  message(DEBUG "Trying to find Qt components ${qt_components}...")

  find_package(Qt${_QT_VERSION} REQUIRED ${qt_components})

  list(APPEND qt_components Core)

  if("Gui" IN_LIST find_qt_COMPONENTS_LINUX)
    list(APPEND qt_components "GuiPrivate")
  endif()

  # Check for versionless targets of each requested component and create if necessary
  foreach(component IN LISTS qt_components)
    message(DEBUG "Checking for target Qt::${component}")
    if(NOT TARGET Qt::${component} AND TARGET Qt${_QT_VERSION}::${component})
      add_library(Qt::${component} INTERFACE IMPORTED)
      set_target_properties(Qt::${component} PROPERTIES INTERFACE_LINK_LIBRARIES Qt${_QT_VERSION}::${component})
    endif()
    set_property(TARGET Qt::${component} PROPERTY INTERFACE_COMPILE_FEATURES "")
  endforeach()

endmacro()

# check_uuid: Helper function to check for valid UUID
function(check_uuid uuid_string return_value)
  set(valid_uuid TRUE)
  set(uuid_token_lengths 8 4 4 4 12)
  set(token_num 0)

  string(REPLACE "-" ";" uuid_tokens ${uuid_string})
  list(LENGTH uuid_tokens uuid_num_tokens)

  if(uuid_num_tokens EQUAL 5)
    message(DEBUG "UUID ${uuid_string} is valid with 5 tokens.")
    foreach(uuid_token IN LISTS uuid_tokens)
      list(GET uuid_token_lengths ${token_num} uuid_target_length)
      string(LENGTH "${uuid_token}" uuid_actual_length)
      if(uuid_actual_length EQUAL uuid_target_length)
        string(REGEX MATCH "[0-9a-fA-F]+" uuid_hex_match ${uuid_token})
        if(NOT uuid_hex_match STREQUAL uuid_token)
          set(valid_uuid FALSE)
          break()
        endif()
      else()
        set(valid_uuid FALSE)
        break()
      endif()
      math(EXPR token_num "${token_num}+1")
    endforeach()
  else()
    set(valid_uuid FALSE)
  endif()
  message(DEBUG "UUID ${uuid_string} valid: ${valid_uuid}")
  set(${return_value}
      ${valid_uuid}
      PARENT_SCOPE)
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-support.c.in")
  configure_file(src/plugin-support.c.in plugin-support.c @ONLY)
  add_library(plugin-support STATIC)
  target_sources(
    plugin-support
    PRIVATE plugin-support.c
    PUBLIC src/plugin-support.h)
  target_include_directories(plugin-support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
  if(OS_LINUX
     OR OS_FREEBSD
     OR OS_OPENBSD)
    # add fPIC on Linux to prevent shared object errors
    set_property(TARGET plugin-support PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
endif()
