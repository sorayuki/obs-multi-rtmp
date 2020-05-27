cmake_minimum_required(VERSION 3.16)
project(obs-multi-rtmp VERSION 0.2.4)

set (AUTOMOC ON)
set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (QTDIR "" CACHE PATH "Directory of Qt 5, example D:/dev/qt5/5.10.1/msvc2017")
set (CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QTDIR})

find_package(Qt5 COMPONENTS Widgets REQUIRED)

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
elseif (UNIX)
# Linux
    find_package(LibObs REQUIRED)
else ()
    message(FATAL_ERROR "Unsupported OS!")
endif ()

configure_file(src/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

add_library(obs-multi-rtmp SHARED
    src/obs-multi-rtmp.cpp
    src/obs-multi-rtmp.h
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)
target_include_directories(obs-multi-rtmp PRIVATE
    ${OBS_SRC_DIR}/libobs
    ${OBS_SRC_DIR}/UI/obs-frontend-api
)
target_link_directories(obs-multi-rtmp PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(obs-multi-rtmp PRIVATE
    Qt5::Widgets
    libobs
    obs-frontend-api
)

if (WIN32)
    install(TARGETS obs-multi-rtmp RUNTIME
        DESTINATION "obs-plugins/${PLAT}"
    )
    install(DIRECTORY locale
        DESTINATION data/obs-plugins/obs-multi-rtmp
    )
elseif (APPLE)

elseif (UNIX)
    set_target_properties(obs-multi-rtmp PROPERTIES PREFIX "")

    install(TARGETS obs-multi-rtmp LIBRARY
        DESTINATION lib/obs-plugins
    )
    install(DIRECTORY locale
        DESTINATION share/obs/obs-plugins/obs-multi-rtmp
    )
endif ()
