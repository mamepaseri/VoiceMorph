cmake_minimum_required(VERSION 3.24)
project(VoiceMorph VERSION 0.8.0 LANGUAGES C CXX)

include(FetchContent)

# Framework. JUCE is dual-licensed (AGPLv3 or commercial); see THIRD_PARTY_NOTICES.md.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Optional permissive DSP backends. They are fetched from their upstream projects
# and are NOT copied into this repository. Keep these switches on for the full
# research build; turn them off for an offline UI-only build.
option(VOICEMORPH_ENABLE_SIGNALSMITH "Fetch and compile Signalsmith Stretch adapter (MIT)" ON)
option(VOICEMORPH_ENABLE_WORLD "Fetch and compile WORLD analysis backend (modified BSD)" OFF)
set(VOICEMORPH_SIGNALSMITH_GIT_TAG "57b93f4e9206a089a45387eaa39bdc9f310d3308" CACHE STRING "Pinned Signalsmith Stretch revision")
set(VOICEMORPH_WORLD_GIT_TAG "v1.0.1" CACHE STRING "WORLD release tag")

if (VOICEMORPH_ENABLE_SIGNALSMITH)
    FetchContent_Declare(signalsmith_stretch
        GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
        GIT_TAG ${VOICEMORPH_SIGNALSMITH_GIT_TAG}
        GIT_SHALLOW FALSE # Required when pinning an exact commit hash.
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(signalsmith_stretch)
endif()

if (VOICEMORPH_ENABLE_WORLD)
    # WORLD's examples/tests are not needed inside a plug-in build.
    set(WORLD_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WORLD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(world
        GIT_REPOSITORY https://github.com/mmorise/World.git
        GIT_TAG ${VOICEMORPH_WORLD_GIT_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(world)
endif()

juce_add_plugin(VoiceMorph
    COMPANY_NAME "VoiceMorph"
    BUNDLE_ID "com.voicemorph.plugin"
    PLUGIN_MANUFACTURER_CODE Vmor
    PLUGIN_CODE Vmc1
    PRODUCT_NAME "VoiceMorph"
    DESCRIPTION "Natural voice transformation prototype"
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(VoiceMorph)

target_sources(VoiceMorph PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Dsp/SignalsmithPitchEngine.cpp
    Source/Dsp/SignalsmithPitchEngine.h
    Source/Dsp/WorldSpeechAnalyzer.cpp
    Source/Dsp/WorldSpeechAnalyzer.h
    Source/Dsp/SpeechFeatureAnalyzer.cpp
    Source/Dsp/SpeechFeatureAnalyzer.h
    Source/Dsp/AdaptivePeriodicSplitter.cpp
    Source/Dsp/AdaptivePeriodicSplitter.h
    Source/Dsp/LatencyAlignedRecombiner.cpp
    Source/Dsp/HybridVoicePath.cpp
    Source/Dsp/HybridVoicePath.h
    Source/Dsp/SignalsmithHybridVoiceEngine.cpp
    Source/Dsp/SignalsmithHybridVoiceEngine.h
    Source/Dsp/LatencyAlignedRecombiner.h
    Source/Dsp/PitchSynchronousShifter.cpp
    Source/Dsp/PitchSynchronousShifter.h)

target_compile_features(VoiceMorph PRIVATE cxx_std_17)
target_compile_definitions(VoiceMorph PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if (VOICEMORPH_ENABLE_SIGNALSMITH)
    if (TARGET signalsmith-stretch)
        target_link_libraries(VoiceMorph PRIVATE signalsmith-stretch)
    else()
        message(FATAL_ERROR "Signalsmith Stretch was fetched, but its CMake target signalsmith-stretch was not created.")
    endif()
    target_compile_definitions(VoiceMorph PRIVATE VOICEMORPH_HAS_SIGNALSMITH=1)
else()
    target_compile_definitions(VoiceMorph PRIVATE VOICEMORPH_HAS_SIGNALSMITH=0)
endif()

if (VOICEMORPH_ENABLE_WORLD)
    if (TARGET world::core)
        target_link_libraries(VoiceMorph PRIVATE world::core)
    elseif (TARGET world)
        target_link_libraries(VoiceMorph PRIVATE world)
    else()
        message(FATAL_ERROR "WORLD was fetched, but no WORLD library target was created.")
    endif()
    target_compile_definitions(VoiceMorph PRIVATE VOICEMORPH_HAS_WORLD=1)
else()
    target_compile_definitions(VoiceMorph PRIVATE VOICEMORPH_HAS_WORLD=0)
endif()

target_link_libraries(VoiceMorph PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
