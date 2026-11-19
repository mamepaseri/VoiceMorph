cmake_minimum_required(VERSION 3.16)
project(VoiceMorphSpeechFeatureTests LANGUAGES CXX)

add_executable(speech_feature_test
    speech_feature_test.cpp
    ../Source/Dsp/SpeechFeatureAnalyzer.cpp
    ../Source/Dsp/AdaptivePeriodicSplitter.cpp
    ../Source/Dsp/LatencyAlignedRecombiner.cpp
    ../Source/Dsp/HybridVoicePath.cpp
    ../Source/Dsp/SignalsmithHybridVoiceEngine.cpp
    ../Source/Dsp/PitchSynchronousShifter.cpp
    ../Source/Dsp/SignalsmithPitchEngine.cpp)
target_compile_features(speech_feature_test PRIVATE cxx_std_17)

option(VOICEMORPH_TEST_SIGNALSMITH "Fetch Signalsmith and test its pitch-shift path" OFF)
if (VOICEMORPH_TEST_SIGNALSMITH)
    include(FetchContent)
    FetchContent_Declare(signalsmith_stretch
        GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
        GIT_TAG 57b93f4e9206a089a45387eaa39bdc9f310d3308
        GIT_SHALLOW FALSE
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(signalsmith_stretch)
    if (NOT TARGET signalsmith-stretch)
        message(FATAL_ERROR "Signalsmith Stretch did not create the expected target.")
    endif()
    target_link_libraries(speech_feature_test PRIVATE signalsmith-stretch)
    target_compile_definitions(speech_feature_test PRIVATE VOICEMORPH_HAS_SIGNALSMITH=1)
    if (NOT MSVC)
        # The pinned upstream headers trigger -Wshadow; the OFF test job keeps
        # strict warning-as-error coverage for VoiceMorph's own source.
        target_compile_options(speech_feature_test PRIVATE -Wno-shadow)
    endif()
else()
    target_compile_definitions(speech_feature_test PRIVATE VOICEMORPH_HAS_SIGNALSMITH=0)
endif()

if (MSVC)
    target_compile_options(speech_feature_test PRIVATE /W4 /permissive-)
else()
    target_compile_options(speech_feature_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
endif()

option(VOICEMORPH_TEST_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if (VOICEMORPH_TEST_SANITIZERS AND NOT MSVC)
    target_compile_options(speech_feature_test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(speech_feature_test PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME speech_feature_test COMMAND speech_feature_test)
