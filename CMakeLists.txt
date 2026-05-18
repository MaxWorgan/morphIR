cmake_minimum_required(VERSION 3.21)
project(MorphIR VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.7
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE clap-juce-extensions)

juce_add_plugin(MorphIR
    COMPANY_NAME                "MorphIR"
    PLUGIN_MANUFACTURER_CODE    Mrph
    PLUGIN_CODE                 MrIR
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "MorphIR"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

target_sources(MorphIR PRIVATE
    src/MorphIRProcessor.cpp
    src/MorphIREditor.cpp
)

target_include_directories(MorphIR PRIVATE src)

target_compile_definitions(MorphIR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MorphIR PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

clap_juce_extensions_plugin(
    TARGET MorphIR
    CLAP_ID "com.morphir.morphir"
    CLAP_FEATURES audio-effect reverb
)
