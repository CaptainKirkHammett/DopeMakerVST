#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DopeMakerState", createParameterLayout()),
      engine (std::make_unique<SamplerEngine>())
{
    formatManager.registerBasicFormats();
    setupLfoNoteOnCallback();

    // Cache gate step parameter pointers to avoid per-block string lookups.
    for (int i = 0; i < GateController::kMaxSteps; ++i)
        gateStepPtrs[i] = apvts.getRawParameterValue (ParamIDs::gateStepId (i).c_str());

    // Register listener to enforce start < end constraint.
    apvts.addParameterListener (ParamIDs::sampleStartPos, this);
    apvts.addParameterListener (ParamIDs::sampleEndPos,   this);
}

PluginProcessor::~PluginProcessor()
{
    apvts.removeParameterListener (ParamIDs::sampleStartPos, this);
    apvts.removeParameterListener (ParamIDs::sampleEndPos,   this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    using APVTS = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- Pre/Post Gain ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::preGainDb, "Pre Gain",
        juce::NormalisableRange<float> (-40.0f, 12.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::postGainDb, "Post Gain",
        juce::NormalisableRange<float> (-40.0f, 12.0f), 0.0f));

    // --- Sample Engine ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::gainDb, "Gain",
        juce::NormalisableRange<float> (-40.0f, 12.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stereoWidth, "Stereo Width", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::limiterAmount, "Limiter",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 4); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sampleStartPos, "Sample Start",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 4); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sampleEndPos, "Sample End",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 4); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::zcPerChannel, "ZC Per Channel", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::sampleDirection, "Sample Direction",
        juce::StringArray { "Sample FW", "Sample BW" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::loopMode, "Loop Mode",
        juce::StringArray { "No Loop", "Forward", "Ping-Pong" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        ParamIDs::rootNote, "Root Note", 0, 119, 48));
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        ParamIDs::semitoneOffset, "Semitone Offset", -48, 48, 0));
    {
        juce::StringArray pbChoices;
        for (int i = 1; i <= 24; ++i) pbChoices.add (juce::String (i));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::pitchBendRange, "Pitch Bend Range", pbChoices, 11)); // default index 11 = 12 semitones
    }
    {
        juce::StringArray vcChoices;
        for (int i = 1; i <= 16; ++i) vcChoices.add (juce::String (i));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::voiceCount, "Voice Count", vcChoices, 0)); // default index 0 = 1 voice (mono)
    }
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::portamento, "Portamento",
        juce::NormalisableRange<float> (0.0f, 1000.0f, 0.0f, 0.3f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::normalizeEnabled, "Normalize", false));

    // --- Velocity LFO ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::velLfoProb, "Note Play Probability", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::velLfoWaveform, "Vel LFO Waveform",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" },
        0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::velLfoRate, "Velocity LFO Rate",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::velLfoDepth, "Velocity LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::velLfoTempoSync, "Vel LFO Tempo Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::velLfoSyncDiv, "Vel LFO Sync Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));

    // --- Sample Start LFO ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::lfoStartStepOverride, "Step Controller Override", false));

    // --- Slicer ---
    {
        juce::StringArray noteNames;
        for (int i = 0; i < 128; ++i)
            noteNames.add (juce::MidiMessage::getMidiNoteName (i, true, true, 4));
        juce::StringArray sliceCounts;
        for (int i = 1; i <= 128; ++i)
            sliceCounts.add (juce::String (i));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            ParamIDs::slicerEnabled, "Slicer Mode", false));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::slicerLoNote, "Slice Start", noteNames, 60)); // C3 (middle C)
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::sliceCount, "Slices", sliceCounts, 15)); // index 15 = 16 slices (default)
    }

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::lfoWaveform, "LFO Waveform",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" },
        0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::lfoRate, "Sample Start LFO Rate",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::lfoDepth, "Sample Start LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::lfoTempoSync, "LFO Tempo Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::lfoSyncDiv, "LFO Sync Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));

    // --- Volume Envelope ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volAttack,  "Volume Envelope Attack",
        juce::NormalisableRange<float> (0.0f, 10000.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volDecay,   "Volume Envelope Decay",
        juce::NormalisableRange<float> (0.0f, 10000.0f, 0.0f, 0.3f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volSustain, "Volume Envelope Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volRelease, "Volume Envelope Release",
        juce::NormalisableRange<float> (0.0f, 10000.0f, 0.0f, 0.3f), 200.0f));

    // --- Granulizer ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::granEnabled, "Granulizer Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granAttack, "Granulizer Attack",
        juce::NormalisableRange<float> (1.0f, 500.0f, 0.0f, 0.4f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granHold, "Granulizer Hold",
        juce::NormalisableRange<float> (1.0f, 500.0f, 0.0f, 0.4f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granSpacing, "Granulizer Spacing",
        juce::NormalisableRange<float> (5.0f, 500.0f, 0.0f, 0.4f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granWaveSpacing, "Granulizer Wave Spacing",
        juce::NormalisableRange<float> (-3.0f, 3.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (static_cast<int> (v * 100.0f)) + "%"; }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granPan, "Granulizer Pan",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granDepth, "Granulizer Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granSpeed, "Granulizer Speed",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::granRand, "Granulizer Rand",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // --- Volume Release LFO ---
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::volRelLfoWaveform, "Vol Rel LFO Wave",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" },
        0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volRelLfoRate, "Vol Rel LFO Rate",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volRelLfoDepth, "Vol Rel LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::volRelLfoTempoSync, "Vol Rel LFO Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::volRelLfoSyncDiv, "Vol Rel LFO Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));

    // --- Pitch LFO ---
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::pitchLfoWaveform, "Pitch LFO Wave",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" },
        0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchLfoRate, "Pitch LFO Rate",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchLfoDepth, "Pitch LFO Depth", 0.0f, 64.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::pitchLfoTempoSync, "Pitch LFO Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::pitchLfoSyncDiv, "Pitch LFO Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));

    // --- Pitch Envelope ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchAttack,  "Pitch Attack",
        juce::NormalisableRange<float> (0.0f, 10000.0f, 0.0f, 0.3f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchDecay,   "Pitch Decay",
        juce::NormalisableRange<float> (0.0f, 5000.0f, 0.0f, 0.3f), 500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchSustain, "Pitch Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchRelease, "Pitch Release",
        juce::NormalisableRange<float> (0.0f, 2000.0f, 0.0f, 0.3f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchEnvDepth, "Pitch Envelope Depth", -128.0f, 128.0f, 0.0f));

    // --- Filter ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::filterEnabled, "Filter Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::filterMode, "Filter Mode",
        juce::StringArray { "Low-Pass", "High-Pass", "Band-Pass", "Comb+", "Comb-" }, 4));  // default: Comb-
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::filterCutoff, "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::filterResonance, "Filter Resonance",
        juce::NormalisableRange<float> (0.1f, 10.0f), 8.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::filterLfoWaveform, "Filter LFO Waveform",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::filterLfoRate, "Filter LFO Rate",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::filterLfoDepth, "Filter LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.05f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // --- Distortion ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::distEnabled, "Distortion Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::distMode, "Distortion Mode",
        juce::StringArray { "Bit Crush","Clip Hard","Clip Soft","Foldback",
                            "Ring Mod","Sat Hard","Sat Soft","Waveshaper" }, 4)); // default: Ring Mod
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::distDrive, "Distortion Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::distOutput, "Distortion Output", -24.0f, 0.0f, -6.0f));
    {
        juce::StringArray bdChoices;
        for (int i = 2; i <= 16; ++i) bdChoices.add (juce::String (i));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::distBitDepth, "Bit Depth", bdChoices, 6)); // default index 6 = 8 bits
    }
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::distDriveLow, "Distortion Band Low",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::distDriveMid, "Distortion Band Mid",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::distDriveHigh, "Distortion Band High",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // --- Flanger ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::flangerEnabled, "Flanger Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::flangerRate, "Flanger Rate",
        juce::NormalisableRange<float> (0.01f, 5.0f, 0.0f, 0.5f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::flangerFeedback, "Flanger Feedback",
        juce::NormalisableRange<float> (-0.99f, 0.99f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::flangerDelayStart, "Flanger Delay Start", 1.0f, 20.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::flangerDelayEnd, "Flanger Delay End", 1.0f, 20.0f, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::flangerMix, "Flanger Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // --- Reverb ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::reverbEnabled, "Reverb Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::reverbRoomSize, "Reverb Room Size",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::reverbDamping, "Reverb Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::reverbWidth, "Reverb Width",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::reverbMix, "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::reverbReverse, "Reverb Reverse", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::reverbRevLen, "Reverb Reverse Length",
        juce::NormalisableRange<float> (100.0f, 4000.0f, 1.0f, 0.4f), 500.0f,
        "ms"));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::reverbRevSync, "Reverb Reverse Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::reverbRevDiv, "Reverb Reverse Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));   // default: 1/4

    // --- Phaser ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::phaserEnabled, "Phaser Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::phaserRate, "Phaser Rate",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.3f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::phaserDepth, "Phaser Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::phaserFeedback, "Phaser Feedback",
        juce::NormalisableRange<float> (-0.99f, 0.99f), 0.6f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::phaserStages, "Phaser Stages",
        juce::StringArray { "2","4","6","8","10","12" }, 3));   // default 8 = index 3
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::phaserCentreFreq, "Phaser Centre",
        juce::NormalisableRange<float> (200.0f, 20000.0f, 0.0f, 0.3f), 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::phaserMix, "Phaser Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // --- Gate Controller ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::gateEnabled, "Step Controller Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::gateStepCount, "Gate Steps",
        juce::NormalisableRange<float> (1.0f, 32.0f, 1.0f), 16.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::gateDivision, "Gate Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        1)); // default: 1/16
    for (int i = 0; i < GateController::kMaxSteps; ++i)
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            ParamIDs::gateStepId (i), "Gate Step " + juce::String (i + 1),
            0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::envFilterAmt,  "Step Filter Amount",  -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::envDistAmt,    "Step Distortion Amount",    -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::envFlangerAmt, "Step Flanger Amount", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::envPhaserAmt,  "Step Phaser Amount",  -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::envDelayAmt,   "Step Delay Amount",   -1.0f, 1.0f, 0.0f));

    // --- Audio envelope follower ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::envFollowerEnabled, "Envelope Follower Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::envFollowerAttack,  "Env Follower Attack",
        juce::NormalisableRange<float> (1.0f, 500.0f, 0.1f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::envFollowerRelease, "Env Follower Release",
        juce::NormalisableRange<float> (10.0f, 2000.0f, 1.0f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::afEnvFilterAmt,  "Envelope Ctrl Filter Amount",  -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::afEnvDistAmt,    "Envelope Ctrl Distortion Amount",    -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::afEnvFlangerAmt, "Envelope Ctrl Flanger Amount", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::afEnvPhaserAmt,  "Envelope Ctrl Phaser Amount",  -1.0f, 1.0f, 0.0f));

    // --- Ping-Pong Delay ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::delayEnabled, "Delay Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::delayKillTail, "Kill Tail", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayTimeLMs, "Delay L",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 0.0f, 0.3f), 375.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayTimeRMs, "Delay R",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 0.0f, 0.3f), 500.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::delayTempoSync, "Delay Tempo Sync", true));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::delayTimeLDiv, "Delay L Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::delayTimeRDiv, "Delay R Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        3));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayFeedback, "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.99f), 0.4f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayMix, "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // Feedback filter.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::delayFbFilterMode, "Delay Feedback Filter Type",
        juce::StringArray { "Low-Pass","High-Pass","Band-Pass" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayFbCutoff, "Delay Feedback Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 20000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayFbResonance, "Delay Feedback Resonance",
        juce::NormalisableRange<float> (0.1f, 10.0f), 0.707f));

    // Feedback filter LFO.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::delayFbLfoWave, "Delay Feedback LFO Wave",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayFbLfoRate, "Delay Feedback LFO Rate",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.3f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayFbLfoDepth, "Delay Feedback LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // Mix LFO.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::delayMixLfoWave, "Delay Mix LFO Wave",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" }, 4));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayMixLfoRate, "Delay Mix LFO Rate",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.3f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayMixLfoDepth, "Delay Mix LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::delayMixLfoSync, "Delay Mix LFO Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::delayMixLfoDiv, "Delay Mix LFO Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayDuck, "Delay Duck",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    // --- Highcut (LP) ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::highcutEnabled, "Highcut Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::highcutFreq, "Highcut Frequency",
        juce::NormalisableRange<float> (200.0f, 20000.0f, 0.0f, 0.25f), 8000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::highcutAmt, "Highcut Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) {
            const int stages = juce::jlimit (1, 8, static_cast<int> (v * 8) + 1);
            return juce::String (stages * 12) + " dB";
        }));

    // --- Lowcut (HP) ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::lowcutEnabled, "Lowcut Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::lowcutFreq, "Lowcut Frequency",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 0.0f, 0.25f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::lowcutAmt, "Lowcut Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) {
            const int stages = juce::jlimit (1, 8, static_cast<int> (v * 8) + 1);
            return juce::String (stages * 12) + " dB";
        }));

    // --- Clip ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::clipEnabled, "Clip Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::clipMode, "Clip Mode",
        juce::StringArray { "Hard", "Soft" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::clipAmt, "Clip Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 0.5f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::clipMakeup, "Clip Makeup",
        juce::NormalisableRange<float> (0.0f, 24.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " dB"; }));

    // --- Arpeggiator ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::arpEnabled, "Arpeggiator Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::arpOctaves, "Arpeggiator Octaves",
        juce::StringArray { "1", "2", "3", "4", "5" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::arpDirection, "Arpeggiator Direction",
        juce::StringArray { "Up", "Down", "Random" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::arpNoteLength, "Arpeggiator Note Length", 0.0f, 1.0f, 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::arpDivision, "Arpeggiator Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        1));  // default: 1/16
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::arpProbability, "Arp Play Probability", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::arpLfoWaveform, "Arpeggiator LFO Waveform",
        juce::StringArray { "Sine","Tri","Sqr","SawU","SawD","Rnd~","Rnd|" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::arpLfoDivision, "Arp LFO Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));  // default: 1/4
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::arpLfoDepth, "Arp Note Length LFO Depth", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::arpTie, "Arpeggiator Tie", 0.0f, 1.0f, 0.0f));

    // --- Sidechain volume modulator ---
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::sidechainMode, "Sidechain Mode",
        juce::StringArray { "Sidechain Off", "Sidechain Pre", "Sidechain Post" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::sidechainDivision, "Sidechain Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" },
        4));  // default: 1/4
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sidechainMix, "Sidechain Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sidechainPreDelay, "Sidechain Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sidechainRise, "Sidechain Rise",
        juce::NormalisableRange<float> (0.01f, 1.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sidechainCurve, "Sidechain Curve",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sidechainFall, "Sidechain Fall",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));

    // --- Stutter ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::stutterEnabled, "Stutter Enable", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stutterAmount, "Stutter Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stutterLfoLow, "Stutter LFO Low",
        juce::NormalisableRange<float> (0.0f, 4.0f, 1.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { static const char* names[] = {"1/4","1/8","1/16","1/32","1/64"};
                            return juce::String (names[juce::jlimit (0, 4, juce::roundToInt (v))]); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stutterLfoHigh, "Stutter LFO High",
        juce::NormalisableRange<float> (0.0f, 4.0f, 1.0f), 2.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { static const char* names[] = {"1/4","1/8","1/16","1/32","1/64"};
                            return juce::String (names[juce::jlimit (0, 4, juce::roundToInt (v))]); }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stutterLfoSpeed, "Stutter LFO Speed",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.3f), 0.3f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 2) + " Hz"; }));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stutterMidiLength, "Stutter Length",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::stutterLenRepeat, "Stutter Len Repeat Mode", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::stutterLenLfoWaveform, "Stutter Len LFO Waveform",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stutterLenLfoRate, "Stutter Len LFO Rate",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::stutterLenLfoDepth, "Stutter Len LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }));

    return { params.begin(), params.end() };
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;

    engine->prepare    (spec);

    gainSmoothed.reset (sampleRate, 0.02);  // 20 ms ramp — fast enough to track automation, slow enough to avoid zipper noise
    gainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::gainDb)->load()));

    preGainSmoothed.reset (sampleRate, 0.02);
    preGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::preGainDb)->load()));

    postGainSmoothed.reset (sampleRate, 0.02);
    postGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::postGainDb)->load()));

    normalizeSmoothed.reset (sampleRate, 0.02);
    normalizeSmoothed.setCurrentAndTargetValue (
        apvts.getRawParameterValue (ParamIDs::normalizeEnabled)->load() > 0.5f
            ? normalizeGain.load (std::memory_order_relaxed)
            : 1.0f);

    stereoProc.prepare  (spec);
    limiterProc.prepare (spec);
    filterProc.prepare  (spec);
    distProc.prepare    (spec);
    flangerProc.prepare  (spec);
    phaserProc.prepare   (spec);
    reverbProc.prepare   (spec);
    delayProc.prepare    (spec);
    highcutProc.prepare  (spec);
    lowcutProc.prepare   (spec);
    lowcutSpectrumSource.setSampleRate (sampleRate);
    clipProc.prepare     (spec);
    granulizerProc.prepare (sampleRate, samplesPerBlock);
    stutterProc.prepare    (sampleRate, samplesPerBlock);
    lfo.prepare            (sampleRate);
    velLfo.prepare         (sampleRate);
    filterLfo.prepare      (sampleRate);
    stutterLenLfo.prepare  (sampleRate);
    gateController.prepare   (spec);
    audioEnvFollower.prepare (spec);
    sidechainProc.prepare    (sampleRate);
    arpProc.prepare          (sampleRate);
}

void PluginProcessor::releaseResources() {}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear any channels not used for output.
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Voice count: index 0 = 1 voice (mono), index N = N+1 voices.
    {
        const int voiceCount = static_cast<int> (
            apvts.getRawParameterValue (ParamIDs::voiceCount)->load()) + 1;
        engine->setPolyMode (voiceCount > 1, voiceCount);
    }

    // Build the VoiceParams template from current parameter values.
    {
        auto& vp = engine->voiceParamsTemplate;
        vp.startPos      = apvts.getRawParameterValue (ParamIDs::sampleStartPos)->load();
        vp.endPos        = apvts.getRawParameterValue (ParamIDs::sampleEndPos)->load();
        vp.perChannelZC  = apvts.getRawParameterValue (ParamIDs::zcPerChannel)->load() > 0.5f;
        vp.reversed      = apvts.getRawParameterValue (ParamIDs::sampleDirection)->load() > 0.5f;
        {
            const int loopIdx = static_cast<int> (
                apvts.getRawParameterValue (ParamIDs::loopMode)->load());
            vp.looping  = (loopIdx > 0);
            vp.loopMode = (loopIdx == 2) ? LoopMode::PingPong : LoopMode::Forward;
        }
        vp.rootNote       = static_cast<int> (apvts.getRawParameterValue (ParamIDs::rootNote)->load());
        vp.semitoneOffset = static_cast<int> (apvts.getRawParameterValue (ParamIDs::semitoneOffset)->load());
        vp.pitchBendRange = static_cast<int> (apvts.getRawParameterValue (ParamIDs::pitchBendRange)->load()) + 1;  // choice index 0 = 1 semitone
        vp.portamentoMs   = apvts.getRawParameterValue (ParamIDs::portamento)->load();
        vp.volAttackMs   = apvts.getRawParameterValue (ParamIDs::volAttack)->load();
        vp.volDecayMs    = apvts.getRawParameterValue (ParamIDs::volDecay)->load();
        vp.volSustain    = apvts.getRawParameterValue (ParamIDs::volSustain)->load();
        vp.volReleaseMs  = apvts.getRawParameterValue (ParamIDs::volRelease)->load();
        vp.pitchAttackMs = apvts.getRawParameterValue (ParamIDs::pitchAttack)->load();
        vp.pitchDecayMs  = apvts.getRawParameterValue (ParamIDs::pitchDecay)->load();
        vp.pitchSustain  = apvts.getRawParameterValue (ParamIDs::pitchSustain)->load();
        vp.pitchReleaseMs = apvts.getRawParameterValue (ParamIDs::pitchRelease)->load();
        vp.pitchEnvDepth = apvts.getRawParameterValue (ParamIDs::pitchEnvDepth)->load();
        vp.volRelLfoWaveform = static_cast<int> (
            apvts.getRawParameterValue (ParamIDs::volRelLfoWaveform)->load());
        vp.volRelLfoRate  = apvts.getRawParameterValue (ParamIDs::volRelLfoRate)->load();
        vp.volRelLfoDepth = apvts.getRawParameterValue (ParamIDs::volRelLfoDepth)->load();

        // Resolve release LFO rate: free-run Hz or BPM-synced Hz.
        const bool relSync = apvts.getRawParameterValue (ParamIDs::volRelLfoTempoSync)->load() > 0.5f;
        if (relSync)
        {
            const auto divIdx = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::volRelLfoSyncDiv)->load()));
            vp.volRelLfoEffectiveRateHz = LfoProcessor::syncDivisionToHz (getCurrentBpm(), divIdx);
        }
        else
        {
            vp.volRelLfoEffectiveRateHz = vp.volRelLfoRate;
        }

        // Pitch LFO
        vp.pitchLfoWaveform = static_cast<int> (
            apvts.getRawParameterValue (ParamIDs::pitchLfoWaveform)->load());
        vp.pitchLfoDepth = apvts.getRawParameterValue (ParamIDs::pitchLfoDepth)->load();
        const bool pitchSync = apvts.getRawParameterValue (ParamIDs::pitchLfoTempoSync)->load() > 0.5f;
        if (pitchSync)
        {
            const auto divIdx = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::pitchLfoSyncDiv)->load()));
            vp.pitchLfoEffectiveRateHz = LfoProcessor::syncDivisionToHz (getCurrentBpm(), divIdx);
        }
        else
        {
            vp.pitchLfoEffectiveRateHz = apvts.getRawParameterValue (ParamIDs::pitchLfoRate)->load();
        }
    }

    // Read transport state — detect play-start and PPQ discontinuities (scrub/loop).
    {
        bool   isPlaying   = false;
        double ppqPosition = -1.0;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                isPlaying = pos->getIsPlaying();
                if (auto ppq = pos->getPpqPosition())
                    ppqPosition = *ppq;
            }
        }

        if (isPlaying)
        {
            const double bpm              = getCurrentBpm();
            const double blockDurationPpq = (buffer.getNumSamples() / getSampleRate()) * (bpm / 60.0);

            // Reset the pattern when play starts or when the transport jumps to a
            // different position (scrub, loop, rewind).  A normal block advance moves
            // PPQ forward by ~blockDurationPpq; anything else is a jump.
            const bool playStarted  = !wasPlaying;
            const bool transportJumped = ppqPosition >= 0.0
                                      && lastPpqPosition >= 0.0
                                      && std::abs (ppqPosition - lastPpqPosition) > blockDurationPpq * 4.0;

            if (playStarted || transportJumped)
            {
                gateController.reset();
                arpProc.reset();

                // Only hard-kill voices on initial play start.  On transport
                // loops/jumps the DAW re-sends MIDI — hard-killing here would
                // cut audio to zero in one sample, causing an audible click.
                if (playStarted)
                    engine->forceAllVoicesOff();
            }

            lastPpqPosition = (ppqPosition >= 0.0) ? ppqPosition + blockDurationPpq : -1.0;
        }
        else
        {
            lastPpqPosition = -1.0;
        }

        wasPlaying          = isPlaying;
        currentIsPlaying    = isPlaying;
        currentPpqPosition  = isPlaying ? ppqPosition : -1.0;
    }

    // Advance gate controller here so its output is available for both the step
    // override (which must reach lfoStartOffset before renderNextBlock) and the
    // post-render FX modulation.
    float envLevel = 0.0f;
    if (apvts.getRawParameterValue (ParamIDs::gateEnabled)->load() > 0.5f)
    {
        const int stepCount = static_cast<int> (
            apvts.getRawParameterValue (ParamIDs::gateStepCount)->load());
        const auto divIdx = static_cast<SyncDivision> (
            static_cast<int> (apvts.getRawParameterValue (ParamIDs::gateDivision)->load()));
        gateController.setStepCount (stepCount);
        gateController.setDivision  (divIdx);
        for (int i = 0; i < GateController::kMaxSteps; ++i)
            gateController.setStepAmplitude (i, gateStepPtrs[i]->load (std::memory_order_relaxed));
        envLevel = gateController.advance (buffer.getNumSamples(), getCurrentBpm());
    }

    // Arpeggiator — modifies MIDI before it reaches the sampler engine.
    {
        const bool  arpOn   = apvts.getRawParameterValue (ParamIDs::arpEnabled)->load() > 0.5f;
        const int   octIdx  = static_cast<int> (apvts.getRawParameterValue (ParamIDs::arpOctaves)->load());
        const auto  dir     = static_cast<ArpDirection> (
            static_cast<int> (apvts.getRawParameterValue (ParamIDs::arpDirection)->load()));
        const float noteLen = apvts.getRawParameterValue (ParamIDs::arpNoteLength)->load();
        const float prob    = apvts.getRawParameterValue (ParamIDs::arpProbability)->load();
        const auto  div     = static_cast<SyncDivision> (
            static_cast<int> (apvts.getRawParameterValue (ParamIDs::arpDivision)->load()));
        const int  lfoWaveIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::arpLfoWaveform)->load());
        const auto lfoWave    = static_cast<LfoWaveform> (lfoWaveIdx);
        const float lfoDepth  = apvts.getRawParameterValue (ParamIDs::arpLfoDepth)->load();
        // Depth knob controls whether LFO is active — no Off waveform option.
        const bool lfoActive  = (lfoDepth > 0.0f);
        const auto lfoDiv     = static_cast<SyncDivision> (
            static_cast<int> (apvts.getRawParameterValue (ParamIDs::arpLfoDivision)->load()));
        const float tieProbability = apvts.getRawParameterValue (ParamIDs::arpTie)->load();
        arpProc.process (midiMessages, buffer.getNumSamples(),
                         getCurrentBpm(), div, octIdx + 1, dir, noteLen, prob, arpOn,
                         lfoActive, lfoWave, lfoDiv, lfoDepth, tieProbability);
    }

    // Update LFO and push start-point offset to the engine.
    updateLfoAndPushOffset    (buffer.getNumSamples());
    updateVelLfoAndPushOffset (buffer.getNumSamples());

    // Step override: pressing the button disables the Sample Start LFO entirely.
    // When gate is also on, the step amplitude drives the offset instead.
    // Must run after updateLfoAndPushOffset so it wins the write.
    if (apvts.getRawParameterValue (ParamIDs::lfoStartStepOverride)->load() > 0.5f)
    {
        if (apvts.getRawParameterValue (ParamIDs::gateEnabled)->load() > 0.5f)
        {
            const float baseStart  = apvts.getRawParameterValue (ParamIDs::sampleStartPos)->load();
            const float endPos     = apvts.getRawParameterValue (ParamIDs::sampleEndPos)->load();
            const float windowSize = juce::jmax (0.0f, endPos - baseStart);
            engine->lfoStartOffset.store (envLevel * windowSize, std::memory_order_relaxed);
        }
        else
        {
            // Gate off but override active — kill LFO offset so it has no effect.
            engine->lfoStartOffset.store (0.0f, std::memory_order_relaxed);
        }
    }

    // Push slicer state to the engine so noteOn() can read it lock-free.
    engine->slicerEnabled.store (
        apvts.getRawParameterValue (ParamIDs::slicerEnabled)->load() > 0.5f,
        std::memory_order_relaxed);
    engine->slicerLoNote.store (
        static_cast<int> (apvts.getRawParameterValue (ParamIDs::slicerLoNote)->load()),
        std::memory_order_relaxed);
    // sliceCount parameter is 0-indexed (choice index); add 1 to get actual slice count.
    engine->sliceCount.store (
        static_cast<int> (apvts.getRawParameterValue (ParamIDs::sliceCount)->load()) + 1,
        std::memory_order_relaxed);

    // (Stutter is now an audio effect — processed later in the FX chain, before the filter.)

    // Render voices.
    engine->renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Peak normalize — smoothed toggle so enabling/disabling is click-free.
    {
        const float target = apvts.getRawParameterValue (ParamIDs::normalizeEnabled)->load() > 0.5f
                                 ? normalizeGain.load (std::memory_order_relaxed)
                                 : 1.0f;
        normalizeSmoothed.setTargetValue (target);

        if (normalizeSmoothed.isSmoothing() || target != 1.0f)
        {
            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                const float g = normalizeSmoothed.getNextValue();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.getWritePointer (ch)[s] *= g;
            }
        }
    }

    // Pre-gain — applied immediately after engine render, before any processing.
    {
        const float targetGain = juce::Decibels::decibelsToGain (
            apvts.getRawParameterValue (ParamIDs::preGainDb)->load());
        preGainSmoothed.setTargetValue (targetGain);

        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float g = preGainSmoothed.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[s] *= g;
        }
    }

    // Granulizer — post-engine, pre-FX chain.  Delay buffer is always updated
    // so toggling on is glitch-free; processing only occurs when enabled.
    granulizerProc.process (buffer,
        apvts.getRawParameterValue (ParamIDs::granEnabled)->load() > 0.5f,
        apvts.getRawParameterValue (ParamIDs::granAttack)->load(),
        apvts.getRawParameterValue (ParamIDs::granHold)->load(),
        apvts.getRawParameterValue (ParamIDs::granSpacing)->load(),
        apvts.getRawParameterValue (ParamIDs::granWaveSpacing)->load(),
        apvts.getRawParameterValue (ParamIDs::granPan)->load(),
        apvts.getRawParameterValue (ParamIDs::granDepth)->load(),
        apvts.getRawParameterValue (ParamIDs::granSpeed)->load(),
        apvts.getRawParameterValue (ParamIDs::granRand)->load());

    // Pre-FX VU measurement — captured after gain, before any FX processor.
    {
        const int n  = buffer.getNumSamples();
        const int nC = buffer.getNumChannels();
        vuPreFxL.store (nC > 0 ? buffer.getMagnitude (0, 0, n) : 0.0f, std::memory_order_relaxed);
        vuPreFxR.store (nC > 1 ? buffer.getMagnitude (1, 0, n) : 0.0f, std::memory_order_relaxed);
    }

    // Audio envelope follower — runs on the pre-FX signal so it tracks the dry source.
    float audioEnvLevel = 0.0f;
    if (apvts.getRawParameterValue (ParamIDs::envFollowerEnabled)->load() > 0.5f)
    {
        audioEnvFollower.setAttackMs  (apvts.getRawParameterValue (ParamIDs::envFollowerAttack)->load());
        audioEnvFollower.setReleaseMs (apvts.getRawParameterValue (ParamIDs::envFollowerRelease)->load());
        audioEnvLevel = audioEnvFollower.process (buffer);
    }
    audioEnvLevelForUI.store (audioEnvLevel, std::memory_order_relaxed);

    // Ensure stereo output for FX chain.
    if (buffer.getNumChannels() < 2)
        return;

    juce::dsp::AudioBlock<float>        block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // Signal path: Stereo Width → Limiter → Gain → FX chain.

    // Stereo width — applied first on the raw sampler output.
    {
        const float width = apvts.getRawParameterValue (ParamIDs::stereoWidth)->load();
        stereoProc.setWidth (width);
        stereoProc.process (block);
        stereoFieldSource.pushSamples (buffer);
    }

    // Limiter — always runs so its internal envelope stays primed;
    // at amount=0 the threshold sits at 0 dB (no limiting, pass-through).
    {
        const float amount = apvts.getRawParameterValue (ParamIDs::limiterAmount)->load();
        limiterProc.setAmount (amount);
        limiterProc.process (ctx);
        limiterGainReductionDb.store (
            amount > 0.0f
                ? limiterProc.gainReductionDb.load (std::memory_order_relaxed)
                : 0.0f,
            std::memory_order_relaxed);
    }

    // Gain — applied sample-by-sample via SmoothedValue so automation is zipper-free.
    {
        const float targetGain = juce::Decibels::decibelsToGain (
            apvts.getRawParameterValue (ParamIDs::gainDb)->load());
        gainSmoothed.setTargetValue (targetGain);

        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float g = gainSmoothed.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[s] *= g;
        }
    }

    // Sidechain Pre-FX — volume modulation before the FX chain.
    {
        const int scMode = static_cast<int> (
            apvts.getRawParameterValue (ParamIDs::sidechainMode)->load());
        if (scMode == 1)
        {
            const auto scDiv = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::sidechainDivision)->load()));
            sidechainProc.process (buffer, getCurrentBpm(), scDiv,
                apvts.getRawParameterValue (ParamIDs::sidechainPreDelay)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainRise)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainCurve)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainFall)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainMix)->load());
        }
    }

    // FX chain: Stutter → Filter → Distortion → Phaser → Ping-Pong Delay.
    // Each FX skipped entirely if disabled — no CPU waste.

    // Stutter Len LFO — modulates the Len parameter each block.
    {
        const float lenLfoDepth = apvts.getRawParameterValue (ParamIDs::stutterLenLfoDepth)->load();
        const int   lenLfoWaveIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::stutterLenLfoWaveform)->load());
        stutterLenLfo.setWaveform (static_cast<LfoWaveform> (lenLfoWaveIdx));
        stutterLenLfo.setRate     (apvts.getRawParameterValue (ParamIDs::stutterLenLfoRate)->load());
        stutterLenLfo.setDepth    (1.0f);
        stutterLenLfo.setTempoSync (false);
        stutterLenLfo.advance (buffer.getNumSamples() - 1);
        const float lenLfoRaw = stutterLenLfo.getNextSample();
        stutterLenLfoOutput.store (lenLfoRaw, std::memory_order_relaxed);

        const float baseLen = apvts.getRawParameterValue (ParamIDs::stutterMidiLength)->load();
        const float modulatedLen = juce::jlimit (0.0f, 1.0f, baseLen + lenLfoRaw * lenLfoDepth);

        stutterProc.process (buffer, buffer.getNumSamples(),
            getCurrentBpm(), currentPpqPosition,
            apvts.getRawParameterValue (ParamIDs::stutterEnabled)->load() > 0.5f,
            apvts.getRawParameterValue (ParamIDs::stutterAmount)->load(),
            apvts.getRawParameterValue (ParamIDs::stutterLfoSpeed)->load(),
            apvts.getRawParameterValue (ParamIDs::stutterLfoLow)->load(),
            apvts.getRawParameterValue (ParamIDs::stutterLfoHigh)->load(),
            modulatedLen,
            apvts.getRawParameterValue (ParamIDs::stutterLenRepeat)->load() > 0.5f);
    }

    if (apvts.getRawParameterValue (ParamIDs::filterEnabled)->load() > 0.5f)
    {
        const int   modeIdx    = static_cast<int> (apvts.getRawParameterValue (ParamIDs::filterMode)->load());
        const float baseCutoff = apvts.getRawParameterValue (ParamIDs::filterCutoff)->load();
        const float res        = apvts.getRawParameterValue (ParamIDs::filterResonance)->load();
        const float envAmt     = apvts.getRawParameterValue (ParamIDs::envFilterAmt)->load();

        // Block-rate LFO modulating cutoff.
        const float lfoDepth   = apvts.getRawParameterValue (ParamIDs::filterLfoDepth)->load();
        const int   lfoWaveIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::filterLfoWaveform)->load());
        filterLfo.setWaveform (static_cast<LfoWaveform> (lfoWaveIdx));
        filterLfo.setRate     (apvts.getRawParameterValue (ParamIDs::filterLfoRate)->load());
        filterLfo.setDepth    (1.0f);   // raw [-1..+1]; depth applied manually so UI can read unscaled signal
        filterLfo.setTempoSync (false);
        filterLfo.advance (buffer.getNumSamples() - 1);
        const float lfoRaw    = filterLfo.getNextSample();   // raw [-1..+1]
        filterLfoOutput.store (lfoRaw, std::memory_order_relaxed);
        const float lfoSample = lfoRaw * lfoDepth;           // depth-scaled [-depth..+depth]

        // Gate step, audio envelope, and LFO all modulate cutoff in octaves.
        const float afEnvAmt = apvts.getRawParameterValue (ParamIDs::afEnvFilterAmt)->load();
        const float cutoff = juce::jlimit (20.0f, 20000.0f,
            baseCutoff * std::pow (2.0f, envLevel * envAmt * 4.0f
                                       + audioEnvLevel * afEnvAmt * 4.0f
                                       + lfoSample * 4.0f));
        filterProc.setParameters (static_cast<FilterMode> (modeIdx), cutoff, res);
        filterProc.process (ctx);
    }

    if (apvts.getRawParameterValue (ParamIDs::distEnabled)->load() > 0.5f)
    {
        const int   modeIdx  = static_cast<int> (apvts.getRawParameterValue (ParamIDs::distMode)->load());
        const float baseDrive = apvts.getRawParameterValue (ParamIDs::distDrive)->load();
        const float envAmt   = apvts.getRawParameterValue (ParamIDs::envDistAmt)->load();
        const float afEnvAmt = apvts.getRawParameterValue (ParamIDs::afEnvDistAmt)->load();
        const float drive    = juce::jlimit (0.0f, 1.0f, baseDrive + envLevel * envAmt + audioEnvLevel * afEnvAmt);
        const float output   = apvts.getRawParameterValue (ParamIDs::distOutput)->load();
        const int   bits    = static_cast<int> (apvts.getRawParameterValue (ParamIDs::distBitDepth)->load()) + 2;  // choice index 0 = 2 bits
        distProc.setMode       (static_cast<DistMode> (modeIdx));
        distProc.setDrive      (drive);
        distProc.setOutputGain (output);
        distProc.setBitDepth   (bits);
        distProc.setBandDrives (
            apvts.getRawParameterValue (ParamIDs::distDriveLow) ->load(),
            apvts.getRawParameterValue (ParamIDs::distDriveMid) ->load(),
            apvts.getRawParameterValue (ParamIDs::distDriveHigh)->load());
        distProc.process (ctx);
    }

    if (apvts.getRawParameterValue (ParamIDs::flangerEnabled)->load() > 0.5f)
    {
        const float baseMix      = apvts.getRawParameterValue (ParamIDs::flangerMix)->load();
        const float envAmt       = apvts.getRawParameterValue (ParamIDs::envFlangerAmt)->load();
        const float mix          = juce::jlimit (0.0f, 1.0f, baseMix + envLevel * envAmt);
        const float baseFeedback = apvts.getRawParameterValue (ParamIDs::flangerFeedback)->load();
        const float afEnvAmt     = apvts.getRawParameterValue (ParamIDs::afEnvFlangerAmt)->load();
        const float feedback     = juce::jlimit (0.0f, 1.0f, baseFeedback + audioEnvLevel * afEnvAmt);
        flangerProc.setParameters (
            apvts.getRawParameterValue (ParamIDs::flangerRate)->load(),
            feedback,
            apvts.getRawParameterValue (ParamIDs::flangerDelayStart)->load(),
            apvts.getRawParameterValue (ParamIDs::flangerDelayEnd)->load(),
            mix);
        flangerProc.process (ctx);
    }

    if (apvts.getRawParameterValue (ParamIDs::phaserEnabled)->load() > 0.5f)
    {
        const float rate    = apvts.getRawParameterValue (ParamIDs::phaserRate)->load();
        const float depth   = apvts.getRawParameterValue (ParamIDs::phaserDepth)->load();
        const float baseFb   = apvts.getRawParameterValue (ParamIDs::phaserFeedback)->load();
        const float afEnvAmt = apvts.getRawParameterValue (ParamIDs::afEnvPhaserAmt)->load();
        const float fb       = juce::jlimit (-1.0f, 1.0f, baseFb + audioEnvLevel * afEnvAmt);
        const int   stIdx   = static_cast<int> (apvts.getRawParameterValue (ParamIDs::phaserStages)->load());
        const int   stages  = (stIdx + 1) * 2;   // 0→2, 1→4, 2→6, 3→8, 4→10, 5→12
        const float centre  = apvts.getRawParameterValue (ParamIDs::phaserCentreFreq)->load();
        const float baseMix = apvts.getRawParameterValue (ParamIDs::phaserMix)->load();
        const float phaserEnvAmt = apvts.getRawParameterValue (ParamIDs::envPhaserAmt)->load();
        const float mix     = juce::jlimit (0.0f, 1.0f, baseMix + envLevel * phaserEnvAmt);
        phaserProc.setParameters (rate, depth, fb, stages, centre, mix);
        phaserProc.process (ctx);

        // Transparent soft-clamp to tame peaks from extreme phaser settings.
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* samples = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = samples[i];
                if (x > 1.0f || x < -1.0f)
                    samples[i] = std::tanh (x);
            }
        }
    }

    if (apvts.getRawParameterValue (ParamIDs::reverbEnabled)->load() > 0.5f)
    {
        const float roomSize  = apvts.getRawParameterValue (ParamIDs::reverbRoomSize)->load();
        const float damping   = apvts.getRawParameterValue (ParamIDs::reverbDamping)->load();
        const float width     = apvts.getRawParameterValue (ParamIDs::reverbWidth)->load();
        const float mix       = apvts.getRawParameterValue (ParamIDs::reverbMix)->load();
        const bool  reverse   = apvts.getRawParameterValue (ParamIDs::reverbReverse)->load() > 0.5f;

        float revLenMs = apvts.getRawParameterValue (ParamIDs::reverbRevLen)->load();
        if (apvts.getRawParameterValue (ParamIDs::reverbRevSync)->load() > 0.5f)
        {
            const auto div = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::reverbRevDiv)->load()));
            // Convert division to ms: Hz = beatsPerSec / divBeats, period = 1/Hz.
            const float hz = LfoProcessor::syncDivisionToHz (getCurrentBpm(), div);
            revLenMs = (hz > 0.0f) ? (1000.0f / hz) : 500.0f;
        }

        reverbProc.setParameters (roomSize, damping, width, mix, reverse, revLenMs);
        reverbProc.process (ctx);
    }

    if (apvts.getRawParameterValue (ParamIDs::delayEnabled)->load() > 0.5f)
    {
        const bool  sync   = apvts.getRawParameterValue (ParamIDs::delayTempoSync)->load() > 0.5f;
        if (sync)
        {
            const double bpm   = getCurrentBpm();
            const auto   lDiv  = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::delayTimeLDiv)->load()));
            const auto   rDiv  = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::delayTimeRDiv)->load()));
            delayProc.setTempoSync (true, bpm, lDiv, rDiv);
        }
        else
        {
            const float tL = apvts.getRawParameterValue (ParamIDs::delayTimeLMs)->load();
            const float tR = apvts.getRawParameterValue (ParamIDs::delayTimeRMs)->load();
            delayProc.setDelayTimes (tL, tR);
        }

        delayProc.setFeedback (apvts.getRawParameterValue (ParamIDs::delayFeedback)->load() * 100.0f);
        {
            const float baseMix = apvts.getRawParameterValue (ParamIDs::delayMix)->load();
            const float delayEnvAmt = apvts.getRawParameterValue (ParamIDs::envDelayAmt)->load();
            delayProc.setMix (juce::jlimit (0.0f, 1.0f, baseMix + envLevel * delayEnvAmt));
        }

        const int   fbModeIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::delayFbFilterMode)->load());
        const float fbCutoff  = apvts.getRawParameterValue (ParamIDs::delayFbCutoff)->load();
        const float fbRes     = apvts.getRawParameterValue (ParamIDs::delayFbResonance)->load();
        delayProc.setFbFilterParams (static_cast<FilterMode> (fbModeIdx), fbCutoff, fbRes);

        const auto  fbLfoWave = static_cast<LfoWaveform> (
            static_cast<int> (apvts.getRawParameterValue (ParamIDs::delayFbLfoWave)->load()));
        const float fbLfoRate  = apvts.getRawParameterValue (ParamIDs::delayFbLfoRate)->load();
        const float fbLfoDepth = apvts.getRawParameterValue (ParamIDs::delayFbLfoDepth)->load();
        delayProc.setFbLfoParams (fbLfoWave, fbLfoRate, fbLfoDepth);

        // Mix LFO — resolve effective rate (free-run or tempo-synced).
        const auto  mixLfoWave  = static_cast<LfoWaveform> (
            static_cast<int> (apvts.getRawParameterValue (ParamIDs::delayMixLfoWave)->load()));
        const float mixLfoDepth = apvts.getRawParameterValue (ParamIDs::delayMixLfoDepth)->load();
        const bool  mixLfoSync  = apvts.getRawParameterValue (ParamIDs::delayMixLfoSync)->load() > 0.5f;
        float mixLfoRateHz;
        if (mixLfoSync)
        {
            const auto mixDiv = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::delayMixLfoDiv)->load()));
            mixLfoRateHz = LfoProcessor::syncDivisionToHz (getCurrentBpm(), mixDiv);
        }
        else
        {
            mixLfoRateHz = apvts.getRawParameterValue (ParamIDs::delayMixLfoRate)->load();
        }
        delayProc.setMixLfoParams (mixLfoWave, mixLfoRateHz, mixLfoDepth);

        // Lock the mix LFO phase to the DAW transport every block so it starts
        // in sync and never drifts.  Only active when sync is enabled and the
        // transport is running; free-run mode is left untouched.
        if (mixLfoSync && currentIsPlaying && currentPpqPosition >= 0.0)
            delayProc.syncMixLfoToTransport (currentPpqPosition, mixLfoRateHz, getCurrentBpm());

        const float duckAmount = apvts.getRawParameterValue (ParamIDs::delayDuck)->load();
        delayProc.setDuckAmount (duckAmount);

        delayProc.process (ctx);
    }

    // Kill-tail: always runs so prevKillTailState is never stale when delay
    // is toggled on.  No auto-reset here — the UI pulses 0→1→0 manually, and
    // DAW automation controls the value directly without plugin interference.
    {
        const bool killNow = apvts.getRawParameterValue (ParamIDs::delayKillTail)->load() > 0.5f;
        if (killNow && !prevKillTailState)
            delayProc.killTail();
        prevKillTailState = killNow;
    }

    // Sidechain Post-FX — volume modulation after the FX chain.
    {
        const int scMode = static_cast<int> (
            apvts.getRawParameterValue (ParamIDs::sidechainMode)->load());
        if (scMode == 2)
        {
            const auto scDiv = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::sidechainDivision)->load()));
            sidechainProc.process (buffer, getCurrentBpm(), scDiv,
                apvts.getRawParameterValue (ParamIDs::sidechainPreDelay)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainRise)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainCurve)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainFall)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainMix)->load());
        }
    }

    // Highcut (LP) — sits right before clip, after sidechain.
    if (apvts.getRawParameterValue (ParamIDs::highcutEnabled)->load() > 0.5f)
    {
        highcutProc.setParameters (true,
            apvts.getRawParameterValue (ParamIDs::highcutFreq)->load(),
            apvts.getRawParameterValue (ParamIDs::highcutAmt)->load());
        highcutProc.process (ctx);
    }

    // Lowcut (HP) — sits right after highcut, before clip.
    if (apvts.getRawParameterValue (ParamIDs::lowcutEnabled)->load() > 0.5f)
    {
        lowcutProc.setParameters (false,
            apvts.getRawParameterValue (ParamIDs::lowcutFreq)->load(),
            apvts.getRawParameterValue (ParamIDs::lowcutAmt)->load());
        lowcutProc.process (ctx);
    }

    // Feed the post-lowcut signal to the spectrum visualiser.
    lowcutSpectrumSource.pushSamples (buffer);

    // Clip — last in the chain, after sidechain/highcut/lowcut, to catch any final peaks.
    {
        const bool clipOn = apvts.getRawParameterValue (ParamIDs::clipEnabled)->load() > 0.5f;
        if (clipOn)
        {
            const int   modeIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::clipMode)->load());
            const float amount  = apvts.getRawParameterValue (ParamIDs::clipAmt)->load();

            // Capture pre-clip peaks and the effective ceiling for the UI overlay.
            // Hard clip: threshold lowers from 1.0→0.05 as amount increases.
            // Soft clip: dry/wet blend — ceiling is 1.0 at amount=0,
            //            trending toward 1/drive at amount=1.
            const int   n       = buffer.getNumSamples();
            const float softCeil = 1.0f / (1.0f + amount * 19.0f);
            const float ceiling  = (modeIdx == 0)
                ? (1.0f - 0.95f * amount)
                : (1.0f + amount * (softCeil - 1.0f));
            clipPrePeakL .store (buffer.getMagnitude (0, 0, n), std::memory_order_relaxed);
            clipPrePeakR .store (buffer.getMagnitude (1, 0, n), std::memory_order_relaxed);
            clipThreshold.store (ceiling,  std::memory_order_relaxed);
            clipModeState.store (modeIdx,  std::memory_order_relaxed);

            const float makeup = apvts.getRawParameterValue (ParamIDs::clipMakeup)->load();
            clipProc.setMode        (static_cast<ClipMode> (modeIdx));
            clipProc.setAmount      (amount);
            clipProc.setMakeupGain  (makeup);
            clipProc.process        (ctx);
        }
        else
        {
            clipModeState.store (-1, std::memory_order_relaxed);
        }
    }

    // Post-gain — applied as the final step in the processing chain.
    {
        const float targetGain = juce::Decibels::decibelsToGain (
            apvts.getRawParameterValue (ParamIDs::postGainDb)->load());
        postGainSmoothed.setTargetValue (targetGain);

        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float g = postGainSmoothed.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[s] *= g;
        }
    }

    // Post-FX VU measurement — captured after post-gain, reflecting final output.
    {
        const int n = buffer.getNumSamples();
        vuPostFxL.store (buffer.getMagnitude (0, 0, n), std::memory_order_relaxed);
        vuPostFxR.store (buffer.getMagnitude (1, 0, n), std::memory_order_relaxed);
    }
}

//==============================================================================
bool PluginProcessor::loadSampleFile (const juce::File& file)
{
    if (!file.existsAsFile())
    {
        DBG ("loadSampleFile: file does not exist: " + file.getFullPathName());
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        DBG ("loadSampleFile: cannot create reader for: " + file.getFullPathName());
        return false;
    }

    auto newSample = std::make_shared<SampleData>();
    newSample->sourceSampleRate = reader->sampleRate;
    newSample->buffer.setSize (
        static_cast<int> (reader->numChannels),
        static_cast<int> (reader->lengthInSamples));
    reader->read (&newSample->buffer, 0,
                  static_cast<int> (reader->lengthInSamples), 0, true, true);

    // Calculate peak normalization gain (1 / peak amplitude).
    {
        float peak = 0.0f;
        const int numCh      = newSample->buffer.getNumChannels();
        const int numSamples = newSample->buffer.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, newSample->buffer.getMagnitude (ch, 0, numSamples));
        newSample->peakNormalizeGain = peak > 0.0f ? 1.0f / peak : 1.0f;
    }

    normalizeGain.store (newSample->peakNormalizeGain, std::memory_order_relaxed);
    engine->setSampleData (std::move (newSample));
    lastLoadedFile = file;
    return true;
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Store the sample path as a property on the root node.
    // Using setProperty always overwrites any stale value from a previous save,
    // so there is no accumulation or ordering issue.
    if (lastLoadedFile.existsAsFile())
        state.setProperty ("sampleFilePath", lastLoadedFile.getFullPathName(), nullptr);
    else
        state.removeProperty ("sampleFilePath", nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (!state.isValid())
        return;

    // Extract the sample path BEFORE replaceState so listener callbacks
    // triggered by replaceState cannot affect the value we read.
    juce::String samplePath = state.getProperty ("sampleFilePath", "").toString();

    // Backward compatibility: projects saved by older versions used a child node.
    if (samplePath.isEmpty())
    {
        const auto fileNode = state.getChildWithName ("SampleFile");
        if (fileNode.isValid())
            samplePath = fileNode.getProperty ("path", "").toString();
    }

    apvts.replaceState (state);

    if (samplePath.isNotEmpty())
    {
        const juce::File f (samplePath);
        if (f.existsAsFile() && loadSampleFile (f))
        {
            sampleMissing     = false;
            missingSampleName = {};
        }
        else
        {
            // File missing or unreadable — flag for the UI to display a warning.
            sampleMissing     = true;
            missingSampleName = f.getFileName();
        }
    }
    else
    {
        sampleMissing     = false;
        missingSampleName = {};
    }
}

//==============================================================================
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

double PluginProcessor::getCurrentBpm() const
{
    if (auto* ph = getPlayHead())
    {
        if (auto posInfo = ph->getPosition())
            if (posInfo->getBpm().hasValue())
                return *posInfo->getBpm();
    }
    return 120.0;
}

void PluginProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Guard against recursive callbacks when we call setValueNotifyingHost below.
    if (enforcingMarkerConstraint)
        return;

    enforcingMarkerConstraint = true;

    static constexpr float kEpsilon = 0.005f;

    const float startVal = apvts.getRawParameterValue (ParamIDs::sampleStartPos)->load();
    const float endVal   = apvts.getRawParameterValue (ParamIDs::sampleEndPos)->load();

    if (parameterID == ParamIDs::sampleStartPos)
    {
        // If start was moved past end, clamp it back.
        if (newValue >= endVal - kEpsilon)
        {
            if (auto* p = apvts.getParameter (ParamIDs::sampleStartPos))
                p->setValueNotifyingHost (endVal - kEpsilon);
        }
    }
    else if (parameterID == ParamIDs::sampleEndPos)
    {
        // If end was moved before start, clamp it forward.
        if (newValue <= startVal + kEpsilon)
        {
            if (auto* p = apvts.getParameter (ParamIDs::sampleEndPos))
                p->setValueNotifyingHost (startVal + kEpsilon);
        }
    }

    enforcingMarkerConstraint = false;
}

void PluginProcessor::setupLfoNoteOnCallback()
{
    // Called from the audio thread on each note-on.
    // Resets the LFO phase only when tempo-sync is active (free-run continues uninterrupted).
    engine->onNoteOn = [this]()
    {
        const bool tempoSync = apvts.getRawParameterValue (ParamIDs::lfoTempoSync)->load() > 0.5f;
        if (tempoSync)
            lfo.resetPhase();
    };
}

void PluginProcessor::updateLfoAndPushOffset (int numSamples)
{
    // Read LFO parameters from APVTS (atomic, safe on audio thread).
    const bool  tempoSync = apvts.getRawParameterValue (ParamIDs::lfoTempoSync)->load() > 0.5f;
    const float depth     = apvts.getRawParameterValue (ParamIDs::lfoDepth)->load();
    const int   waveIdx   = static_cast<int> (apvts.getRawParameterValue (ParamIDs::lfoWaveform)->load());

    lfo.setWaveform (static_cast<LfoWaveform> (waveIdx));
    lfo.setDepth    (depth);

    if (tempoSync)
    {
        const int divIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::lfoSyncDiv)->load());
        lfo.setSyncedRate (getCurrentBpm(), static_cast<SyncDivision> (divIdx));
        lfo.setTempoSync (true);
    }
    else
    {
        const float rate = apvts.getRawParameterValue (ParamIDs::lfoRate)->load();
        lfo.setRate (rate);
        lfo.setTempoSync (false);
    }

    // Compute one representative LFO sample for this block (block-rate modulation).
    // Advance the LFO by numSamples-1 then read the final sample to keep phase accurate.
    lfo.advance (numSamples - 1);
    const float lfoSample = lfo.getNextSample();   // also advances by 1

    // The LFO output (-1..+1) is scaled by depth (already applied inside LfoProcessor)
    // and mapped to a start-point offset.  Maximum shift = 50% of the way to the end point.
    const float baseStart  = apvts.getRawParameterValue (ParamIDs::sampleStartPos)->load();
    const float endPos     = apvts.getRawParameterValue (ParamIDs::sampleEndPos)->load();
    const float windowSize = endPos - baseStart;

    // Remap depth-scaled lfoSample from [-depth, +depth] to [0, depth * windowSize].
    // The start point only ever moves FORWARD inside the [startPos, endPos] window,
    // so playback can never begin before the set sample start position.
    const float lfoOffset = (depth > 0.0f)
        ? (lfoSample + depth) * 0.5f * windowSize
        : 0.0f;

    engine->lfoStartOffset.store (lfoOffset, std::memory_order_relaxed);
}

void PluginProcessor::updateVelLfoAndPushOffset (int numSamples)
{
    const bool  tempoSync = apvts.getRawParameterValue (ParamIDs::velLfoTempoSync)->load() > 0.5f;
    const float depth     = apvts.getRawParameterValue (ParamIDs::velLfoDepth)->load();
    const int   waveIdx   = static_cast<int> (apvts.getRawParameterValue (ParamIDs::velLfoWaveform)->load());

    velLfo.setWaveform (static_cast<LfoWaveform> (waveIdx));
    velLfo.setDepth    (depth);

    if (tempoSync)
    {
        const int divIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::velLfoSyncDiv)->load());
        velLfo.setSyncedRate (getCurrentBpm(), static_cast<SyncDivision> (divIdx));
        velLfo.setTempoSync (true);
    }
    else
    {
        const float rate = apvts.getRawParameterValue (ParamIDs::velLfoRate)->load();
        velLfo.setRate (rate);
        velLfo.setTempoSync (false);
    }

    // Advance to end of block and read the final value.
    // The LFO output is already depth-scaled to [-depth, +depth]; store it directly
    // as a velocity offset — positive = louder, negative = quieter at note-on.
    velLfo.advance (numSamples - 1);
    const float lfoSample = velLfo.getNextSample();

    engine->velLfoOffset.store (lfoSample, std::memory_order_relaxed);

    engine->velProbability.store (
        apvts.getRawParameterValue (ParamIDs::velLfoProb)->load(),
        std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
