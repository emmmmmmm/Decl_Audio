cmake_minimum_required(VERSION 3.21)

project(Decl_Audio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Engine sources shared by the library, the tests, and the apps.
set(DECL_AUDIO_ENGINE_SOURCES
    src/api/Decl_Audio.cpp
    src/assets/AssetBank.cpp
    src/backends/AudioDeviceBackend.cpp
    src/backends/MiniaudioBackend.cpp
    src/backends/StubBackend.cpp
    src/compiler/AuthoringParser.cpp
    src/compiler/Compiler.cpp
    src/compiler/CompilerIO.cpp
    src/core/BankSerializer.cpp
    src/core/Engine.cpp
    src/playback/AudioRuntime.cpp
    src/runtime/ControlRuntime.cpp
    src/third_party/MiniaudioImplementation.cpp
)

# The miniaudio implementation is third-party code; keep its warnings out of our logs.
if(NOT MSVC)
    set_source_files_properties(src/third_party/MiniaudioImplementation.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()

function(decl_audio_configure_target target)
    target_include_directories(${target} PUBLIC include)
    target_include_directories(${target} PRIVATE src/platform/win32)
    target_link_libraries(${target} PRIVATE Threads::Threads)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS} m)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# Static core: tests and apps compile against the engine directly (DECL_AUDIO_API expands to nothing).
add_library(decl_audio_core STATIC ${DECL_AUDIO_ENGINE_SOURCES})
decl_audio_configure_target(decl_audio_core)

# Shared library: the distributable engine with the exported C API.
add_library(decl_audio SHARED ${DECL_AUDIO_ENGINE_SOURCES})
decl_audio_configure_target(decl_audio)
target_compile_definitions(decl_audio PRIVATE DECL_AUDIO_BUILD_DLL)
# Base name "Decl_Audio" so .NET's LibraryImport("Decl_Audio") resolves libDecl_Audio.so / Decl_Audio.dll.
set_target_properties(decl_audio PROPERTIES
    OUTPUT_NAME Decl_Audio
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
)
if(WIN32)
    target_sources(decl_audio PRIVATE src/platform/win32/dllmain.cpp)
endif()

# Tests
add_executable(decl_audio_tests
    tests/AssetBankTests.cpp
    tests/BankSerializerTests.cpp
    tests/CompilerTests.cpp
    tests/HostLogTests.cpp
    tests/PlaybackTests.cpp
    tests/RingBufferTests.cpp
    tests/TestMain.cpp
    tests/WorldStateTests.cpp
)
target_link_libraries(decl_audio_tests PRIVATE decl_audio_core)
target_include_directories(decl_audio_tests PRIVATE src/platform/win32)

enable_testing()
add_test(NAME decl_audio_tests COMMAND decl_audio_tests)

# Apps
add_executable(decl_audio_sandbox_cli apps/SandboxCLI/SandboxMain.cpp)
target_link_libraries(decl_audio_sandbox_cli PRIVATE decl_audio_core)
target_include_directories(decl_audio_sandbox_cli PRIVATE src/platform/win32)

add_executable(decl_audio_validator apps/Validator/ValidatorMain.cpp)
target_link_libraries(decl_audio_validator PRIVATE decl_audio_core)
target_include_directories(decl_audio_validator PRIVATE src/platform/win32)
