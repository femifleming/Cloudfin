cmake_minimum_required(VERSION 3.22)

# macOS: one universal build for Apple silicon and Intel
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

project(Cloudfin VERSION 1.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CLOUDFIN_BUILD_TESTS "Build the offline and host tests" OFF)
option(CLOUDFIN_COPY_AFTER_BUILD "Install the plug-ins into your user plug-in folders after building" ON)

add_subdirectory(JUCE)

# AU + VST3 + app on macOS, VST3 + app on Windows, VST3 + LV2 + app on Linux
if(APPLE)
    set(CF_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(CF_FORMATS VST3 Standalone)
else()
    set(CF_FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(Cloudfin
    VERSION 1.1.0
    COMPANY_NAME "FreakiePie"
    COMPANY_WEBSITE "https://github.com/femifleming/Cloudfin"
    BUNDLE_ID "com.freakiepie.cloudfin"
    PLUGIN_MANUFACTURER_CODE Frkp
    PLUGIN_CODE Cldf
    FORMATS ${CF_FORMATS}
    PRODUCT_NAME "Cloudfin"
    DESCRIPTION "A dreamy, floating multi-effect in any order"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/icon_1024.png"
    LV2URI "https://github.com/femifleming/Cloudfin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD ${CLOUDFIN_COPY_AFTER_BUILD})

target_sources(Cloudfin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(CF_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_compile_definitions(Cloudfin PUBLIC ${CF_DEFS})

juce_add_binary_data(CloudfinAssets SOURCES Resources/hydrophone.ogg)

target_link_libraries(Cloudfin
    PRIVATE
        CloudfinAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# tests (not part of the plug-in): cmake -B build -DCLOUDFIN_BUILD_TESTS=ON
#==============================================================================
if(CLOUDFIN_BUILD_TESTS)
    juce_add_console_app(CloudfinTest PRODUCT_NAME "CloudfinTest")
    target_sources(CloudfinTest PRIVATE tests/OfflineTest.cpp)
    target_compile_definitions(CloudfinTest PRIVATE ${CF_DEFS})
    target_link_libraries(CloudfinTest PRIVATE juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)

    juce_add_gui_app(CloudfinHostTest PRODUCT_NAME "CloudfinHostTest")
    target_sources(CloudfinHostTest PRIVATE tests/HostTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(CloudfinHostTest PRIVATE ${CF_DEFS})
    target_link_libraries(CloudfinHostTest PRIVATE CloudfinAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
endif()
