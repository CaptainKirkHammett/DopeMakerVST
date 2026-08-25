cmake_minimum_required(VERSION 3.22)
project(SuomiSampler VERSION 0.2780.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
)

juce_add_binary_data(SuomiSamplerBinaryData SOURCES DOPEsmall.png Inconsolata.ttf Inconsolata-Bold.ttf "Kirna - Glitter2000 cover for vst.png" om.png alleye.png spuge2.png 02780.png signal_path_simple.png)

juce_add_plugin(SuomiSampler
    COMPANY_NAME                "Dope Science"
    PLUGIN_MANUFACTURER_CODE    "DOPE"
    PLUGIN_CODE                 "DpMk"
    FORMATS                     VST3 AU
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Instrument|Sampler"
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    PRODUCT_NAME                "Dope Maker"
)

target_sources(SuomiSampler PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/PresetManager.cpp
    src/ui/PresetManagerPanel.cpp
    src/engine/SamplerEngine.cpp
    src/engine/SamplerVoice.cpp
    src/engine/AdsrEnvelope.cpp
    src/dsp/GranulizerProcessor.cpp
    src/dsp/StutterProcessor.cpp
    src/dsp/StereoProcessor.cpp
    src/dsp/LimiterProcessor.cpp
    src/dsp/FilterProcessor.cpp
    src/dsp/DistortionProcessor.cpp
    src/dsp/FlangerProcessor.cpp
    src/dsp/ReverbProcessor.cpp
    src/dsp/PhaserProcessor.cpp
    src/dsp/PingPongDelayProcessor.cpp
    src/dsp/LfoProcessor.cpp
    src/dsp/GateController.cpp
    src/dsp/EnvelopeFollower.cpp
    src/dsp/ArpeggiatorProcessor.cpp
    src/dsp/ClipProcessor.cpp
    src/dsp/ToneFilterProcessor.cpp
    src/ui/SuomiLookAndFeel.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/WaveformMarker.cpp
    src/ui/SampleControlPanel.cpp
    src/ui/GoniometerComponent.cpp
    src/ui/MagixxPanel.cpp
    src/ui/GranulizerPanel.cpp
    src/ui/StutterPanel.cpp
    src/ui/EnvelopePanel.cpp
    src/ui/FilterPanel.cpp
    src/ui/DistortionPanel.cpp
    src/ui/FlangerPanel.cpp
    src/ui/ReverbPanel.cpp
    src/ui/PhaserPanel.cpp
    src/ui/DelayPanel.cpp
    src/ui/DuckFaderButton.cpp
    src/ui/EnvFollowerPanel.cpp
    src/ui/GateControllerPanel.cpp
    src/ui/SidechainPanel.cpp
    src/ui/ArpeggiatorPanel.cpp
    src/ui/ClipPanel.cpp
    src/ui/HighcutPanel.cpp
    src/ui/LowcutPanel.cpp
    src/ui/ParameterDisplay.cpp
    src/ui/VuMeter.cpp
    src/ui/LfoModIndicator.cpp
    src/ui/VelocityMeterComponent.cpp
    src/dsp/SidechainProcessor.cpp
    src/dsp/ResonanceSuppressorProcessor.cpp
    src/ui/SuppressorPanel.cpp
)

target_compile_definitions(SuomiSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SuomiSampler PRIVATE
    SuomiSamplerBinaryData
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
