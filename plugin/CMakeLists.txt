# Rockband Mod (2026) -- modified from the original GH MIDI by Michael Loya
# (michaelloya.studio), AGPL-3.0. This file adds Windows/Linux build support;
# see README.md for the full credit / changes note (AGPL-3.0 §5a).
cmake_minimum_required(VERSION 3.24)
project(GHMidi VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # universal binary so Intel Macs can run it too
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)

# hidapi is compiled in statically — no Homebrew dependency for users
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)  # hidapi declares an old minimum
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
    # hidraw backend only -- skip hidapi's libusb backend so Linux builds
    # don't also need libusb-1.0-dev. hidraw itself needs libudev-dev
    # (see README's Linux build section).
    set(HIDAPI_WITH_LIBUSB OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(hidapi
    GIT_REPOSITORY https://github.com/libusb/hidapi.git
    GIT_TAG hidapi-0.14.0
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(JUCE hidapi)

# Tried a MinGW-w64/Ninja toolchain here first (no static linker workaround
# needed, in case you're tempted): JUCE 8 has actually dropped MinGW support
# outright (its own juce_TargetPlatform.h says so), and fails compiling
# juce_gui_basics' Windows UI-Automation accessibility code -- MinGW's headers
# don't declare CaretPosition_BeginningOfLine and friends. On Windows this
# needs real MSVC (Visual Studio 2022, "Desktop development with C++"); see
# README.md's Windows build section.

juce_add_plugin(GHMidi
    # Publisher identity for this mod -- deliberately distinct from the
    # original's "Michael Loya Studio" / studio.michaelloya.ghmidi / Ghmd so
    # both plugins can be installed side by side without a VST3 UID clash.
    # Original-work credit lives in README.md, not here (see AGPL-3.0 §5a).
    COMPANY_NAME "NW Studio"
    BUNDLE_ID "studio.nwstudio.ghmidirockbandmod"
    PRODUCT_NAME "GH MIDI Rockband Mod"
    PLUGIN_MANUFACTURER_CODE Nwst
    PLUGIN_CODE Ghrb
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    # JUCE's standalone Options menu always offers "Bluetooth MIDI", and
    # tapping it asks CoreBluetooth for access. Without a usage string in
    # Info.plist macOS does not deny the request, it SIGABRTs the process --
    # confirmed in the crash report as a TCC privacy violation. GH MIDI does
    # not use Bluetooth itself; this only stops the OS killing the app when
    # someone opens that menu item. (Apple/Info.plist-only; harmless no-op
    # on Windows/Linux.)
    # "GH" set in MetalMania on the app's own stage background. Regenerate
    # with tools_make_icon.py -- it ports kBgFrag out of HighwayRenderer.cpp
    # rather than upscaling a screenshot.
    ICON_BIG "assets/icon.png"
    BLUETOOTH_PERMISSION_ENABLED TRUE
    BLUETOOTH_PERMISSION_TEXT "GH MIDI needs Bluetooth only if you want to connect a Bluetooth MIDI device. The guitar itself connects over USB."
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(GHMidi PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/HighwayRenderer.cpp
    src/Theme.cpp
    src/QuickBindPanel.cpp
    src/ModelAsset.cpp
    src/DebugPanel.cpp
    src/AudioSfx.cpp
    src/ChartFile.cpp)

# assets/models/*.obj + *.png: YARG-derived (https://github.com/YARC-Official/YARG,
# LGPL-3.0-or-later) gem/fret meshes and textures, converted once, offline,
# by tools/model_convert/ -- see that tool and README.md for the credit /
# conversion notes. Never re-converted at build time.
# assets/sprites/rockband4/*.PNG: the RB4 Stratocaster QuickBind diagram --
# a Background plus one same-canvas-size PNG per control, each showing just
# that control highlighted. QuickBindPanel composites Background with
# whichever control PNGs are relevant, alpha-driven by mapped/learning/live
# state (see QuickBindPanel.cpp). Supplied directly by the project owner,
# not sourced from a third party like the YARG models above.
# assets/sfx/*.wav: Guitar Hero III sound effects (intro highway animation,
# Standalone UI sounds -- see AudioSfx.cpp/HighwayRenderer.cpp), supplied
# directly by the project owner. Unlike the YARG models above, these are not
# under a license that permits redistribution -- the project owner was told
# this plainly and chose to include them anyway.
juce_add_binary_data(GHMidiAssets SOURCES
    assets/MetalMania-Regular.ttf
    assets/models/yarg_fret.obj
    assets/models/yarg_fret.png
    assets/models/yarg_note.obj
    assets/models/yarg_note.png
    assets/sprites/rockband4/Background.PNG
    assets/sprites/rockband4/Dpad_left.PNG
    assets/sprites/rockband4/Dpad_right.PNG
    assets/sprites/rockband4/Fret_blue.PNG
    assets/sprites/rockband4/Fret_green.PNG
    assets/sprites/rockband4/Fret_orange.PNG
    assets/sprites/rockband4/Fret_red.PNG
    assets/sprites/rockband4/Fret_solo_blue.PNG
    assets/sprites/rockband4/Fret_solo_green.PNG
    assets/sprites/rockband4/Fret_solo_orange.PNG
    assets/sprites/rockband4/Fret_solo_red.PNG
    assets/sprites/rockband4/Fret_solo_yellow.PNG
    assets/sprites/rockband4/Fret_yellow.PNG
    assets/sprites/rockband4/Pause.PNG
    assets/sprites/rockband4/Select.PNG
    assets/sprites/rockband4/SoundEffect_bar.PNG
    assets/sprites/rockband4/Strum_down.PNG
    assets/sprites/rockband4/Strum_up.PNG
    assets/sprites/rockband4/Tilt.PNG
    assets/sprites/rockband4/WammyBar.PNG
    assets/sfx/bad_note1.wav
    assets/sfx/bad_note2.wav
    assets/sfx/bad_note3.wav
    assets/sfx/bad_note4.wav
    assets/sfx/bad_note6.wav
    assets/sfx/checkbox_off.wav
    assets/sfx/checkbox_on.wav
    assets/sfx/highway_rise.wav
    assets/sfx/music_scroll.wav
    assets/sfx/music_select.wav
    assets/sfx/notes_ripple_up.wav
    assets/sfx/song_failed.wav
    assets/sfx/sp_available.wav
    assets/sfx/sp_awarded1.wav
    assets/sfx/sp_awarded2.wav
    assets/sfx/sp_deployed.wav
    assets/sfx/star_available.wav
    assets/sfx/star_release.wav
    assets/sfx/sudden_death.wav
    assets/sfx/you_rock.wav)


target_compile_definitions(GHMidi PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    GL_SILENCE_DEPRECATION=1)

target_link_libraries(GHMidi PRIVATE
    GHMidiAssets
    hidapi::hidapi
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
