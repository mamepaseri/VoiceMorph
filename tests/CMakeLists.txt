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
