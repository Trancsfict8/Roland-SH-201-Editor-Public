cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment target")
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

# Setup libusb
if(WIN32)
    set(LIBUSB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libusb/include")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LIBUSB_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libusb/VS2019/MS64/static/libusb-1.0.lib")
    else()
        set(LIBUSB_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libusb/VS2019/MS32/static/libusb-1.0.lib")
    endif()

elseif(APPLE)
    set(LIBUSB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb-10.13/include/libusb-1.0")
    set(LIBUSB_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/libusb-10.13/lib/libusb-1.0.0.dylib")
endif()

# Plugin formats
if(APPLE)
    set(PLUGIN_FORMATS AU VST3 Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
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
    FORMATS ${PLUGIN_FORMATS}
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
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(SH201Editor
        PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN
    )
endif()

target_link_libraries(SH201Editor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        SH201Assets
)

if(WIN32)
    target_link_libraries(SH201Editor
        PRIVATE
            ${LIBUSB_LIBRARY}
            setupapi
            advapi32
            user32
    )
elseif(APPLE)
    target_link_libraries(SH201Editor
        PRIVATE
            ${LIBUSB_LIBRARY}
    )
endif()

if(APPLE)
    # Bundle libusb inside each macOS app/plugin.
    foreach(PLUGIN_TARGET IN ITEMS
        SH201Editor_Standalone
        SH201Editor_AU
        SH201Editor_VST3
    )
        # Configure the runtime library search path.
	set_target_properties(${PLUGIN_TARGET} PROPERTIES
	    BUILD_WITH_INSTALL_RPATH TRUE
    	    INSTALL_RPATH "@loader_path/../Frameworks"
    	    INSTALL_RPATH_USE_LINK_PATH FALSE
	)

        # Copy and sign libusb after building.
        add_custom_command(TARGET ${PLUGIN_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${PLUGIN_TARGET}>/Contents/Frameworks"

            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBUSB_LIBRARY}"
                "$<TARGET_BUNDLE_DIR:${PLUGIN_TARGET}>/Contents/Frameworks/libusb-1.0.0.dylib"

            COMMAND /usr/bin/codesign --force --sign -
                "$<TARGET_BUNDLE_DIR:${PLUGIN_TARGET}>/Contents/Frameworks/libusb-1.0.0.dylib"

            VERBATIM
        )
    endforeach()
endif()