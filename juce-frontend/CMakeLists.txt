cmake_minimum_required(VERSION 3.22)
project(MambaCompressor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to JUCE source tree")
add_subdirectory(${JUCE_PATH} JUCE_build EXCLUDE_FROM_ALL)

set(ARA_SDK_PATH "$ENV{HOME}/ARA_SDK" CACHE PATH "Path to ARA SDK 2.2.0")
juce_set_ara_sdk_path(${ARA_SDK_PATH})

juce_add_plugin(MambaCompressor
    VERSION 1.0.0
    COMPANY_NAME "SGMStudios"
    BUNDLE_ID "com.sgmstudios.mambacompressor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    IS_ARA_EFFECT TRUE
    ARA_ANALYSIS_TYPES kARAContentTypeNotes kARAContentTypeSheetChords kARAContentTypeKeySignatures
    ARA_TRANSFORMATION_FLAGS kARAPlaybackTransformationTimestretch kARAPlaybackTransformationTimestretchReflectingTempo
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE SGMS
    PLUGIN_CODE Mcmp
    FORMATS VST3 Standalone
    VST3_CATEGORIES "Fx" "Dynamics"
    PRODUCT_NAME "Mamba Compressor"
)

target_sources(MambaCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MambaARADocumentController.h
)

target_include_directories(MambaCompressor PRIVATE Source)

target_compile_definitions(MambaCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_ARA=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_compile_options(MambaCompressor PRIVATE
    -O2
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_libraries(MambaCompressor
    PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors_headless
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
