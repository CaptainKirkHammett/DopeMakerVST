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

    // Initialise drawn sidechain envelope to unity (fully open).
    std::fill (std::begin (sidechainDrawnEnvelope), std::end (sidechainDrawnEnvelope), 1.0f);
    sidechainProc.setDrawnEnvelope (sidechainDrawnEnvelope, kSidechainTableSize);

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
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::hqInterpolation, "HQ Interpolation", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::sampleDirection, "Sample Direction",
        juce::StringArray { "Sample FW", "Sample BW", "FW/BW Rnd" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::loopMode, "Loop Mode",
        juce::StringArray { "No Loop", "Forward", "Ping-Pong" }, 2));
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
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
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
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            ParamIDs::slicerRandomEnabled, "Slice1 Random Mode", false));
    }

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::lfoWaveform, "LFO Waveform",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" },
        5));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::lfoRate, "Sample Start LFO Rate",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::lfoDepth, "Sample Start LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::lfoTempoSync, "LFO Tempo Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::lfoSyncDiv, "LFO Sync Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
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
        5));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volRelLfoRate, "Vol Rel LFO Rate",
        juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volRelLfoDepth, "Vol Rel LFO Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 3); }));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::volRelLfoTempoSync, "Vol Rel LFO Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::volRelLfoSyncDiv, "Vol Rel LFO Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
        4));

    // --- Pitch LFO ---
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::pitchLfoWaveform, "Pitch LFO Wave",
        juce::StringArray { "Sine","Triangle","Square","Saw Up","Saw Down","Rnd Sine","Rnd Square" },
        5));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchLfoRate, "Pitch LFO Rate",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.3f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::pitchLfoDepth, "Pitch LFO Depth", 0.0f, 64.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::pitchLfoTempoSync, "Pitch LFO Sync", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::pitchLfoSyncDiv, "Pitch LFO Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
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
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::reverbDuck, "Reverb Duck",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
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
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
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
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
        1)); // default: 1/16
    for (int i = 0; i < GateController::kMaxSteps; ++i)
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            ParamIDs::gateStepId (i), "Gate Step " + juce::String (i + 1),
            0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::gateVolumeAmt, "Trance Gate Amount", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::gateRouting, "Trance Gate Routing",
        juce::StringArray { "Pre FX", "Pre Reverb", "Post FX" },
        0));
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
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
        4));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::delayTimeRDiv, "Delay R Div",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
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
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
        4));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::delayDuck, "Delay Duck",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    // --- Highcut (LP) ---
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::highcutMode, "Highcut Mode",
        juce::StringArray { "Off", "Pre", "Post" }, 0));
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
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::lowcutMode, "Lowcut Mode",
        juce::StringArray { "Off", "Pre", "Post" }, 0));
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

    // --- Resonance Suppressor (AutoEQ) ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::suppressorEnabled, "AutoEQ", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::suppressorAmt, "Suppressor Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::suppressorFFT, "AutoEQ FFT Mode", false));

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
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
        1));  // default: 1/16
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::arpProbability, "Arp Play Probability", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::arpLfoWaveform, "Arpeggiator LFO Waveform",
        juce::StringArray { "Sine","Tri","Sqr","SawU","SawD","Rnd~","Rnd|" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::arpLfoDivision, "Arp LFO Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
        4));  // default: 1/4
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::arpLfoDepth, "Arp Note Length LFO Depth", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::arpTie, "Arpeggiator Tie", 0.0f, 1.0f, 0.0f));

    // --- Sidechain volume modulator ---
    // Mode: 0 = Off, 1 = Pre-FX, 2 = Post-FX, 3 = FX Only
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::sidechainMode, "Sidechain Mode",
        juce::StringArray { "Sidechain Off", "Sidechain Pre", "Sidechain Post", "Sidechain FX Only" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::sidechainDivision, "Sidechain Division",
        juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1","1/64" },
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
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::sidechainDrawMode, "Sidechain Draw Mode", false));

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

    // Step volume gate — 5ms ramp to smooth step transitions.
    gateVolumeSmoothed.reset (sampleRate, 0.005);
    gateVolumeSmoothed.setCurrentAndTargetValue (1.0f);

    // FX enable crossfade smoothers — 5ms ramp for click-free toggling.
    constexpr double kEnableRampSec = 0.005;
    filterEnableSmoothed .reset (sampleRate, kEnableRampSec);
    distEnableSmoothed   .reset (sampleRate, kEnableRampSec);
    flangerEnableSmoothed.reset (sampleRate, kEnableRampSec);
    phaserEnableSmoothed .reset (sampleRate, kEnableRampSec);
    reverbEnableSmoothed .reset (sampleRate, kEnableRampSec);
    delayEnableSmoothed  .reset (sampleRate, kEnableRampSec);
    highcutPreSmoothed   .reset (sampleRate, kEnableRampSec);
    highcutPostSmoothed  .reset (sampleRate, kEnableRampSec);
    lowcutPreSmoothed    .reset (sampleRate, kEnableRampSec);
    lowcutPostSmoothed   .reset (sampleRate, kEnableRampSec);
    clipEnableSmoothed   .reset (sampleRate, kEnableRampSec);

    filterEnableSmoothed .setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::filterEnabled)->load()  > 0.5f ? 1.0f : 0.0f);
    distEnableSmoothed   .setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::distEnabled)->load()    > 0.5f ? 1.0f : 0.0f);
    flangerEnableSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::flangerEnabled)->load() > 0.5f ? 1.0f : 0.0f);
    phaserEnableSmoothed .setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::phaserEnabled)->load()  > 0.5f ? 1.0f : 0.0f);
    reverbEnableSmoothed .setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::reverbEnabled)->load()  > 0.5f ? 1.0f : 0.0f);
    delayEnableSmoothed  .setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::delayEnabled)->load()   > 0.5f ? 1.0f : 0.0f);
    {
        const int hcMode = (int) apvts.getRawParameterValue (ParamIDs::highcutMode)->load();
        highcutPreSmoothed .setCurrentAndTargetValue (hcMode == 1 ? 1.0f : 0.0f);
        highcutPostSmoothed.setCurrentAndTargetValue (hcMode == 2 ? 1.0f : 0.0f);
    }
    {
        const int lcMode = (int) apvts.getRawParameterValue (ParamIDs::lowcutMode)->load();
        lowcutPreSmoothed .setCurrentAndTargetValue (lcMode == 1 ? 1.0f : 0.0f);
        lowcutPostSmoothed.setCurrentAndTargetValue (lcMode == 2 ? 1.0f : 0.0f);
    }
    clipEnableSmoothed   .setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::clipEnabled)->load()    > 0.5f ? 1.0f : 0.0f);

    suppressorEnableSmoothed.reset (sampleRate, kEnableRampSec);
    suppressorEnableSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::suppressorEnabled)->load() > 0.5f ? 1.0f : 0.0f);

    // Shared scratch buffer for dry/wet crossfade during FX enable transitions.
    fxDryBuffer.setSize (2, samplesPerBlock);
    scPreFxBuffer.setSize (2, samplesPerBlock);

    stereoProc.prepare  (spec);
    limiterProc.prepare (spec);
    filterProc.prepare  (spec);
    distProc.prepare    (spec);
    flangerProc.prepare  (spec);
    phaserProc.prepare   (spec);
    reverbProc.prepare   (spec);
    delayProc.prepare    (spec);
    highcutProc.prepare  (spec);
    highcutPreProc.prepare (spec);
    lowcutProc.prepare   (spec);
    lowcutPreProc.prepare (spec);
    lowcutSpectrumSource.setSampleRate (sampleRate);
    clipProc.prepare       (spec);
    suppressorProc.prepare (spec);
    granulizerProc.prepare (sampleRate, samplesPerBlock);
    stutterProc.prepare    (sampleRate, samplesPerBlock);
    lfo.prepare            (sampleRate);
    velLfo.prepare         (sampleRate);
    filterLfo.prepare      (sampleRate);
    stutterLenLfo.prepare  (sampleRate);
    gateController.prepare   (spec);
    audioEnvFollower.prepare (spec);
    sidechainProc.prepare    (sampleRate);
    sidechainProc.setDrawnEnvelope (sidechainDrawnEnvelope, kSidechainTableSize);
    arpProc.prepare          (sampleRate);
}

void PluginProcessor::releaseResources() {}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // If a preset switch requested a DSP flush, do it here on the audio thread
    // before any new audio is processed — prevents spillover from the old preset.
    if (dspFlushRequested.exchange (false, std::memory_order_acquire))
    {
        flushAllDspState();
        buffer.clear();
    }

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
        vp.perChannelZC    = apvts.getRawParameterValue (ParamIDs::zcPerChannel)->load() > 0.5f;
        vp.hqInterpolation = apvts.getRawParameterValue (ParamIDs::hqInterpolation)->load() > 0.5f;
        {
            const int dirIdx = static_cast<int> (
                apvts.getRawParameterValue (ParamIDs::sampleDirection)->load());
            // 0 = FW, 1 = BW, 2 = random (resolved per-note in SamplerEngine::noteOn)
            vp.reversed    = (dirIdx == 1);
            vp.randomDir   = (dirIdx == 2);
        }
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

                if (playStarted)
                {
                    // Full reset on play start — no held notes carry over.
                    arpProc.reset();
                    engine->forceAllVoicesOff();
                }
                else
                {
                    // Transport loop/jump — reset timing but keep held notes
                    // so the arpeggiator continues with the sustained keys.
                    arpProc.resetTiming();
                }
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
    engine->slicerRandomEnabled.store (
        apvts.getRawParameterValue (ParamIDs::slicerRandomEnabled)->load() > 0.5f,
        std::memory_order_relaxed);

    // (Stutter is now an audio effect — processed later in the FX chain, before the filter.)

    // Render voices.
    engine->renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Lowcut/Highcut mode: 0=Off, 1=Pre, 2=Post.
    const int lowcutMode  = (int) apvts.getRawParameterValue (ParamIDs::lowcutMode)->load();
    const int highcutMode = (int) apvts.getRawParameterValue (ParamIDs::highcutMode)->load();

    // Highcut (LP) — Pre mode: applied right after engine render, before normalize.
    {
        highcutPreSmoothed.setTargetValue (highcutMode == 1 ? 1.0f : 0.0f);
        const bool smoothing = highcutPreSmoothed.isSmoothing();
        const bool active    = smoothing || highcutPreSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            highcutPreProc.setParameters (true,
                apvts.getRawParameterValue (ParamIDs::highcutFreq)->load(),
                apvts.getRawParameterValue (ParamIDs::highcutAmt)->load());

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            juce::dsp::AudioBlock<float> hcPreBlock (buffer);
            juce::dsp::ProcessContextReplacing<float> hcPreCtx (hcPreBlock);
            highcutPreProc.process (hcPreCtx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = highcutPreSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    // Lowcut (HP) — Pre mode: applied right after engine render, before normalize.
    {
        lowcutPreSmoothed.setTargetValue (lowcutMode == 1 ? 1.0f : 0.0f);
        const bool smoothing = lowcutPreSmoothed.isSmoothing();
        const bool active    = smoothing || lowcutPreSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            lowcutPreProc.setParameters (false,
                apvts.getRawParameterValue (ParamIDs::lowcutFreq)->load(),
                apvts.getRawParameterValue (ParamIDs::lowcutAmt)->load());

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            juce::dsp::AudioBlock<float> preBlock (buffer);
            juce::dsp::ProcessContextReplacing<float> preCtx (preBlock);
            lowcutPreProc.process (preCtx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = lowcutPreSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

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
    const bool scDrawMode = apvts.getRawParameterValue (ParamIDs::sidechainDrawMode)->load() > 0.5f;

    {
        const int scMode = static_cast<int> (
            apvts.getRawParameterValue (ParamIDs::sidechainMode)->load());

        // FX Only mode: snapshot the dry signal before the FX chain.
        if (scMode == 3)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                scPreFxBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
        }

        if (scMode == 1)
        {
            const auto scDiv = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::sidechainDivision)->load()));
            sidechainProc.process (buffer, getCurrentBpm(), scDiv,
                apvts.getRawParameterValue (ParamIDs::sidechainPreDelay)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainRise)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainCurve)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainFall)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainMix)->load(),
                currentPpqPosition, scDrawMode);
        }
    }

    // FX chain: Volume Gate → Stutter → Filter → Distortion → Flanger → Phaser → Reverb → Ping-Pong Delay.
    // Each FX skipped entirely if disabled — no CPU waste.
    // Volume gate routing: 0 = Pre FX, 1 = Pre Reverb, 2 = Post FX.

    const int gateRoutingIdx = static_cast<int> (
        apvts.getRawParameterValue (ParamIDs::gateRouting)->load());

    // Step volume gate — per-sample smoothed to prevent clicks at step transitions.
    auto applyGateVolume = [&]()
    {
        const bool gateOn = apvts.getRawParameterValue (ParamIDs::gateEnabled)->load() > 0.5f;
        const float gateAmt = gateOn
            ? apvts.getRawParameterValue (ParamIDs::gateVolumeAmt)->load()
            : 0.0f;

        const float targetGain = 1.0f - gateAmt * (1.0f - envLevel);
        gateVolumeSmoothed.setTargetValue (targetGain);

        if (gateVolumeSmoothed.isSmoothing() || targetGain < 1.0f)
        {
            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                const float g = gateVolumeSmoothed.getNextValue();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.getWritePointer (ch)[s] *= g;
            }
        }
    };

    if (gateRoutingIdx == 0)
        applyGateVolume();

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

    {
        filterEnableSmoothed.setTargetValue (
            apvts.getRawParameterValue (ParamIDs::filterEnabled)->load() > 0.5f ? 1.0f : 0.0f);
        const bool smoothing = filterEnableSmoothed.isSmoothing();
        const bool active    = smoothing || filterEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
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
                baseCutoff * std::pow (2.0f, envLevel * envAmt * 6.0f
                                           + audioEnvLevel * afEnvAmt * 6.0f
                                           + lfoSample * 4.0f));
            filterProc.setParameters (static_cast<FilterMode> (modeIdx), cutoff, res);

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            filterProc.process (ctx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = filterEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    {
        distEnableSmoothed.setTargetValue (
            apvts.getRawParameterValue (ParamIDs::distEnabled)->load() > 0.5f ? 1.0f : 0.0f);
        const bool smoothing = distEnableSmoothed.isSmoothing();
        const bool active    = smoothing || distEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            const int   modeIdx  = static_cast<int> (apvts.getRawParameterValue (ParamIDs::distMode)->load());
            const float baseDrive = apvts.getRawParameterValue (ParamIDs::distDrive)->load();
            const float envAmt   = apvts.getRawParameterValue (ParamIDs::envDistAmt)->load();
            const float afEnvAmt = apvts.getRawParameterValue (ParamIDs::afEnvDistAmt)->load();
            const float drive    = juce::jlimit (0.0f, 1.0f, baseDrive + envLevel * envAmt * 1.5f + audioEnvLevel * afEnvAmt * 1.5f);
            const float output   = apvts.getRawParameterValue (ParamIDs::distOutput)->load();
            const int   bits    = static_cast<int> (apvts.getRawParameterValue (ParamIDs::distBitDepth)->load()) + 2;
            distProc.setMode       (static_cast<DistMode> (modeIdx));
            distProc.setDrive      (drive);
            distProc.setOutputGain (output);
            distProc.setBitDepth   (bits);
            distProc.setBandDrives (
                apvts.getRawParameterValue (ParamIDs::distDriveLow) ->load(),
                apvts.getRawParameterValue (ParamIDs::distDriveMid) ->load(),
                apvts.getRawParameterValue (ParamIDs::distDriveHigh)->load());

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            distProc.process (ctx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = distEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    {
        flangerEnableSmoothed.setTargetValue (
            apvts.getRawParameterValue (ParamIDs::flangerEnabled)->load() > 0.5f ? 1.0f : 0.0f);
        const bool smoothing = flangerEnableSmoothed.isSmoothing();
        const bool active    = smoothing || flangerEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            const float baseMix      = apvts.getRawParameterValue (ParamIDs::flangerMix)->load();
            const float envAmt       = apvts.getRawParameterValue (ParamIDs::envFlangerAmt)->load();
            const float mix          = juce::jlimit (0.0f, 1.0f, baseMix + envLevel * envAmt * 1.5f);
            const float baseFeedback = apvts.getRawParameterValue (ParamIDs::flangerFeedback)->load();
            const float afEnvAmt     = apvts.getRawParameterValue (ParamIDs::afEnvFlangerAmt)->load();
            const float feedback     = juce::jlimit (0.0f, 1.0f, baseFeedback + audioEnvLevel * afEnvAmt * 1.5f);
            flangerProc.setParameters (
                apvts.getRawParameterValue (ParamIDs::flangerRate)->load(),
                feedback,
                apvts.getRawParameterValue (ParamIDs::flangerDelayStart)->load(),
                apvts.getRawParameterValue (ParamIDs::flangerDelayEnd)->load(),
                mix);

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            flangerProc.process (ctx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = flangerEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    {
        phaserEnableSmoothed.setTargetValue (
            apvts.getRawParameterValue (ParamIDs::phaserEnabled)->load() > 0.5f ? 1.0f : 0.0f);
        const bool smoothing = phaserEnableSmoothed.isSmoothing();
        const bool active    = smoothing || phaserEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            const float rate    = apvts.getRawParameterValue (ParamIDs::phaserRate)->load();
            const float depth   = apvts.getRawParameterValue (ParamIDs::phaserDepth)->load();
            const float baseFb   = apvts.getRawParameterValue (ParamIDs::phaserFeedback)->load();
            const float afEnvAmt = apvts.getRawParameterValue (ParamIDs::afEnvPhaserAmt)->load();
            const float fb       = juce::jlimit (-1.0f, 1.0f, baseFb + audioEnvLevel * afEnvAmt * 1.5f);
            const int   stIdx   = static_cast<int> (apvts.getRawParameterValue (ParamIDs::phaserStages)->load());
            const int   stages  = (stIdx + 1) * 2;
            const float centre  = apvts.getRawParameterValue (ParamIDs::phaserCentreFreq)->load();
            const float baseMix = apvts.getRawParameterValue (ParamIDs::phaserMix)->load();
            const float phaserEnvAmt = apvts.getRawParameterValue (ParamIDs::envPhaserAmt)->load();
            const float mix     = juce::jlimit (0.0f, 1.0f, baseMix + envLevel * phaserEnvAmt * 1.5f);
            phaserProc.setParameters (rate, depth, fb, stages, centre, mix);

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

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

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = phaserEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    if (gateRoutingIdx == 1)
        applyGateVolume();

    {
        reverbEnableSmoothed.setTargetValue (
            apvts.getRawParameterValue (ParamIDs::reverbEnabled)->load() > 0.5f ? 1.0f : 0.0f);
        const bool smoothing = reverbEnableSmoothed.isSmoothing();
        const bool active    = smoothing || reverbEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            const float roomSize   = apvts.getRawParameterValue (ParamIDs::reverbRoomSize)->load();
            const float damping    = apvts.getRawParameterValue (ParamIDs::reverbDamping)->load();
            const float width      = apvts.getRawParameterValue (ParamIDs::reverbWidth)->load();
            const float mix        = apvts.getRawParameterValue (ParamIDs::reverbMix)->load();
            const float reverbDuck = apvts.getRawParameterValue (ParamIDs::reverbDuck)->load();
            const bool  reverse    = apvts.getRawParameterValue (ParamIDs::reverbReverse)->load() > 0.5f;

            reverbProc.setDuckAmount (reverbDuck);

            float revLenMs = apvts.getRawParameterValue (ParamIDs::reverbRevLen)->load();
            if (apvts.getRawParameterValue (ParamIDs::reverbRevSync)->load() > 0.5f)
            {
                const auto div = static_cast<SyncDivision> (
                    static_cast<int> (apvts.getRawParameterValue (ParamIDs::reverbRevDiv)->load()));
                const float hz = LfoProcessor::syncDivisionToHz (getCurrentBpm(), div);
                revLenMs = (hz > 0.0f) ? (1000.0f / hz) : 500.0f;
            }

            reverbProc.setParameters (roomSize, damping, width, mix, reverse, revLenMs);

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            reverbProc.process (ctx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = reverbEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    {
        delayEnableSmoothed.setTargetValue (
            apvts.getRawParameterValue (ParamIDs::delayEnabled)->load() > 0.5f ? 1.0f : 0.0f);
        const bool smoothing = delayEnableSmoothed.isSmoothing();
        const bool active    = smoothing || delayEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
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
                delayProc.setMix (juce::jlimit (0.0f, 1.0f, baseMix + envLevel * delayEnvAmt * 1.5f));
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

            if (mixLfoSync && currentIsPlaying && currentPpqPosition >= 0.0)
                delayProc.syncMixLfoToTransport (currentPpqPosition, mixLfoRateHz, getCurrentBpm());

            const float duckAmount = apvts.getRawParameterValue (ParamIDs::delayDuck)->load();
            delayProc.setDuckAmount (duckAmount);

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            delayProc.process (ctx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = delayEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    if (gateRoutingIdx == 2)
        applyGateVolume();

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
                apvts.getRawParameterValue (ParamIDs::sidechainMix)->load(),
                currentPpqPosition, scDrawMode);
        }

        // Sidechain FX Only — apply envelope to the wet FX signal only;
        // the dry sound passes through untouched.  Mix knob acts as Vol
        // (volume of the sidechained FX return).
        if (scMode == 3)
        {
            const int n  = buffer.getNumSamples();
            const int nC = buffer.getNumChannels();
            const float vol = apvts.getRawParameterValue (ParamIDs::sidechainMix)->load();

            // Step 1: isolate the wet signal in buffer (subtract dry).
            for (int ch = 0; ch < nC; ++ch)
            {
                float*       out = buffer.getWritePointer (ch);
                const float* dry = scPreFxBuffer.getReadPointer (ch);
                for (int s = 0; s < n; ++s)
                    out[s] -= dry[s];
            }

            // Step 2: apply sidechain envelope to the wet-only signal (full depth).
            const auto scDiv = static_cast<SyncDivision> (
                static_cast<int> (apvts.getRawParameterValue (ParamIDs::sidechainDivision)->load()));
            sidechainProc.process (buffer, getCurrentBpm(), scDiv,
                apvts.getRawParameterValue (ParamIDs::sidechainPreDelay)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainRise)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainCurve)->load(),
                apvts.getRawParameterValue (ParamIDs::sidechainFall)->load(),
                1.0f,   // full sidechain depth on wet signal
                currentPpqPosition, scDrawMode);

            // Step 3: recombine — dry untouched + sidechained wet scaled by Vol.
            for (int ch = 0; ch < nC; ++ch)
            {
                float*       out = buffer.getWritePointer (ch);
                const float* dry = scPreFxBuffer.getReadPointer (ch);
                for (int s = 0; s < n; ++s)
                    out[s] = dry[s] + out[s] * vol;
            }
        }
    }

    // Highcut (LP) — Post mode: sits right before clip, after sidechain.
    {
        highcutPostSmoothed.setTargetValue (highcutMode == 2 ? 1.0f : 0.0f);
        const bool smoothing = highcutPostSmoothed.isSmoothing();
        const bool active    = smoothing || highcutPostSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            highcutProc.setParameters (true,
                apvts.getRawParameterValue (ParamIDs::highcutFreq)->load(),
                apvts.getRawParameterValue (ParamIDs::highcutAmt)->load());

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            highcutProc.process (ctx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = highcutPostSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    // Lowcut (HP) — Post mode: sits right after highcut, before clip.
    {
        lowcutPostSmoothed.setTargetValue (lowcutMode == 2 ? 1.0f : 0.0f);
        const bool smoothing = lowcutPostSmoothed.isSmoothing();
        const bool active    = smoothing || lowcutPostSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            lowcutProc.setParameters (false,
                apvts.getRawParameterValue (ParamIDs::lowcutFreq)->load(),
                apvts.getRawParameterValue (ParamIDs::lowcutAmt)->load());

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            lowcutProc.process (ctx);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = lowcutPostSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
    }

    // Feed the post-lowcut signal to the spectrum visualiser.
    lowcutSpectrumSource.pushSamples (buffer);

    // Clip — last in the chain, after sidechain/highcut/lowcut, to catch any final peaks.
    {
        const bool clipOn = apvts.getRawParameterValue (ParamIDs::clipEnabled)->load() > 0.5f;
        clipEnableSmoothed.setTargetValue (clipOn ? 1.0f : 0.0f);
        const bool smoothing = clipEnableSmoothed.isSmoothing();
        const bool active    = smoothing || clipEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            const int   modeIdx = static_cast<int> (apvts.getRawParameterValue (ParamIDs::clipMode)->load());
            const float amount  = apvts.getRawParameterValue (ParamIDs::clipAmt)->load();

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

            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            clipProc.process        (ctx);

            if (smoothing)
            {
                for (int s = 0; s < n; ++s)
                {
                    const float wet = clipEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
        }
        else
        {
            clipModeState.store (-1, std::memory_order_relaxed);
        }
    }

    // Resonance suppressor — last in the FX chain, after clip.
    {
        suppressorEnableSmoothed.setTargetValue (
            apvts.getRawParameterValue (ParamIDs::suppressorEnabled)->load() > 0.5f ? 1.0f : 0.0f);
        const float suppressAmt = apvts.getRawParameterValue (ParamIDs::suppressorAmt)->load();
        const bool  suppressFFT = apvts.getRawParameterValue (ParamIDs::suppressorFFT)->load() > 0.5f;

        // Report latency to DAW so it can compensate for FFT mode's delay.
        const bool  suppressOn  = suppressorEnableSmoothed.getCurrentValue() > 0.5f;
        const int   latency     = (suppressOn && suppressFFT)
                                  ? suppressorProc.getLatencySamples (true) : 0;
        if (getLatencySamples() != latency)
            setLatencySamples (latency);
        const bool smoothing = suppressorEnableSmoothed.isSmoothing();
        const bool active    = smoothing || suppressorEnableSmoothed.getCurrentValue() > 0.5f;

        if (active)
        {
            if (smoothing)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    fxDryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            suppressorProc.process (buffer, suppressAmt, suppressFFT);

            if (smoothing)
            {
                for (int s = 0; s < buffer.getNumSamples(); ++s)
                {
                    const float wet = suppressorEnableSmoothed.getNextValue();
                    const float dry = 1.0f - wet;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[s] = buffer.getReadPointer (ch)[s] * wet
                                                       + fxDryBuffer.getReadPointer (ch)[s] * dry;
                }
            }
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

    // Calculate peak normalization gain with -3 dB headroom so downstream
    // FX have room to add energy without clipping.
    {
        constexpr float kHeadroomDb   = -3.0f;
        const     float kHeadroomGain = juce::Decibels::decibelsToGain (kHeadroomDb);

        float peak = 0.0f;
        const int numCh      = newSample->buffer.getNumChannels();
        const int numSamples = newSample->buffer.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, newSample->buffer.getMagnitude (ch, 0, numSamples));
        newSample->peakNormalizeGain = peak > 0.0f ? (kHeadroomGain / peak) : 1.0f;
    }

    normalizeGain.store (newSample->peakNormalizeGain, std::memory_order_relaxed);
    engine->forceAllVoicesOff();
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

    // Store the active bank and preset names so the UI can display them after DAW recall.
    const auto& bName = presetManager.getCurrentBankName();
    if (bName.isNotEmpty())
        state.setProperty ("bankName", bName, nullptr);
    else
        state.removeProperty ("bankName", nullptr);

    const auto& pName = presetManager.getCurrentPresetName();
    if (pName.isNotEmpty())
        state.setProperty ("presetName", pName, nullptr);
    else
        state.removeProperty ("presetName", nullptr);

    // Serialize drawn sidechain envelope as a comma-separated string.
    {
        juce::String envStr;
        for (int i = 0; i < kSidechainTableSize; ++i)
        {
            if (i > 0) envStr += ",";
            envStr += juce::String (sidechainDrawnEnvelope[i], 4);
        }
        state.setProperty ("sidechainDrawnEnvelope", envStr, nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::flushAllDspState()
{
    // Kill all active voices so no old sample audio lingers.
    engine->forceAllVoicesOff();

    // Reset every DSP processor to clear internal buffers and state.
    filterProc.reset();
    distProc.reset();
    flangerProc.reset();
    phaserProc.reset();
    reverbProc.reset();
    delayProc.reset();
    highcutProc.reset();
    highcutPreProc.reset();
    lowcutProc.reset();
    lowcutPreProc.reset();
    clipProc.reset();
    stereoProc.reset();
    limiterProc.reset();
    suppressorProc.reset();
    sidechainProc.reset();
    gateController.reset();
    arpProc.reset();

    // Re-prepare stutter so its capture buffer is zeroed out.
    stutterProc.prepare (getSampleRate(), getBlockSize());

    // Re-prepare granulizer so its delay buffer is cleared.
    granulizerProc.prepare (getSampleRate(), getBlockSize());

    // Reset LFOs.
    lfo.resetPhase();
    velLfo.resetPhase();
    filterLfo.resetPhase();
    stutterLenLfo.resetPhase();

    // Snap all smoothed values so old ramps don't bleed through.
    gateVolumeSmoothed.setCurrentAndTargetValue (1.0f);

    // Snap FX enable crossfades to their new target values instantly — prevents
    // the old preset's FX tails from playing during a slow enable/disable ramp.
    auto snapEnable = [&] (juce::SmoothedValue<float>& sv, const juce::String& paramId) {
        const float target = apvts.getRawParameterValue (paramId)->load() > 0.5f ? 1.0f : 0.0f;
        sv.setCurrentAndTargetValue (target);
    };
    snapEnable (filterEnableSmoothed,      ParamIDs::filterEnabled);
    snapEnable (distEnableSmoothed,        ParamIDs::distEnabled);
    snapEnable (flangerEnableSmoothed,     ParamIDs::flangerEnabled);
    snapEnable (phaserEnableSmoothed,      ParamIDs::phaserEnabled);
    snapEnable (reverbEnableSmoothed,      ParamIDs::reverbEnabled);
    snapEnable (delayEnableSmoothed,       ParamIDs::delayEnabled);
    {
        const int hcMode = (int) apvts.getRawParameterValue (ParamIDs::highcutMode)->load();
        highcutPreSmoothed .setCurrentAndTargetValue (hcMode == 1 ? 1.0f : 0.0f);
        highcutPostSmoothed.setCurrentAndTargetValue (hcMode == 2 ? 1.0f : 0.0f);
    }
    {
        const int lcMode = (int) apvts.getRawParameterValue (ParamIDs::lowcutMode)->load();
        lowcutPreSmoothed .setCurrentAndTargetValue (lcMode == 1 ? 1.0f : 0.0f);
        lowcutPostSmoothed.setCurrentAndTargetValue (lcMode == 2 ? 1.0f : 0.0f);
    }
    snapEnable (clipEnableSmoothed,        ParamIDs::clipEnabled);
    snapEnable (suppressorEnableSmoothed,  ParamIDs::suppressorEnabled);

    // Snap gain ramps so the old preset's gain doesn't fade into the new one.
    gainSmoothed.setCurrentAndTargetValue (gainSmoothed.getTargetValue());
    preGainSmoothed.setCurrentAndTargetValue (preGainSmoothed.getTargetValue());
    postGainSmoothed.setCurrentAndTargetValue (postGainSmoothed.getTargetValue());
    normalizeSmoothed.setCurrentAndTargetValue (normalizeSmoothed.getTargetValue());
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

    // Restore the bank and preset names so the editor can display them after DAW recall.
    const auto restoredBank = state.getProperty ("bankName", "").toString();
    if (restoredBank.isNotEmpty())
        presetManager.setCurrentBankName (restoredBank);

    presetManager.setCurrentPresetName (
        state.getProperty ("presetName", "").toString());

    apvts.replaceState (state);

    // Restore drawn sidechain envelope.
    {
        const auto envStr = state.getProperty ("sidechainDrawnEnvelope", "").toString();
        if (envStr.isNotEmpty())
        {
            juce::StringArray tokens;
            tokens.addTokens (envStr, ",", "");
            const int count = juce::jmin (tokens.size(), kSidechainTableSize);
            for (int i = 0; i < count; ++i)
                sidechainDrawnEnvelope[i] = tokens[i].getFloatValue();
        }
    }

    // Request a DSP flush so no audio from the previous preset spills over.
    // The actual flush runs on the audio thread at the top of processBlock().
    dspFlushRequested.store (true, std::memory_order_release);

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
