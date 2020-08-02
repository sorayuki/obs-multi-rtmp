cmake_minimum_required(VERSION 3.16)
project(obs-multi-rtmp VERSION 0.2.5)

set(QTDIR "${QTDIR}" CACHE PATH "Path to qt directory")

list(APPEND CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

set(LIB_OBS libobs)

# build obs lib
if (WIN32)
    # Windows
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(PLAT "32bit")
        set(LINK_PLAT "x86")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PLAT "64bit")
        set(LINK_PLAT "x64")
    else ()
        message(FATAL_ERROR "Unsupported Architecture!")
    endif ()

    if (NOT DEFINED OBS_SRC_DIR)
        set(OBS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obs-src")
    endif ()
    if (NOT DEFINED OBS_BIN_DIR)
        set(OBS_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obs-bin")
    endif ()

    if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "${OBS_BIN_DIR}" CACHE PATH "" FORCE)
    endif ()

    execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/pexports.exe"
        "${OBS_BIN_DIR}/bin/${PLAT}/obs.dll"
        OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/libobs.def"
        COMMAND_ECHO STDOUT
    )
    execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/pexports.exe"
        "${OBS_BIN_DIR}/bin/${PLAT}/obs-frontend-api.dll"
        OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/obs-frontend-api.def"
        COMMAND_ECHO STDOUT
    )
    execute_process(COMMAND ${CMAKE_LINKER}
        "/lib" "/MACHINE:${LINK_PLAT}"
        "/def:${CMAKE_CURRENT_BINARY_DIR}/libobs.def"
        "/out:${CMAKE_CURRENT_BINARY_DIR}/libobs.lib"
    )
    execute_process(COMMAND ${CMAKE_LINKER}
        "/lib" "/MACHINE:${LINK_PLAT}"
        "/def:${CMAKE_CURRENT_BINARY_DIR}/obs-frontend-api.def"
        "/out:${CMAKE_CURRENT_BINARY_DIR}/obs-frontend-api.lib"
    )
elseif (APPLE)
    # macOS
    find_library(_LIB_OBS "${LIB_OBS}.0.dylib" PATHS ${OBS_BIN_DIR} REQUIRED)
    set(LIB_OBS "${_LIB_OBS}")
elseif (UNIX)
    # Linux
    find_package(LibObs REQUIRED)
else ()
    message(FATAL_ERROR "Unsupported OS!")
endif ()

add_library(obs-multi-rtmp SHARED
    "src/obs-multi-rtmp.cpp"
    "src/obs-multi-rtmp.h"
)
if (WIN32)
    configure_file("src/version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    target_sources(obs-multi-rtmp PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    )
endif ()
target_include_directories(obs-multi-rtmp PRIVATE
    "${OBS_SRC_DIR}/libobs"
    "${OBS_SRC_DIR}/UI/obs-frontend-api"
)
target_link_directories(obs-multi-rtmp PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    ${OBS_BIN_DIR}
)
target_link_libraries(obs-multi-rtmp PRIVATE
    Qt5::Widgets
    ${LIB_OBS}
    obs-frontend-api
)

if (WIN32)
    install(TARGETS obs-multi-rtmp RUNTIME
        DESTINATION "obs-plugins/${PLAT}"
    )
    install(DIRECTORY locale
        DESTINATION "data/obs-plugins/obs-multi-rtmp"
    )
elseif (APPLE)
    set_target_properties(obs-multi-rtmp PROPERTIES
        PREFIX ""
        SUFFIX ".so"
    )

    install(TARGETS obs-multi-rtmp LIBRARY
        DESTINATION "obs-studio/plugins/obs-multi-rtmp/bin"
    )
    install(DIRECTORY locale
        DESTINATION "obs-studio/plugins/obs-multi-rtmp/data"
    )
elseif (UNIX)
    set_target_properties(obs-multi-rtmp PROPERTIES PREFIX "")

    install(TARGETS obs-multi-rtmp LIBRARY
        DESTINATION "lib/obs-plugins"
    )
    install(DIRECTORY locale
        DESTINATION "share/obs/obs-plugins/obs-multi-rtmp"
    )
endif ()
