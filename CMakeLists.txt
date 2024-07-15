cmake_minimum_required(VERSION 3.22.1)

project(obs-streamlink C CXX)
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

find_package(Python 3.8...3.11 REQUIRED COMPONENTS Development)
find_package(FFmpeg REQUIRED COMPONENTS swscale)


set(OBS_STUDIO_SOURCE_PATH "" CACHE PATH "the path to the source code of OBS Studio")
if (NOT EXISTS "${OBS_STUDIO_SOURCE_PATH}/CMakeLists.txt" OR NOT EXISTS "${OBS_STUDIO_SOURCE_PATH}/libobs")
    message(FATAL_ERROR "Please specify valid OBS_STUDIO_SOURCE_PATH")
endif ()

add_subdirectory("${OBS_STUDIO_SOURCE_PATH}/deps/media-playback" deps/obs-studio-media-playback EXCLUDE_FROM_ALL)
target_link_libraries(media-playback INTERFACE ${FFMPEG_LIBRARIES})

if (MSVC)
    add_library(w32-pthreads SHARED IMPORTED)
endif ()
add_library(libobs SHARED IMPORTED)
add_library(libobs-frontend-api SHARED IMPORTED)

add_library(${CMAKE_PROJECT_NAME} SHARED
        obs-streamlink.cpp
        python-streamlink.cpp
        streamlink-source.cpp
)
if (MSVC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE streamlink-delayload.cpp)
endif ()
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        "deps/"
)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        Python::Module media-playback
        libobs libobs-frontend-api
)
if (MSVC)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
            w32-pthreads
    )
endif ()
target_compile_options(${CMAKE_PROJECT_NAME}
        PRIVATE "$<$<C_COMPILER_ID:MSVC>:/utf-8>"
        PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

if (MSVC)
    get_filename_component(python-stable-abi-lib-filename ${Python_RUNTIME_LIBRARY} NAME)
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
            "/DELAYLOAD:${python-stable-abi-lib-filename}"
    )
endif ()

if (WIN32)
    install(
            TARGETS ${CMAKE_PROJECT_NAME}
            LIBRARY DESTINATION "obs-plugins/64bit"
            RUNTIME DESTINATION "obs-plugins/64bit"
    )
    install(
            FILES "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
            CONFIGURATIONS Debug Release RelWithDebInfo
            DESTINATION "obs-plugins/64bit"
            OPTIONAL
    )
    install(
            DIRECTORY ${CMAKE_SOURCE_DIR}/data/
            DESTINATION "data/obs-plugins/obs-streamlink"
    )
elseif (APPLE)
    # TODO
else ()
    include(GNUInstallDirs)
    # https://github.com/obsproject/obs-plugintemplate/blob/e3688b7491c52ef6e37ac59daa93e7cf4d9e2b28/cmake/linux/helpers.cmake
    install(
            TARGETS ${CMAKE_PROJECT_NAME}
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins"
    )
    install(
            DIRECTORY ${CMAKE_SOURCE_DIR}/data/
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/obs-streamlink"
    )
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
            INSTALL_RPATH "$ORIGIN/.."
    )
endif()

set(OBS_STUDIO_BUILD_PATH "${CMAKE_BINARY_DIR}/obs-studio-build")

# https://discourse.cmake.org/t/depending-on-package-config-file-as-external-project/1501
list(JOIN CMAKE_PREFIX_PATH "|" CMAKE_PREFIX_PATH_ALT_SEP)
include(ExternalProject)
ExternalProject_Add(
        obs-studio-source
        SOURCE_DIR "${OBS_STUDIO_SOURCE_PATH}"
        BINARY_DIR "${OBS_STUDIO_BUILD_PATH}"
        INSTALL_DIR ""
        LIST_SEPARATOR "|"
        CMAKE_ARGS
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH_ALT_SEP}"
            -DENABLE_BROWSER=OFF
            -DENABLE_AJA=0
            -DENABLE_WEBRTC=0
            -DQT_VERSION=6
            -DENABLE_SCRIPTING=OFF
            -DENABLE_PLUGINS=OFF
            -DENABLE_UPDATER=OFF
            -DENABLE_DECKLINK=OFF
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG> --target libobs obs-frontend-api
        BUILD_BYPRODUCTS
            # Ninja only: https://stackoverflow.com/questions/40314785/linking-against-an-externalproject-add-dependency-in-cmake
            "${OBS_STUDIO_BUILD_PATH}/rundir/$<CONFIG>/lib/libobs.so"
            "${OBS_STUDIO_BUILD_PATH}/rundir/$<CONFIG>/lib/libobs-frontend-api.so"
        INSTALL_COMMAND ""
)

if (MSVC)
    target_compile_definitions(w32-pthreads INTERFACE "-DHAVE_STRUCT_TIMESPEC=1")
    set_target_properties(w32-pthreads PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OBS_STUDIO_SOURCE_PATH}/deps/w32-pthreads"
            IMPORTED_IMPLIB_DEBUG "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/Debug/w32-pthreads.lib"
            IMPORTED_IMPLIB_RELEASE "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/Release/w32-pthreads.lib"
            IMPORTED_IMPLIB_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/MinSizeRel/w32-pthreads.lib"
            IMPORTED_IMPLIB_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.lib"
            IMPORTED_LOCATION_DEBUG "${OBS_STUDIO_BUILD_PATH}/rundir/Debug/bin/64bit/w32-pthreads.dll"
            IMPORTED_LOCATION_RELEASE "${OBS_STUDIO_BUILD_PATH}/rundir/Release/bin/64bit/w32-pthreads.dll"
            IMPORTED_LOCATION_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/rundir/MinSizeRel/bin/64bit/w32-pthreads.dll"
            IMPORTED_LOCATION_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/rundir/RelWithDebInfo/bin/64bit/w32-pthreads.dll"
    )
    add_dependencies(w32-pthreads obs-studio-source)
endif ()

set_target_properties(libobs PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OBS_STUDIO_SOURCE_PATH}/libobs"
)
if (MSVC)
    set_target_properties(libobs PROPERTIES
            IMPORTED_IMPLIB_DEBUG "${OBS_STUDIO_BUILD_PATH}/libobs/Debug/obs.lib"
            IMPORTED_IMPLIB_RELEASE "${OBS_STUDIO_BUILD_PATH}/libobs/Release/obs.lib"
            IMPORTED_IMPLIB_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/libobs/MinSizeRel/obs.lib"
            IMPORTED_IMPLIB_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/libobs/RelWithDebInfo/obs.lib"
            IMPORTED_LOCATION_DEBUG "${OBS_STUDIO_BUILD_PATH}/rundir/Debug/bin/64bit/obs.dll"
            IMPORTED_LOCATION_RELEASE "${OBS_STUDIO_BUILD_PATH}/rundir/Release/bin/64bit/obs.dll"
            IMPORTED_LOCATION_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/rundir/MinSizeRel/bin/64bit/obs.dll"
            IMPORTED_LOCATION_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/rundir/RelWithDebInfo/bin/64bit/obs.dll"
    )
else ()
    set_target_properties(libobs PROPERTIES
            IMPORTED_LOCATION_DEBUG "${OBS_STUDIO_BUILD_PATH}/rundir/Debug/lib/libobs.so"
            IMPORTED_LOCATION_RELEASE "${OBS_STUDIO_BUILD_PATH}/rundir/Release/lib/libobs.so"
            IMPORTED_LOCATION_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/rundir/MinSizeRel/lib/libobs.so"
            IMPORTED_LOCATION_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/rundir/RelWithDebInfo/lib/libobs.so"
    )
endif ()
add_dependencies(libobs obs-studio-source)

set_target_properties(libobs-frontend-api PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OBS_STUDIO_SOURCE_PATH}/UI/obs-frontend-api"
)
if (MSVC)
    set_target_properties(libobs-frontend-api PROPERTIES
            IMPORTED_IMPLIB_DEBUG "${OBS_STUDIO_BUILD_PATH}/UI/obs-frontend-api/Debug/obs-frontend-api.lib"
            IMPORTED_IMPLIB_RELEASE "${OBS_STUDIO_BUILD_PATH}/UI/obs-frontend-api/Release/obs-frontend-api.lib"
            IMPORTED_IMPLIB_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/UI/obs-frontend-api/MinSizeRel/obs-frontend-api.lib"
            IMPORTED_IMPLIB_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.lib"
            IMPORTED_LOCATION_DEBUG "${OBS_STUDIO_BUILD_PATH}/rundir/Debug/bin/64bit/obs-frontend-api.dll"
            IMPORTED_LOCATION_RELEASE "${OBS_STUDIO_BUILD_PATH}/rundir/Release/bin/64bit/obs-frontend-api.dll"
            IMPORTED_LOCATION_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/rundir/MinSizeRel/bin/64bit/obs-frontend-api.dll"
            IMPORTED_LOCATION_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/rundir/RelWithDebInfo/bin/64bit/obs-frontend-api.dll"
    )
else ()
    set_target_properties(libobs-frontend-api PROPERTIES
            IMPORTED_LOCATION_DEBUG "${OBS_STUDIO_BUILD_PATH}/rundir/Debug/lib/libobs-frontend-api.so"
            IMPORTED_LOCATION_RELEASE "${OBS_STUDIO_BUILD_PATH}/rundir/Release/lib/libobs-frontend-api.so"
            IMPORTED_LOCATION_MINSIZEREL "${OBS_STUDIO_BUILD_PATH}/rundir/MinSizeRel/lib/libobs-frontend-api.so"
            IMPORTED_LOCATION_RELWITHDEBINFO "${OBS_STUDIO_BUILD_PATH}/rundir/RelWithDebInfo/lib/libobs-frontend-api.so"
    )
endif ()
add_dependencies(libobs-frontend-api obs-studio-source)
