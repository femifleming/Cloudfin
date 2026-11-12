cmake_minimum_required(VERSION 3.22)

# macOS: one universal build for Apple silicon and Intel
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

project(Leitner VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LEITNER_BUILD_TESTS "Build the offline tests" OFF)
option(LEITNER_COPY_AFTER_BUILD "Install the plug-ins into your user plug-in folders after building" ON)

# Leitner lives in Cloudfin's repository: it is Cloudfin's processor with a spatial stage, and shares its JUCE
set(CLOUDFIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
if(NOT EXISTS "${CLOUDFIN_DIR}/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE is missing: run  git submodule update --init  in the Cloudfin folder")
endif()
add_subdirectory("${CLOUDFIN_DIR}/JUCE" "${CMAKE_BINARY_DIR}/JUCE")

if(APPLE)
    set(LT_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(LT_FORMATS VST3 Standalone)
else()
    set(LT_FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(Leitner
    VERSION 0.1.0
    COMPANY_NAME "FreakiePie"
    COMPANY_WEBSITE "https://github.com/femifleming/Cloudfin/tree/main/Leitner"
    BUNDLE_ID "com.freakiepie.leitner"
    PLUGIN_MANUFACTURER_CODE Frkp
    PLUGIN_CODE Ltnr
    FORMATS ${LT_FORMATS}
    PRODUCT_NAME "Leitner"
    DESCRIPTION "Cloudfin as a sound space: an Ambisonics encoder, 3rd to 7th order"
    ICON_BIG "${CLOUDFIN_DIR}/Resources/icon/icon_1024.png"
    LV2URI "https://github.com/femifleming/Cloudfin/Leitner"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Spatial
    COPY_PLUGIN_AFTER_BUILD ${LEITNER_COPY_AFTER_BUILD})

set(LT_SOURCES "${CLOUDFIN_DIR}/Source/PluginProcessor.cpp" Source/LeitnerProcessor.cpp Source/LeitnerEditor.cpp)
set(LT_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 CLOUDFIN_AS_BASE=1)
set(LT_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${CLOUDFIN_DIR}/Source/dsp")

target_sources(Leitner PRIVATE ${LT_SOURCES})
target_compile_definitions(Leitner PUBLIC ${LT_DEFS})
target_include_directories(Leitner PRIVATE ${LT_INCLUDES})
juce_add_binary_data(LeitnerAssets SOURCES "${CLOUDFIN_DIR}/Resources/hydrophone.ogg")
target_link_libraries(Leitner
    PRIVATE
        LeitnerAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# tests (not part of the plug-in): cmake -B build -DLEITNER_BUILD_TESTS=ON
#==============================================================================
if(LEITNER_BUILD_TESTS)
    juce_add_console_app(LeitnerTest PRODUCT_NAME "LeitnerTest")
    target_sources(LeitnerTest PRIVATE tests/OfflineTest.cpp ${LT_SOURCES})
    target_compile_definitions(LeitnerTest PRIVATE ${LT_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1)
    target_include_directories(LeitnerTest PRIVATE ${LT_INCLUDES})
    target_link_libraries(LeitnerTest PRIVATE LeitnerAssets juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)
endif()
