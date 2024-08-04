cmake_minimum_required(VERSION 3.22.1)

project(obs-streamlink C CXX)
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

find_package(Python 3.8...<3.12 REQUIRED COMPONENTS Development)
find_package(FFmpeg REQUIRED COMPONENTS swscale)


# TODO: obsproject/obs-websocket/cmake/FindLibObs.cmake


set(OBS_STUDIO_SOURCE_PATH "" CACHE PATH "the path to the source code of OBS Studio")
if (NOT EXISTS "${OBS_STUDIO_SOURCE_PATH}/CMakeLists.txt" OR NOT EXISTS "${OBS_STUDIO_SOURCE_PATH}/libobs")
    message(FATAL_ERROR "Please specify valid OBS_STUDIO_SOURCE_PATH")
endif ()

# TODO: SIMD stuff?
add_subdirectory("${OBS_STUDIO_SOURCE_PATH}/deps/media-playback" deps/obs-studio-media-playback EXCLUDE_FROM_ALL)
target_link_libraries(media-playback INTERFACE ${FFMPEG_LIBRARIES})

if (WIN32)
    add_library(w32-pthreads SHARED IMPORTED)
endif ()
add_library(libobs SHARED IMPORTED)

set(SRC_FILES
        obs-streamlink.cpp
        python-streamlink.cpp
        streamlink-source.cpp)

if (APPLE)
    add_library(${CMAKE_PROJECT_NAME} MODULE ${SRC_FILES})
else ()
    add_library(${CMAKE_PROJECT_NAME} SHARED ${SRC_FILES})
endif ()

if (WIN32)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE streamlink-delayload.cpp)
endif ()
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "deps/")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE media-playback Python::Module Python::Python #[[ TODO ]] libobs)
if (WIN32)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE w32-pthreads)
endif ()
# https://stackoverflow.com/questions/47690822/possible-to-force-cmake-msvc-to-use-utf-8-encoding-for-source-files-without-a-bo
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

if (WIN32)
    get_filename_component(python-stable-abi-lib-filename ${Python_RUNTIME_LIBRARY} NAME)
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "/DELAYLOAD:${python-stable-abi-lib-filename}")
endif ()

if (WIN32)
    install(TARGETS ${CMAKE_PROJECT_NAME}
            LIBRARY DESTINATION "obs-plugins/64bit"
            RUNTIME DESTINATION "obs-plugins/64bit")
    install(FILES "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
            CONFIGURATIONS Debug Release RelWithDebInfo
            DESTINATION "obs-plugins/64bit"
            OPTIONAL)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/
            DESTINATION "data/obs-plugins/obs-streamlink")
elseif (APPLE)
    # TODO
else ()
    # https://github.com/obsproject/obs-plugintemplate/blob/e3688b7491c52ef6e37ac59daa93e7cf4d9e2b28/cmake/linux/defaults.cmake#L10
    include(GNUInstallDirs)
    # https://github.com/obsproject/obs-plugintemplate/blob/e3688b7491c52ef6e37ac59daa93e7cf4d9e2b28/cmake/linux/helpers.cmake#L27-L30
    install(TARGETS ${CMAKE_PROJECT_NAME}
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    # https://github.com/obsproject/obs-plugintemplate/blob/e3688b7491c52ef6e37ac59daa93e7cf4d9e2b28/cmake/linux/helpers.cmake#L60-L63
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/obs-streamlink")
    # to be able to find libobs.so
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
            INSTALL_RPATH "$ORIGIN/..")
endif()

set(OBS_STUDIO_BUILD_PATH "${CMAKE_BINARY_DIR}/obs-studio-build")

# https://discourse.cmake.org/t/depending-on-package-config-file-as-external-project/1501
list(JOIN CMAKE_PREFIX_PATH "|" CMAKE_PREFIX_PATH_ALT_SEP)
include(ExternalProject)
ExternalProject_Add(obs-studio-source
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
            -DENABLE_UI=OFF
        # https://stackoverflow.com/questions/76362485/how-can-i-get-the-variable-of-build-configuration-of-multi-config-generator-in-c
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG> --target libobs
        BUILD_BYPRODUCTS
            # Ninja only: https://stackoverflow.com/questions/40314785/linking-against-an-externalproject-add-dependency-in-cmake
            "${OBS_STUDIO_BUILD_PATH}/libobs/libobs.so"
        INSTALL_COMMAND "")

if (WIN32)
    add_dependencies(w32-pthreads obs-studio-source)
    target_compile_definitions(w32-pthreads INTERFACE "-DHAVE_STRUCT_TIMESPEC=1")
    set_target_properties(w32-pthreads PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OBS_STUDIO_SOURCE_PATH}/deps/w32-pthreads")
    if (NOT CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UPPER)
        set_target_properties(w32-pthreads PROPERTIES
                IMPORTED_IMPLIB_${CMAKE_BUILD_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/${CMAKE_BUILD_TYPE}/w32-pthreads.lib"
                IMPORTED_LOCATION_${CMAKE_BUILD_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/${CMAKE_BUILD_TYPE}/w32-pthreads.dll")
    else ()
        foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
            set_target_properties(w32-pthreads PROPERTIES
                    IMPORTED_IMPLIB_${CONFIG_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/${CONFIG_TYPE}/w32-pthreads.lib"
                    IMPORTED_LOCATION_${CONFIG_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/deps/w32-pthreads/${CONFIG_TYPE}/w32-pthreads.dll")
        endforeach ()
    endif ()
endif ()

add_dependencies(libobs obs-studio-source)
set_target_properties(libobs PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OBS_STUDIO_SOURCE_PATH}/libobs")
if (WIN32)
    if (NOT CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UPPER)
        set_target_properties(libobs PROPERTIES
                IMPORTED_IMPLIB_${CMAKE_BUILD_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/libobs/${CMAKE_BUILD_TYPE}/obs.lib"
                IMPORTED_LOCATION_${CMAKE_BUILD_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/libobs/${CMAKE_BUILD_TYPE}/obs.dll")
    else ()
        foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
            set_target_properties(libobs PROPERTIES
                    IMPORTED_IMPLIB_${CONFIG_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/libobs/${CONFIG_TYPE}/obs.lib"
                    IMPORTED_LOCATION_${CONFIG_TYPE_UPPER} "${OBS_STUDIO_BUILD_PATH}/libobs/${CONFIG_TYPE}/obs.dll")
        endforeach ()
    endif ()
elseif (APPLE)
    set_target_properties(libobs PROPERTIES
            IMPORTED_LOCATION "${OBS_STUDIO_BUILD_PATH}/libobs/libobs.framework")
else ()
    set_target_properties(libobs PROPERTIES
            IMPORTED_LOCATION "${OBS_STUDIO_BUILD_PATH}/libobs/libobs.so")
endif ()
