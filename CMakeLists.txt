cmake_minimum_required(VERSION 3.22)
project(SpaceVocal VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS")

# JUCE 8 (licença AGPLv3 / comercial). Requer internet APENAS para este download inicial.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SpaceVocal
    COMPANY_NAME "SpaceVocal"
    PLUGIN_MANUFACTURER_CODE SpVc
    PLUGIN_CODE SpVo
    FORMATS VST3
    PRODUCT_NAME "SPACE VOCAL"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx)

set(SV_CORE_SOURCES
    Source/PluginProcessor.cpp
    Source/PresetManager.cpp)

target_sources(SpaceVocal PRIVATE ${SV_CORE_SOURCES} Source/PluginEditor.cpp)
target_include_directories(SpaceVocal PRIVATE Source)
target_compile_definitions(SpaceVocal PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(SpaceVocal PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Testes (executável de consola, corre o DSP e o estado reais)
option(SV_BUILD_TESTS "Construir testes" ON)
if(SV_BUILD_TESTS)
    juce_add_console_app(SpaceVocalTests PRODUCT_NAME "SpaceVocalTests")
    # Testes sem editor gráfico (SV_NO_EDITOR): só DSP, parâmetros, presets e estado reais
    target_sources(SpaceVocalTests PRIVATE Tests/Tests.cpp ${SV_CORE_SOURCES})
    target_include_directories(SpaceVocalTests PRIVATE Source)
    target_compile_definitions(SpaceVocalTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 SV_NO_EDITOR=1)
    target_link_libraries(SpaceVocalTests PRIVATE
        juce::juce_audio_processors
        juce::juce_recommended_config_flags)
endif()
