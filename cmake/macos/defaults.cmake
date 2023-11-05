# CMake macOS defaults module

include_guard(GLOBAL)

# Set empty codesigning team if not specified as cache variable
if(NOT CODESIGN_TEAM)
  set(CODESIGN_TEAM
      ""
      CACHE STRING "OBS code signing team for macOS" FORCE)

  # Set ad-hoc codesigning identity if not specified as cache variable
  if(NOT CODESIGN_IDENTITY)
    set(CODESIGN_IDENTITY
        "-"
        CACHE STRING "OBS code signing identity for macOS" FORCE)
  endif()
endif()

if(XCODE)
  include(xcode)
endif()

include(buildspec)

# Set default deployment target to 11.0 if not set and enable selection in GUI up to 13.0
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET
      11.0
      CACHE STRING "Minimum macOS version to target for deployment (at runtime). Newer APIs will be weak-linked." FORCE)
endif()
set_property(CACHE CMAKE_OSX_DEPLOYMENT_TARGET PROPERTY STRINGS 13.0 12.0 11.0)

# Use Applications directory as default install destination
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "$ENV{HOME}/Library/Application Support/obs-studio/plugins"
      CACHE STRING "Directory to install OBS after building" FORCE)
endif()

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
# Enable RPATH support for generated binaries
set(CMAKE_MACOSX_RPATH TRUE)
# Use RPATHs from build tree _in_ the build tree
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
# Do not add default linker search paths to RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
# Use common bundle-relative RPATH for installed targets
set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
