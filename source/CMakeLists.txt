cmake_minimum_required(VERSION 3.25)
project(Crusher VERSION 1.45.2 LANGUAGES C CXX)

set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../_Tools/JUCE/_9.0.4")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE 9.0.4 not found at: ${JUCE_DIR}")
endif()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(Crusher
    VERSION 1.45.2
    COMPANY_NAME "sl23"
    BUNDLE_ID "com.sl23.crusher"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE sl23
    PLUGIN_CODE cru1
    FORMATS VST3
    VST3_CATEGORIES Fx Distortion
    PRODUCT_NAME "Crusher")

# The user owns Resources, which is never packaged in development ZIPs.
# Embed the required PNG logo and optional JPEG background so the portable
# VST3 needs no external artwork. Rebuild after replacing either image.
set(CRUSHER_RESOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/../Resources/logo.png")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../Resources/crusher_background.jpg")
    list(APPEND CRUSHER_RESOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/../Resources/crusher_background.jpg")
    target_compile_definitions(Crusher PRIVATE CRUSHER_HAS_BACKGROUND=1)
endif()
juce_add_binary_data(CrusherResources SOURCES ${CRUSHER_RESOURCE_FILES})

juce_generate_juce_header(Crusher)
target_sources(Crusher PRIVATE
    PluginProcessor.cpp PluginProcessor.h CrusherDistortion.cpp
    CrusherAmpSim.h CrusherAsym.h CrusherBumpy.h CrusherComb.h CrusherCrackle.h CrusherCodec.h CrusherVocal.h CrusherDiodeClip.h CrusherFresh.h CrusherFuzz.h CrusherHardClip.h CrusherHump.h CrusherLightning.h CrusherLoFi.h CrusherPixelated.h CrusherRazor.h CrusherRectify.h CrusherSamurai.h CrusherSaturation.h CrusherSoftClip.h CrusherGraphicEQ.h
    CrusherStressed.h CrusherTube.h
    PluginEditor.cpp PluginEditor.h
    CrusherSequencer.h)
target_compile_features(Crusher PRIVATE cxx_std_17)
target_compile_definitions(Crusher PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Crusher PRIVATE juce::juce_audio_utils CrusherResources
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags
           juce::juce_recommended_lto_flags)
