cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(PlayableAmbience VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(PA_BUILD_PLUGIN "Build JUCE plugin/standalone targets" ON)
option(PA_BUILD_TESTS "Build core tests and tools" ON)
option(PA_SANITIZE "Build core tests with address/undefined sanitizers" OFF)

# ------------------------------------------------------------------------------------------------ core DSP
add_library(pa_core STATIC
    Source/Core/Params.cpp
    Source/Core/FFT.cpp
    Source/Core/Notes.cpp
    Source/Core/Engine.cpp
    Source/Core/Fixtures.cpp
    Source/Core/Sequences.cpp
    Source/Core/Analysis.cpp
    Source/DSP/Harmony/ClassicVocoder.cpp
    Source/DSP/Harmony/FftVocoder.cpp
    Source/DSP/Harmony/Resonator.cpp
    Source/DSP/Harmony/IntervalShift.cpp
    Source/DSP/Harmony/HarmonyUnit.cpp
    Source/DSP/Delay/Delays.cpp
    Source/DSP/Reverb/Reverbs.cpp
    Source/Presets/FactoryPresets.cpp)
target_include_directories(pa_core PUBLIC Source)
# Deliberately no -ffast-math: finite checks and denormal handling must stay valid.
if(MSVC)
    target_compile_options(pa_core PRIVATE /W3)
else()
    target_compile_options(pa_core PRIVATE -Wall -Wextra -Wno-unused-parameter $<$<CONFIG:Release>:-O3>)
endif()
if(PA_SANITIZE)
    target_compile_options(pa_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(pa_core PUBLIC -fsanitize=address,undefined)
endif()

if(PA_BUILD_TESTS)
    add_executable(pa_tests Tests/TestMain.cpp Tests/CoreTests.cpp Tests/DspTests.cpp Tests/GraphTests.cpp)
    target_link_libraries(pa_tests PRIVATE pa_core)
    add_executable(pa_render Tools/Render.cpp)
    target_link_libraries(pa_render PRIVATE pa_core)
    add_executable(pa_bench Tools/Bench.cpp)
    target_link_libraries(pa_bench PRIVATE pa_core)
    enable_testing()
    add_test(NAME core COMMAND pa_tests)
endif()

# ------------------------------------------------------------------------------------------------ JUCE
if(PA_BUILD_PLUGIN)
    include(cmake/JuceDependency.cmake)
    include(cmake/PluginTargets.cmake)
endif()
