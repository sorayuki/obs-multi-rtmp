cmake_minimum_required(VERSION 3.16)
project(obs-multi-rtmp VERSION 0.2.8.0)

option(PORTABLE_MODE "if obs is used in portable mode" OFF)
set(QTDIR "${QTDIR}" CACHE PATH "Path to qt directory")

list(APPEND CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(PLAT "32bit")
    set(LINK_PLAT "x86")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLAT "64bit")
    set(LINK_PLAT "x64")
else ()
    message(FATAL_ERROR "Unsupported Architecture!")
endif ()

# build obs lib
if (WIN32)
    # Windows
    set(LIB_OBS libobs)

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    if (NOT DEFINED OBS_SRC_DIR)
        set(OBS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obs-src")
    endif ()
    if (NOT DEFINED OBS_BIN_DIR)
        set(OBS_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obs-bin")
    endif ()

    if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        if (${PORTABLE_MODE})
            set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/obs-bin" CACHE PATH "" FORCE)
        else ()
            set(CMAKE_INSTALL_PREFIX "$ENV{ProgramData}/obs-studio" CACHE PATH "" FORCE)
        endif ()
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
    set(LIBOBS_DIR ${CMAKE_CURRENT_BINARY_DIR}) 
elseif (APPLE)
    # macOS
    find_library(_LIB_OBS "${LIB_OBS}.0.dylib" PATHS ${OBS_BIN_DIR} REQUIRED)
    set(LIB_OBS "${_LIB_OBS}")
elseif (UNIX)
    # Linux
    find_package(libobs REQUIRED)

    # locate obs
    execute_process(COMMAND "which" "obs" 
        OUTPUT_VARIABLE OBS_BIN_FILE 
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message("=== OBS executable located in " ${OBS_BIN_FILE})
    # locate libobs
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/tmpscript.sh "#!/bin/sh\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/tmpscript.sh "readelf -d \"${OBS_BIN_FILE}\" | grep RUNPATH | grep -Po \"(?<=\\[)[^]]+\"")
    execute_process(COMMAND "sh" ${CMAKE_CURRENT_BINARY_DIR}/tmpscript.sh
        OUTPUT_VARIABLE LIBOBS_DIR 
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message("=== OBS libraries located in " ${LIBOBS_DIR})

    # locate obs headers
    find_path(OBS_SRC_DIR NAMES obs.h obs-module.h obs-frontend-api.h HINTS /usr/include/obs REQUIRED)
    message("=== OBS headers located in " ${OBS_SRC_DIR})
else ()
    message(FATAL_ERROR "Unsupported OS!")
endif ()

# find library
find_library(LIB_OBS obs HINTS ${LIBOBS_DIR} REQUIRED)
find_library(LIB_OBSFE obs-frontend-api HINTS ${LIBOBS_DIR} REQUIRED)

# find headers
find_path(OBS_INC_DIR NAMES obs.h obs-module.h HINTS "${OBS_SRC_DIR}" "${OBS_SRC_DIR}/libobs" REQUIRED)
find_path(OBS_FEINC_DIR NAMES obs-frontend-api.h HINTS "${OBS_SRC_DIR}" "${OBS_SRC_DIR}/UI/obs-frontend-api" REQUIRED)

# build plugin
add_library(obs-multi-rtmp SHARED
    "src/obs-multi-rtmp.cpp"
    "src/obs-multi-rtmp.h"
    "src/edit-widget.cpp"
    "src/edit-widget.h"
    "src/push-widget.cpp"
    "src/push-widget.h"
    "src/pch.h"
)
target_precompile_headers(obs-multi-rtmp PRIVATE
    "src/pch.h"
)
if (WIN32)
    configure_file("src/version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    target_sources(obs-multi-rtmp PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    )
endif ()
target_include_directories(obs-multi-rtmp PRIVATE
    "${OBS_INC_DIR}"
    "${OBS_FEINC_DIR}"
)
target_link_directories(obs-multi-rtmp PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    ${OBS_BIN_DIR}
)
target_link_libraries(obs-multi-rtmp PRIVATE
    Qt6::Widgets
    ${LIB_OBS}
    ${LIB_OBSFE}
)


if (${PORTABLE_MODE})
    install(TARGETS obs-multi-rtmp RUNTIME
        DESTINATION "obs-plugins/${PLAT}"
    )
    install(DIRECTORY locale
        DESTINATION "data/obs-plugins/obs-multi-rtmp"
    )
else()
    if (WIN32)
        install(TARGETS obs-multi-rtmp RUNTIME
            DESTINATION "plugins/obs-multi-rtmp/bin/${PLAT}"
        )
        install(DIRECTORY locale
            DESTINATION "plugins/obs-multi-rtmp/data"
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

        # guess directories
        find_path(PLG_SODIR 
                "obs-outputs.so"
            HINTS
                "${LIBOBS_DIR}/obs-plugins"
            REQUIRED
        )
        find_path(PLG_DATADIR 
                "obs-outputs/locale/en-US.ini"
            HINTS
                "/usr/share/obs/obs-plugins"
            REQUIRED
        )

        message("Install plugin to ${PLG_SODIR} and ${PLG_DATADIR}")

        install(TARGETS obs-multi-rtmp LIBRARY
            DESTINATION ${PLG_SODIR}
        )
        install(DIRECTORY locale
            DESTINATION ${PLG_DATADIR}/obs-multi-rtmp
        )
    endif ()
endif()