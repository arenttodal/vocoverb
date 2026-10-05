# Pinned JUCE dependency (see DEPENDENCIES.lock). A local checkout can be supplied with -DPA_JUCE_DIR=...
set(PA_JUCE_TAG "8.0.15")
set(PA_JUCE_COMMIT "91ad83ae34a81e0833b1a2b0866f54846370ae53")

if(NOT PA_JUCE_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/.deps/JUCE/CMakeLists.txt")
    set(PA_JUCE_DIR "${CMAKE_SOURCE_DIR}/.deps/JUCE")
endif()

if(PA_JUCE_DIR)
    message(STATUS "Using local JUCE at ${PA_JUCE_DIR}")
    add_subdirectory("${PA_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${PA_JUCE_COMMIT}
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
endif()
