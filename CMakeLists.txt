cmake_minimum_required(VERSION 3.21)
project(DeclarativeSoundEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DECLSOUND_BUILD_CLI "Build the AudioTestCLI example application" ON)

include(FetchContent)

if(EXISTS "${CMAKE_SOURCE_DIR}/../miniaudio/miniaudio.h")
    message(STATUS "Using local miniaudio")
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE "${CMAKE_SOURCE_DIR}/../miniaudio")
else()
    message(FATAL_ERROR "miniaudio.h not found. Please place it in ../miniaudio")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/../yaml-cpp-master/CMakeLists.txt")
    message(STATUS "Using local yaml-cpp")
    add_subdirectory("${CMAKE_SOURCE_DIR}/../yaml-cpp-master" yaml-cpp)
else()
    message(STATUS "Fetching yaml-cpp from GitHub")
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif()

set(DECLSOUND_PUBLIC_HEADERS
    include/DeclarativeSoundEngine/AudioConfig.hpp
    include/DeclarativeSoundEngine/AudioManagerAPI.hpp
    include/DeclarativeSoundEngine/Log.hpp
    include/DeclarativeSoundEngine/declsound_export.hpp
)

set(DECLSOUND_ENGINE_SOURCES
    src/core/ActiveBehavior.cpp
    src/core/AudioBuffer.cpp
    src/core/AudioBufferManager.cpp
    src/core/AudioManager.cpp
    src/core/AudioManagerAPI.cpp
    src/core/BehaviorLoader.cpp
    src/core/Condition.cpp
    src/core/Entity.cpp
    src/core/Expression.cpp
    src/core/LeafBuilder.cpp
    src/core/Log.cpp
    src/core/MatchUtils.cpp
    src/core/Node.cpp
    src/core/ParserUtils.cpp
    src/core/TagMap.cpp
    src/core/ValueMap.cpp
    src/backends/AudioDeviceMiniAudio.cpp
    src/backends/AudioDeviceStub.cpp
    src/backends/AudioDeviceUnity.cpp
    src/platform/win32/pch.cpp
)

add_library(DeclarativeSoundEngine SHARED
    ${DECLSOUND_PUBLIC_HEADERS}
    ${DECLSOUND_ENGINE_SOURCES}
)
add_library(DeclarativeSoundEngine::DeclarativeSoundEngine ALIAS DeclarativeSoundEngine)

if(WIN32)
    target_sources(DeclarativeSoundEngine PRIVATE src/platform/win32/dllmain.cpp)
endif()

target_include_directories(DeclarativeSoundEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/DeclarativeSoundEngine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/src/support
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/win32
)

target_compile_definitions(DeclarativeSoundEngine PRIVATE DECLSOUND_BUILD_DLL)
target_link_libraries(DeclarativeSoundEngine PRIVATE yaml-cpp miniaudio)

if(MSVC)
    target_compile_options(DeclarativeSoundEngine PRIVATE /W4 /permissive-)
else()
    target_compile_options(DeclarativeSoundEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${DECLSOUND_PUBLIC_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES ${DECLSOUND_ENGINE_SOURCES})

if(DECLSOUND_BUILD_CLI)
    add_executable(AudioTestCLI apps/AudioTestCLI/main.cpp)
    target_link_libraries(AudioTestCLI PRIVATE DeclarativeSoundEngine::DeclarativeSoundEngine)

    if(MSVC)
        target_compile_options(AudioTestCLI PRIVATE /W4 /permissive-)
    else()
        target_compile_options(AudioTestCLI PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
