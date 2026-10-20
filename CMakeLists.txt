cmake_minimum_required(VERSION 3.22)
project(SH201Editor VERSION 1.2.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
)
FetchContent_MakeAvailable(JUCE)

# Setup libusb from third_party
set(LIBUSB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libusb/include")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LIBUSB_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libusb/VS2019/MS64/static/libusb-1.0.lib")
else()
    set(LIBUSB_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libusb/VS2019/MS32/static/libusb-1.0.lib")
endif()

juce_add_plugin(SH201Editor
    VERSION "1.2.1"
    COMPANY_NAME "Transfict8 Audio Tools"
    BUNDLE_ID "com.Transfict8AudioTools.SH201Editor"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Rold
    PLUGIN_CODE S201
    FORMATS VST3 Standalone
    PRODUCT_NAME "Roland SH-201 Editor"
)

juce_add_binary_data(SH201Assets
    HEADER_NAME "BinaryData.h"
    NAMESPACE BinaryData
    SOURCES
        "201 knobs/201 knob strip.png"
        "201 knobs/201slider.png"
)

target_sources(SH201Editor
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/Patches/SH201PatchSerializer.cpp
        src/Patches/SH201PatchSerializer.h
        src/Patches/PatchBrowserComponent.cpp
        src/Patches/PatchBrowserComponent.h
        src/USB/SH201USBManager.cpp
        src/USB/SH201USBManager.h
        src/MIDI/SH201DinMidiManager.cpp
        src/MIDI/SH201DinMidiManager.h
)

target_include_directories(SH201Editor
    PRIVATE
        src
        ${LIBUSB_INCLUDE_DIR}
)

target_compile_definitions(SH201Editor
    PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SH201Editor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        ${LIBUSB_LIBRARY}
        setupapi
        advapi32
        user32
        SH201Assets
)
