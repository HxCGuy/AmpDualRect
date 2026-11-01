cmake_minimum_required(VERSION 3.22)
project(AmpDualRect VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# JUCE: use -DJUCE_DIR=<path to a JUCE checkout> to build offline, otherwise it is fetched.
if (DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(AmpDualRect
    COMPANY_NAME "Amp5150Sim"
    PLUGIN_MANUFACTURER_CODE Amsm
    PLUGIN_CODE ADr2
    FORMATS VST3 Standalone
    PRODUCT_NAME "AmpDualRect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(AmpDualRect PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginProcessor.h
    plugin/PluginEditor.cpp
    plugin/PluginEditor.h
    plugin/Parameters.h
    plugin/DefaultCab.h
    dsp/DualRect.h dsp/DKNet.h dsp/Tubes.h dsp/NoiseGate.h)

target_include_directories(AmpDualRect PRIVATE dsp plugin)

target_compile_definitions(AmpDualRect PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(AmpDualRect
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Speed: the amp model is double-precision math in small dense loops. AVX2 + FMA (present on every
# CPU Windows 11 supports) and link-time optimisation make it noticeably faster. Set
# -DAMPDUALRECT_AVX2=OFF to build for older x86 CPUs.
option(AMPDUALRECT_AVX2 "Build with AVX2/FMA (x86-64)" ON)
set(AMP_X86 OFF)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|X86_64")
    set(AMP_X86 ON)
endif()
if (MSVC)
    target_compile_options(AmpDualRect PRIVATE /O2 /fp:precise /utf-8)
    if (AMPDUALRECT_AVX2 AND AMP_X86)
        target_compile_options(AmpDualRect PRIVATE /arch:AVX2 /fp:contract)
    endif()
else()
    target_compile_options(AmpDualRect PRIVATE -O3)
    if (AMPDUALRECT_AVX2 AND AMP_X86)
        target_compile_options(AmpDualRect PRIVATE -mavx2 -mfma)
    endif()
endif()
target_link_libraries(AmpDualRect PRIVATE juce::juce_recommended_lto_flags)

# Optional: offline test host (headless render of the processor + editor snapshots) -- see tests/offline_host.cpp
option(AMPDUALRECT_BUILD_TESTHOST "Build the offline test host" OFF)
if (AMPDUALRECT_BUILD_TESTHOST)
    juce_add_console_app(AmpDualRectTestHost PRODUCT_NAME "AmpDualRectTestHost")
    target_sources(AmpDualRectTestHost PRIVATE tests/offline_host.cpp plugin/PluginProcessor.cpp plugin/PluginEditor.cpp)
    target_include_directories(AmpDualRectTestHost PRIVATE dsp plugin)
    target_compile_definitions(AmpDualRectTestHost PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(AmpDualRectTestHost PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    if (NOT MSVC)
        target_compile_options(AmpDualRectTestHost PRIVATE -O3)
        if (AMPDUALRECT_AVX2 AND AMP_X86)
            target_compile_options(AmpDualRectTestHost PRIVATE -mavx2 -mfma)
        endif()
    endif()
endif()
