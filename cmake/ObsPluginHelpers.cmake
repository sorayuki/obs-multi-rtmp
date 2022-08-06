if(POLICY CMP0087)
  cmake_policy(SET CMP0087 NEW)
endif()

set(OBS_STANDALONE_PLUGIN_DIR ${CMAKE_SOURCE_DIR}/release)
set(INCLUDED_LIBOBS_CMAKE_MODULES ON)

include(GNUInstallDirs)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(OS_MACOS ON)
  set(OS_POSIX ON)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD")
  set(OS_POSIX ON)
  string(TOUPPER "${CMAKE_SYSTEM_NAME}" _SYSTEM_NAME_U)
  set(OS_${_SYSTEM_NAME_U} ON)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  set(OS_WINDOWS ON)
  set(OS_POSIX OFF)
endif()

# Old-Style plugin detected, find "modern" libobs variant instead and set global include directories
# to fix "bad" plugin behaviour
if(DEFINED LIBOBS_INCLUDE_DIR AND NOT TARGET OBS::libobs)
  message(
    DEPRECATION
      "You are using an outdated method of adding 'libobs' to your project. Refer to the updated wiki on how to build and export 'libobs' and use it in your plugin projects."
  )
  find_package(libobs REQUIRED)
  if(TARGET OBS::libobs)
    set_target_properties(OBS::libobs PROPERTIES IMPORTED_GLOBAL TRUE)
    message(STATUS "OBS: Using modern libobs target")

    add_library(libobs ALIAS OBS::libobs)
    if(OS_WINDOWS)
      add_library(w32-pthreads ALIAS OBS::w32-pthreads)
    endif()
  endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND (OS_WINDOWS OR OS_MACOS))
  set(CMAKE_INSTALL_PREFIX
      ${OBS_STANDALONE_PLUGIN_DIR}
      CACHE STRING "Directory to install OBS plugin after building" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      "RelWithDebInfo"
      CACHE STRING "OBS build type [Release, RelWithDebInfo, Debug, MinSizeRel]" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release RelWithDebInfo Debug MinSizeRel)
endif()

if(NOT QT_VERSION)
  set(QT_VERSION
      AUTO
      CACHE STRING "OBS Qt version [AUTO, 5, 6]" FORCE)
  set_property(CACHE QT_VERSION PROPERTY STRINGS AUTO 5 6)
endif()

macro(find_qt)
  set(multiValueArgs COMPONENTS COMPONENTS_WIN COMPONENTS_MAC COMPONENTS_LINUX)
  cmake_parse_arguments(FIND_QT "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  set(QT_NO_CREATE_VERSIONLESS_TARGETS ON)
  find_package(
    Qt5
    COMPONENTS Core
    QUIET)
  find_package(
    Qt6
    COMPONENTS Core
    QUIET)

  if(NOT _QT_VERSION AND QT_VERSION STREQUAL AUTO)
    if(TARGET Qt6::Core)
      set(_QT_VERSION
          6
          CACHE INTERNAL "")
    elseif(TARGET Qt5::Core)
      set(_QT_VERSION
          5
          CACHE INTERNAL "")
    endif()
    message(STATUS "Qt version: ${_QT_VERSION}")
  elseif(NOT _QT_VERSION)
    if(TARGET Qt${QT_VERSION}::Core)
      set(_QT_VERSION
          ${QT_VERSION}
          CACHE INTERNAL "")
    else()
      if(QT_VERSION EQUAL 6)
        set(FALLBACK_QT_VERSION 5)
      else()
        set(FALLBACK_QT_VERSION 6)
      endif()
      message(WARNING "Qt${QT_VERSION} was not found, falling back to Qt${FALLBACK_QT_VERSION}")

      if(TARGET Qt${FALLBACK_QT_VERSION}::Core)
        set(_QT_VERSION
            ${FALLBACK_QT_VERSION}
            CACHE INTERNAL "")
      endif()
    endif()
    message(STATUS "Qt version: ${_QT_VERSION}")
  endif()

  set(QT_NO_CREATE_VERSIONLESS_TARGETS OFF)

  if(NOT _QT_VERSION)
    message(FATAL_ERROR "Neither Qt5 or Qt6 were found")
  endif()

  if(OS_WINDOWS)
    find_package(
      Qt${_QT_VERSION}
      COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_WIN}
      REQUIRED)
  elseif(OS_MACOS)
    find_package(
      Qt${_QT_VERSION}
      COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_MAC}
      REQUIRED)
  else()
    find_package(
      Qt${_QT_VERSION}
      COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_LINUX}
      REQUIRED)
  endif()

  list(APPEND FIND_QT_COMPONENTS "Core")

  if("Gui" IN_LIST FIND_QT_COMPONENTS_LINUX)
    list(APPEND FIND_QT_COMPONENTS_LINUX "GuiPrivate")
  endif()

  foreach(_COMPONENT IN LISTS FIND_QT_COMPONENTS FIND_QT_COMPONENTS_WIN FIND_QT_COMPONENTS_MAC
                              FIND_QT_COMPONENTS_LINUX)
    if(NOT TARGET Qt::${_COMPONENT} AND TARGET Qt${_QT_VERSION}::${_COMPONENT})

      add_library(Qt::${_COMPONENT} INTERFACE IMPORTED)
      set_target_properties(Qt::${_COMPONENT} PROPERTIES INTERFACE_LINK_LIBRARIES
                                                         "Qt${_QT_VERSION}::${_COMPONENT}")
    endif()
  endforeach()
endmacro()

file(RELATIVE_PATH RELATIVE_INSTALL_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_INSTALL_PREFIX})
file(RELATIVE_PATH RELATIVE_BUILD_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

# Set up OS-specific environment and helper functions
if(OS_POSIX)
  find_program(CCACHE_PROGRAM "ccache")
  set(CCACHE_SUPPORT
      ON
      CACHE BOOL "Enable ccache support")
  mark_as_advanced(CCACHE_PROGRAM)
  if(CCACHE_PROGRAM AND CCACHE_SUPPORT)
    set(CMAKE_CXX_COMPILER_LAUNCHER
        ${CCACHE_PROGRAM}
        CACHE INTERNAL "")
    set(CMAKE_C_COMPILER_LAUNCHER
        ${CCACHE_PROGRAM}
        CACHE INTERNAL "")
    set(CMAKE_OBJC_COMPILER_LAUNCHER
        ${CCACHE_PROGRAM}
        CACHE INTERNAL "")
    set(CMAKE_OBJCXX_COMPILER_LAUNCHER
        ${CCACHE_PROGRAM}
        CACHE INTERNAL "")
    set(CMAKE_CUDA_COMPILER_LAUNCHER
        ${CCACHE_PROGRAM}
        CACHE INTERNAL "") # CMake 3.9+
  endif()
endif()

if(OS_MACOS)
  set(CMAKE_OSX_ARCHITECTURES
      "x86_64"
      CACHE STRING "OBS build architecture for macOS - x86_64 required at least")
  set_property(CACHE CMAKE_OSX_ARCHITECTURES PROPERTY STRINGS x86_64 arm64 "x86_64;arm64")

  set(CMAKE_OSX_DEPLOYMENT_TARGET
      "10.15"
      CACHE STRING "OBS deployment target for macOS - 10.15+ required")
  set_property(CACHE CMAKE_OSX_DEPLOYMENT_TARGET PROPERTY STRINGS 10.15 11.0 12.0 13.0)

  set(OBS_BUNDLE_CODESIGN_IDENTITY
      "-"
      CACHE STRING "OBS code signing identity for macOS")
  set(OBS_CODESIGN_ENTITLEMENTS
      ${CMAKE_SOURCE_DIR}/cmake/bundle/macos/entitlements.plist
      CACHE INTERNAL "Path to codesign entitlements plist")
  set(OBS_CODESIGN_LINKER
      ON
      CACHE BOOL "Enable linker code-signing on macOS (macOS 11+ required)")

  # Xcode configuration
  if(XCODE)
    # Tell Xcode to pretend the linker signed binaries so that editing with install_name_tool
    # preserves ad-hoc signatures. This option is supported by codesign on macOS 11 or higher. See
    # CMake Issue 21854: https://gitlab.kitware.com/cmake/cmake/-/issues/21854

    set(CMAKE_XCODE_GENERATE_SCHEME ON)
    if(OBS_CODESIGN_LINKER)
      set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "-o linker-signed")
    endif()
  endif()

  # Set default options for bundling on macOS
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_SKIP_BUILD_RPATH OFF)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
  set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks/")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)

  function(setup_plugin_target target)
    if(NOT DEFINED MACOSX_PLUGIN_GUI_IDENTIFIER)
      message(
        FATAL_ERROR
          "No 'MACOSX_PLUGIN_GUI_IDENTIFIER' set, but is required to build plugin bundles on macOS - example: 'com.yourname.pluginname'"
      )
    endif()

    if(NOT DEFINED MACOSX_PLUGIN_BUNDLE_VERSION)
      message(
        FATAL_ERROR
          "No 'MACOSX_PLUGIN_BUNDLE_VERSION' set, but is required to build plugin bundles on macOS - example: '25'"
      )
    endif()

    if(NOT DEFINED MACOSX_PLUGIN_SHORT_VERSION_STRING)
      message(
        FATAL_ERROR
          "No 'MACOSX_PLUGIN_SHORT_VERSION_STRING' set, but is required to build plugin bundles on macOS - example: '1.0.2'"
      )
    endif()

    set(MACOSX_PLUGIN_BUNDLE_NAME
        "${target}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_BUNDLE_VERSION
        "${MACOSX_BUNDLE_BUNDLE_VERSION}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_SHORT_VERSION_STRING
        "${MACOSX_BUNDLE_SHORT_VERSION_STRING}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_EXECUTABLE_NAME
        "${target}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_BUNDLE_TYPE
        "BNDL"
        PARENT_SCOPE)

    install(
      TARGETS ${target}
      LIBRARY DESTINATION "."
              COMPONENT obs_plugins
              NAMELINK_COMPONENT ${target}_Development)

    if(${QT_VERSION} EQUAL 5)
      set(_QT_FW_VERSION "${QT_VERSION}")
    else()
      set(_QT_FW_VERSION "A")
    endif()

    set(_COMMAND
        "${CMAKE_INSTALL_NAME_TOOL} \\
      -change ${CMAKE_PREFIX_PATH}/lib/QtWidgets.framework/Versions/${QT_VERSION}/QtWidgets @rpath/QtWidgets.framework/Versions/${_QT_FW_VERSION}/QtWidgets \\
      -change ${CMAKE_PREFIX_PATH}/lib/QtCore.framework/Versions/${QT_VERSION}/QtCore @rpath/QtCore.framework/Versions/${_QT_FW_VERSION}/QtCore \\
      -change ${CMAKE_PREFIX_PATH}/lib/QtGui.framework/Versions/${QT_VERSION}/QtGui @rpath/QtGui.framework/Versions/${_QT_FW_VERSION}/QtGui \\
      \\\"\${CMAKE_INSTALL_PREFIX}/${target}.plugin/Contents/MacOS/${target}\\\"")
    install(CODE "execute_process(COMMAND /bin/sh -c \"${_COMMAND}\")" COMPONENT obs_plugins)
    unset(_QT_FW_VERSION)

    if(NOT XCODE)
      set(_COMMAND
          "/usr/bin/codesign --force \\
          --sign \\\"${OBS_BUNDLE_CODESIGN_IDENTITY}\\\" \\
          --options runtime \\
          --entitlements \\\"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bundle/macOS/entitlements.plist\\\" \\
          \\\"\${CMAKE_INSTALL_PREFIX}/${target}.plugin\\\"")
      install(CODE "execute_process(COMMAND /bin/sh -c \"${_COMMAND}\")" COMPONENT obs_plugins)
    endif()

    set_target_properties(
      ${target}
      PROPERTIES BUNDLE ON
                 BUNDLE_EXTENSION "plugin"
                 OUTPUT_NAME ${target}
                 MACOSX_BUNDLE_INFO_PLIST
                 "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bundle/macOS/Plugin-Info.plist.in"
                 XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${MACOSX_PLUGIN_GUI_IDENTIFIER}"
                 XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${OBS_BUNDLE_CODESIGN_IDENTITY}"
                 XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
                 "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bundle/macOS/entitlements.plist")

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        /bin/sh -c
        "codesign --force --sign \"-\" $<$<BOOL:${OBS_CODESIGN_LINKER}>:--options linker-signed >\"$<TARGET_BUNDLE_DIR:${target}>\""
      COMMENT "Codesigning ${target}"
      VERBATIM)

    install_bundle_resources(${target})
  endfunction()

  function(install_bundle_resources target)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
      file(GLOB_RECURSE _DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
      foreach(_DATA_FILE IN LISTS _DATA_FILES)
        file(RELATIVE_PATH _RELATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data/ ${_DATA_FILE})
        get_filename_component(_RELATIVE_PATH ${_RELATIVE_PATH} PATH)
        target_sources(${target} PRIVATE ${_DATA_FILE})
        set_source_files_properties(${_DATA_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION
                                                             Resources/${_RELATIVE_PATH})
        string(REPLACE "\\" "\\\\" _GROUP_NAME ${_RELATIVE_PATH})
        source_group("Resources\\${_GROUP_NAME}" FILES ${_DATA_FILE})
      endforeach()
    endif()
  endfunction()
else()
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_ARCH_SUFFIX 64)
  else()
    set(_ARCH_SUFFIX 32)
  endif()
  set(OBS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/rundir)

  if(OS_POSIX)
    option(LINUX_PORTABLE "Build portable version (Linux)" ON)
    if(NOT LINUX_PORTABLE)
      set(OBS_LIBRARY_DESTINATION ${CMAKE_INSTALL_LIBDIR})
      set(OBS_PLUGIN_DESTINATION ${OBS_LIBRARY_DESTINATION}/obs-plugins)
      set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
      set(OBS_DATA_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/obs)
    else()
      set(OBS_LIBRARY_DESTINATION bin/${_ARCH_SUFFIX}bit)
      set(OBS_PLUGIN_DESTINATION obs-plugins/${_ARCH_SUFFIX}bit)
      set(CMAKE_INSTALL_RPATH "$ORIGIN/" "${CMAKE_INSTALL_PREFIX}/${OBS_LIBRARY_DESTINATION}")
      set(OBS_DATA_DESTINATION "data")
    endif()

    if(OS_LINUX)
      set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
      set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${LINUX_MAINTAINER_EMAIL}")
      set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
      set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-linux-x86_64")

      set(CPACK_GENERATOR "DEB")
      set(CPACK_DEBIAN_PACKAGE_DEPENDS
          "obs-studio (>= 27.0.0), libqt5core5a (>= 5.9.0~beta), libqt5gui5 (>= 5.3.0), libqt5widgets5 (>= 5.7.0)"
      )

      set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_SOURCE_DIR}/release)

      if(NOT LINUX_PORTABLE)
        set(CPACK_SET_DESTDIR ON)
      endif()
      include(CPack)
    endif()
  else()
    set(OBS_LIBRARY_DESTINATION "bin/${_ARCH_SUFFIX}bit")
    set(OBS_LIBRARY32_DESTINATION "bin/32bit")
    set(OBS_LIBRARY64_DESTINATION "bin/64bit")
    set(OBS_PLUGIN_DESTINATION "obs-plugins/${_ARCH_SUFFIX}bit")
    set(OBS_PLUGIN32_DESTINATION "obs-plugins/32bit")
    set(OBS_PLUGIN64_DESTINATION "obs-plugins/64bit")

    set(OBS_DATA_DESTINATION "data")
  endif()

  function(setup_plugin_target target)
    set_target_properties(${target} PROPERTIES PREFIX "")

    install(
      TARGETS ${target}
      RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}" COMPONENT ${target}_Runtime
      LIBRARY DESTINATION "${OBS_PLUGIN_DESTINATION}"
              COMPONENT ${target}_Runtime
              NAMELINK_COMPONENT ${target}_Development)

    install(
      FILES $<TARGET_FILE:${target}>
      DESTINATION $<CONFIG>/${OBS_PLUGIN_DESTINATION}
      COMPONENT obs_rundir
      EXCLUDE_FROM_ALL)

    if(OS_WINDOWS)
      install(
        FILES $<TARGET_PDB_FILE:${target}>
        CONFIGURATIONS "RelWithDebInfo" "Debug"
        DESTINATION ${OBS_PLUGIN_DESTINATION}
        COMPONENT ${target}_Runtime
        OPTIONAL)

      install(
        FILES $<TARGET_PDB_FILE:${target}>
        CONFIGURATIONS "RelWithDebInfo" "Debug"
        DESTINATION $<CONFIG>/${OBS_PLUGIN_DESTINATION}
        COMPONENT obs_rundir
        OPTIONAL EXCLUDE_FROM_ALL)
    endif()

    if(MSVC)
      target_link_options(
        ${target}
        PRIVATE
        "LINKER:/OPT:REF"
        "$<$<NOT:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>>:LINKER\:/SAFESEH\:NO>"
        "$<$<CONFIG:DEBUG>:LINKER\:/INCREMENTAL:NO>"
        "$<$<CONFIG:RELWITHDEBINFO>:LINKER\:/INCREMENTAL:NO>")
    endif()

    setup_target_resources(${target} obs-plugins/${target})

    if(OS_WINDOWS AND DEFINED OBS_BUILD_DIR)
      setup_target_for_testing(${target} obs-plugins/${target})
    endif()

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -DCMAKE_INSTALL_PREFIX=${OBS_OUTPUT_DIR}
        -DCMAKE_INSTALL_COMPONENT=obs_rundir -DCMAKE_INSTALL_CONFIG_NAME=$<CONFIG> -P
        ${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake
      COMMENT "Installing to plugin rundir"
      VERBATIM)
  endfunction()

  function(setup_target_resources target destination)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
      install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
        DESTINATION ${OBS_DATA_DESTINATION}/${destination}
        USE_SOURCE_PERMISSIONS
        COMPONENT obs_plugins)

      install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
        DESTINATION $<CONFIG>/${OBS_DATA_DESTINATION}/${destination}
        USE_SOURCE_PERMISSIONS
        COMPONENT obs_rundir
        EXCLUDE_FROM_ALL)
    endif()
  endfunction()

  if(OS_WINDOWS)
    function(setup_target_for_testing target destination)
      install(
        FILES $<TARGET_FILE:${target}>
        DESTINATION $<CONFIG>/${OBS_PLUGIN_DESTINATION}
        COMPONENT obs_testing
        EXCLUDE_FROM_ALL)

      install(
        FILES $<TARGET_PDB_FILE:${target}>
        CONFIGURATIONS "RelWithDebInfo" "Debug"
        DESTINATION $<CONFIG>/${OBS_PLUGIN_DESTINATION}
        COMPONENT obs_testing
        OPTIONAL EXCLUDE_FROM_ALL)

      install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
        DESTINATION $<CONFIG>/${OBS_DATA_DESTINATION}/${destination}
        USE_SOURCE_PERMISSIONS
        COMPONENT obs_testing
        EXCLUDE_FROM_ALL)

      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -DCMAKE_INSTALL_PREFIX=${OBS_BUILD_DIR}/rundir
          -DCMAKE_INSTALL_COMPONENT=obs_testing -DCMAKE_INSTALL_CONFIG_NAME=$<CONFIG> -P
          ${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake
        COMMENT "Installing to OBS test directory"
        VERBATIM)
    endfunction()
  endif()
endif()
