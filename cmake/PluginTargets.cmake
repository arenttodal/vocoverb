# Playable Ambience plugin / standalone targets.
set(PA_VERSION ${PROJECT_VERSION})
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE PA_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT PA_GIT_HASH)
    set(PA_GIT_HASH "unknown")
endif()
configure_file(${CMAKE_SOURCE_DIR}/Source/Plugin/BuildInfo.h.in ${CMAKE_BINARY_DIR}/generated/BuildInfo.h @ONLY)

set(PA_PLUGIN_SOURCES
    Source/Plugin/PluginProcessor.cpp
    Source/Presets/PresetManager.cpp
    Source/Standalone/SourcePlayer.cpp
    Source/Standalone/AuditionExporter.cpp
    Source/Standalone/StandaloneApp.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/Graphs.cpp
    Source/UI/Keyboard.cpp
    Source/UI/AdvancedPanel.cpp
    Source/UI/StandalonePanel.cpp
    Source/UI/PluginEditor.cpp)

juce_add_binary_data(pa_binary_data HEADER_NAME BinaryData.h NAMESPACE BinaryData
    SOURCES ${CMAKE_SOURCE_DIR}/Resources/Fonts/Inter-Regular.ttf ${CMAKE_SOURCE_DIR}/Resources/Fonts/Inter-Medium.ttf ${CMAKE_SOURCE_DIR}/Resources/Fonts/Inter-SemiBold.ttf ${CMAKE_SOURCE_DIR}/Resources/Fonts/Inter-Bold.ttf)
set_target_properties(pa_binary_data PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(PA_COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

set(PA_MIC_TEXT "Playable Ambience uses audio input only when you enable Live Input, to process your voice or instrument.")

function(pa_configure_target tgt)
    target_sources(${tgt} PRIVATE ${PA_PLUGIN_SOURCES})
    target_include_directories(${tgt} PRIVATE ${CMAKE_SOURCE_DIR}/Source ${CMAKE_BINARY_DIR}/generated)
    target_compile_definitions(${tgt} PUBLIC ${PA_COMMON_DEFS})
    target_link_libraries(${tgt}
        PRIVATE pa_core pa_binary_data juce::juce_audio_utils juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    juce_generate_juce_header(${tgt})
endfunction()

set(PA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PA_FORMATS AU)
endif()

juce_add_plugin(PlayableAmbience
    PRODUCT_NAME "Playable Ambience"
    COMPANY_NAME "ARN"
    COMPANY_WEBSITE ""
    BUNDLE_ID com.arn.playableambience
    VERSION ${PA_VERSION}
    PLUGIN_MANUFACTURER_CODE Arnv
    PLUGIN_CODE PaAm
    FORMATS ${PA_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    AU_SANDBOX_SAFE FALSE
    VST3_CATEGORIES Fx Delay Reverb
    COPY_PLUGIN_AFTER_BUILD FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "${PA_MIC_TEXT}"
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS com.apple.security.device.audio-input)
pa_configure_target(PlayableAmbience)
target_compile_definitions(PlayableAmbience PUBLIC PA_COMPANION=0)

if(APPLE)
    # Ordinary AU audio effect companion: manual/stored harmony without a MIDI instrument track.
    juce_add_plugin(PlayableAmbienceAudio
        PRODUCT_NAME "Playable Ambience Audio"
        COMPANY_NAME "ARN"
        BUNDLE_ID com.arn.playableambience.audioau
        VERSION ${PA_VERSION}
        PLUGIN_MANUFACTURER_CODE Arnv
        PLUGIN_CODE PaAa
        FORMATS AU
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        HARDENED_RUNTIME_ENABLED TRUE)
    pa_configure_target(PlayableAmbienceAudio)
    target_compile_definitions(PlayableAmbienceAudio PUBLIC PA_COMPANION=1)
endif()

# Host-side MIDI delivery check (loads the built VST3/AU like a DAW and verifies MIDI changes the output).
juce_add_console_app(pa_hostcheck PRODUCT_NAME "pa_hostcheck")
target_sources(pa_hostcheck PRIVATE Tools/HostMidiCheck.cpp)
target_compile_definitions(pa_hostcheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_LADSPA=0 JUCE_PLUGINHOST_LV2=0
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(pa_hostcheck PRIVATE juce::juce_audio_processors juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
juce_generate_juce_header(pa_hostcheck)
