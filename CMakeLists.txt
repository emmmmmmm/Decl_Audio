cmake_minimum_required(VERSION 3.15)
project(DeclarativeSoundEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# === Miniaudio (header-only, local-only) ===
if(EXISTS "${CMAKE_SOURCE_DIR}/../miniaudio/miniaudio.h")
    message(STATUS "Using local miniaudio")
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE "${CMAKE_SOURCE_DIR}/../miniaudio")
else()
    message(FATAL_ERROR "miniaudio.h not found. Please place it in ../miniaudio")
endif()

# === YAML-CPP (full repo) ===
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

# Engine library
add_library(DeclarativeSoundEngine SHARED
    DeclarativeSoundEngine/ActiveBehavior.cpp
    DeclarativeSoundEngine/AudioBuffer.cpp
    DeclarativeSoundEngine/AudioBufferManager.cpp
    DeclarativeSoundEngine/AudioDeviceMiniAudio.cpp
    DeclarativeSoundEngine/AudioDeviceStub.cpp
    DeclarativeSoundEngine/AudioDeviceUnity.cpp
    DeclarativeSoundEngine/AudioManager.cpp
    DeclarativeSoundEngine/AudioManagerAPI.cpp
    DeclarativeSoundEngine/BehaviorLoader.cpp
    DeclarativeSoundEngine/Condition.cpp
    DeclarativeSoundEngine/Entity.cpp
    DeclarativeSoundEngine/Expression.cpp
    DeclarativeSoundEngine/LeafBuilder.cpp
    DeclarativeSoundEngine/Log.cpp
    DeclarativeSoundEngine/MatchUtils.cpp
    DeclarativeSoundEngine/Node.cpp
    DeclarativeSoundEngine/ParserUtils.cpp
    DeclarativeSoundEngine/TagMap.cpp
    DeclarativeSoundEngine/ValueMap.cpp
    DeclarativeSoundEngine/pch.cpp
)

target_include_directories(DeclarativeSoundEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/DeclarativeSoundEngine
)

if(WIN32)
    target_sources(DeclarativeSoundEngine PRIVATE DeclarativeSoundEngine/dllmain.cpp)
endif()

target_compile_definitions(DeclarativeSoundEngine PRIVATE DECLSOUND_BUILD_DLL)
target_link_libraries(DeclarativeSoundEngine PRIVATE yaml-cpp miniaudio)

# CLI tool executable
add_executable(AudioTestCLI
    AudioTestCLI/AudioTestCLI.cpp
)

target_include_directories(AudioTestCLI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/DeclarativeSoundEngine
)

target_link_libraries(AudioTestCLI DeclarativeSoundEngine)
