#pragma once

#include <string>
#include <cstdio>

// All APVTS parameter IDs for SuomiSampler.
// Declared here so PluginProcessor, SamplerEngine, and UI panels share the same strings.

namespace ParamIDs
{
    // --- Sample Engine ---
    inline constexpr const char* zcPerChannel    = "zcPerChannel";    // Zero-crossing: per-channel (true) or shared (false)
    inline constexpr const char* gainDb          = "gainDb";          // Pre-FX output gain (dB)
    inline constexpr const char* limiterAmount   = "limiterAmount";   // Pre-FX limiter (0=off, 1=max)
    inline constexpr const char* stereoWidth     = "stereoWidth";     // Stereo width (-1=mono, 0=off, 1=wide)
    inline constexpr const char* sampleStartPos  = "sampleStartPos";
    inline constexpr const char* sampleEndPos    = "sampleEndPos";
    // sampleDirection: 0=FW, 1=BW (replaces the old sampleReverse bool)
    inline constexpr const char* sampleDirection = "sampleDirection";
    // loopMode: 0=No Loop, 1=Forward, 2=Ping-Pong (replaces loopEnabled + loopMode pair)
    inline constexpr const char* loopMode        = "loopMode";
    inline constexpr const char* rootNote        = "rootNote";
    inline constexpr const char* semitoneOffset  = "semitoneOffset";
    inline constexpr const char* pitchBendRange  = "pitchBendRange";
    // voiceCount: index 0=1 voice (mono), index N=N+1 voices (replaces polyMode + voiceCount pair)
    inline constexpr const char* voiceCount      = "voiceCount";
    // portamento > 0 enables legato mode (envelope/sample not restarted in mono)
    inline constexpr const char* portamento      = "portamento";      // float ms — pitch glide time (0 = instant / legato off)

    // --- Velocity LFO ---
    inline constexpr const char* velLfoProb      = "velLfoProb";
    inline constexpr const char* velLfoWaveform  = "velLfoWaveform";
    inline constexpr const char* velLfoRate      = "velLfoRate";
    inline constexpr const char* velLfoDepth     = "velLfoDepth";
    inline constexpr const char* velLfoTempoSync = "velLfoTempoSync";
    inline constexpr const char* velLfoSyncDiv   = "velLfoSyncDiv";

    // --- Sample Start LFO ---
    // When true, step-controller amplitude drives sample start instead of the LFO.
    inline constexpr const char* lfoStartStepOverride = "lfoStartStepOverride";
    // When true, each MIDI note maps to one evenly-spaced slice of the [start,end] window.
    // Mutually exclusive with lfoStartStepOverride.
    inline constexpr const char* slicerEnabled   = "slicerEnabled";
    inline constexpr const char* slicerLoNote    = "slicerLoNote";   // AudioParameterChoice, index = MIDI note (Slice Start key)
    inline constexpr const char* sliceCount      = "sliceCount";      // AudioParameterChoice, index+1 = number of slices (1–128)
    inline constexpr const char* lfoWaveform     = "lfoWaveform";
    inline constexpr const char* lfoRate         = "lfoRate";
    inline constexpr const char* lfoDepth        = "lfoDepth";
    inline constexpr const char* lfoTempoSync    = "lfoTempoSync";
    inline constexpr const char* lfoSyncDiv      = "lfoSyncDiv";

    // --- Volume Envelope ---
    inline constexpr const char* volAttack       = "volAttack";
    inline constexpr const char* volDecay        = "volDecay";
    inline constexpr const char* volSustain      = "volSustain";
    inline constexpr const char* volRelease      = "volRelease";

    // --- Volume Release LFO ---
    inline constexpr const char* volRelLfoWaveform = "volRelLfoWaveform";
    inline constexpr const char* volRelLfoRate     = "volRelLfoRate";
    inline constexpr const char* volRelLfoDepth    = "volRelLfoDepth";
    inline constexpr const char* volRelLfoTempoSync = "volRelLfoTempoSync";
    inline constexpr const char* volRelLfoSyncDiv   = "volRelLfoSyncDiv";

    // --- Pitch LFO ---
    inline constexpr const char* pitchLfoWaveform  = "pitchLfoWaveform";
    inline constexpr const char* pitchLfoRate      = "pitchLfoRate";
    inline constexpr const char* pitchLfoDepth     = "pitchLfoDepth";
    inline constexpr const char* pitchLfoTempoSync = "pitchLfoTempoSync";
    inline constexpr const char* pitchLfoSyncDiv   = "pitchLfoSyncDiv";

    // --- Pitch Envelope ---
    inline constexpr const char* pitchAttack     = "pitchAttack";
    inline constexpr const char* pitchDecay      = "pitchDecay";
    inline constexpr const char* pitchSustain    = "pitchSustain";
    inline constexpr const char* pitchRelease    = "pitchRelease";
    inline constexpr const char* pitchEnvDepth   = "pitchEnvDepth";

    // --- Filter ---
    inline constexpr const char* filterEnabled   = "filterEnabled";
    inline constexpr const char* filterMode      = "filterMode";
    inline constexpr const char* filterCutoff    = "filterCutoff";
    inline constexpr const char* filterResonance = "filterResonance";
    inline constexpr const char* filterLfoWaveform = "filterLfoWaveform";
    inline constexpr const char* filterLfoRate    = "filterLfoRate";
    inline constexpr const char* filterLfoDepth   = "filterLfoDepth";

    // --- Distortion ---
    inline constexpr const char* distEnabled     = "distEnabled";
    inline constexpr const char* distMode        = "distMode";
    inline constexpr const char* distDrive       = "distDrive";
    inline constexpr const char* distOutput      = "distOutput";
    inline constexpr const char* distBitDepth    = "distBitDepth";
    inline constexpr const char* distDriveLow    = "distDriveLow";
    inline constexpr const char* distDriveMid    = "distDriveMid";
    inline constexpr const char* distDriveHigh   = "distDriveHigh";

    // --- Flanger ---
    inline constexpr const char* flangerEnabled    = "flangerEnabled";
    inline constexpr const char* flangerRate       = "flangerRate";
    inline constexpr const char* flangerFeedback   = "flangerFeedback";
    inline constexpr const char* flangerDelayStart = "flangerDelayStart";
    inline constexpr const char* flangerDelayEnd   = "flangerDelayEnd";
    inline constexpr const char* flangerMix        = "flangerMix";

    // --- Phaser ---
    inline constexpr const char* phaserEnabled   = "phaserEnabled";
    inline constexpr const char* phaserRate      = "phaserRate";
    inline constexpr const char* phaserDepth     = "phaserDepth";
    inline constexpr const char* phaserFeedback  = "phaserFeedback";
    inline constexpr const char* phaserStages    = "phaserStages";
    inline constexpr const char* phaserCentreFreq = "phaserCentreFreq";
    inline constexpr const char* phaserMix          = "phaserMix";

    // --- Gate Controller ---
    inline constexpr const char* gateEnabled    = "gateEnabled";
    inline constexpr const char* gateStepCount  = "gateStepCount";
    inline constexpr const char* gateDivision   = "gateDivision";
    // Per-step amplitude IDs: gateStep01 … gateStep32 (1-indexed, zero-padded).
    inline std::string gateStepId (int zeroBasedIndex)
    {
        char buf[16];
        std::snprintf (buf, sizeof (buf), "gateStep%02d", zeroBasedIndex + 1);
        return std::string (buf);
    }
    inline constexpr const char* envFilterAmt       = "envFilterAmt";
    inline constexpr const char* envDistAmt         = "envDistAmt";
    inline constexpr const char* envFlangerAmt      = "envFlangerAmt";
    inline constexpr const char* envPhaserAmt       = "envPhaserAmt";
    inline constexpr const char* envDelayAmt        = "envDelayAmt";

    // --- Ping-Pong Delay ---
    inline constexpr const char* delayEnabled    = "delayEnabled";
    // Momentary trigger — fires killTail() on the audio thread, then auto-resets to false.
    inline constexpr const char* delayKillTail   = "delayKillTail";
    inline constexpr const char* delayTimeLMs    = "delayTimeLMs";
    inline constexpr const char* delayTimeRMs    = "delayTimeRMs";
    inline constexpr const char* delayTempoSync  = "delayTempoSync";
    inline constexpr const char* delayTimeLDiv   = "delayTimeLDiv";
    inline constexpr const char* delayTimeRDiv   = "delayTimeRDiv";
    inline constexpr const char* delayFeedback   = "delayFeedback";
    inline constexpr const char* delayMix        = "delayMix";
    inline constexpr const char* delayFbFilterMode  = "delayFbFilterMode";
    inline constexpr const char* delayFbCutoff   = "delayFbCutoff";
    inline constexpr const char* delayFbResonance = "delayFbResonance";
    inline constexpr const char* delayFbLfoWave  = "delayFbLfoWave";
    inline constexpr const char* delayFbLfoRate  = "delayFbLfoRate";
    inline constexpr const char* delayFbLfoDepth = "delayFbLfoDepth";

    // Mix LFO (modulates delay dry/wet mix).
    inline constexpr const char* delayMixLfoWave  = "delayMixLfoWave";
    inline constexpr const char* delayMixLfoRate  = "delayMixLfoRate";
    inline constexpr const char* delayMixLfoDepth = "delayMixLfoDepth";
    inline constexpr const char* delayMixLfoSync  = "delayMixLfoSync";
    inline constexpr const char* delayMixLfoDiv   = "delayMixLfoDiv";

    inline constexpr const char* delayDuck = "delayDuck";

    // --- Audio Envelope Follower ---
    inline constexpr const char* envFollowerEnabled = "envFollowerEnabled";
    inline constexpr const char* envFollowerAttack  = "envFollowerAttack";   // ms
    inline constexpr const char* envFollowerRelease = "envFollowerRelease";  // ms
    // Per-FX modulation amounts driven by the audio envelope follower:
    inline constexpr const char* afEnvFilterAmt  = "afEnvFilterAmt";   // → filter cutoff (octaves)
    inline constexpr const char* afEnvDistAmt    = "afEnvDistAmt";     // → dist drive
    inline constexpr const char* afEnvFlangerAmt = "afEnvFlangerAmt";  // → flanger feedback
    inline constexpr const char* afEnvPhaserAmt  = "afEnvPhaserAmt";   // → phaser feedback

    // --- Sidechain volume modulator ---
    // mode: 0 = Off, 1 = Pre-FX, 2 = Post-FX
    inline constexpr const char* sidechainMode      = "sidechainMode";
    inline constexpr const char* sidechainDivision  = "sidechainDivision";
    inline constexpr const char* sidechainMix       = "sidechainMix";
    inline constexpr const char* sidechainPreDelay  = "sidechainPreDelay";
    inline constexpr const char* sidechainRise      = "sidechainRise";
    inline constexpr const char* sidechainCurve     = "sidechainCurve";
    inline constexpr const char* sidechainFall      = "sidechainFall";  // 0=instant drop, 1=shaped fall

    // --- Highcut (LP) ---
    inline constexpr const char* highcutEnabled = "highcutEnabled";
    inline constexpr const char* highcutFreq    = "highcutFreq";   // Hz, 200–20000
    inline constexpr const char* highcutAmt     = "highcutAmt";    // 0–1

    // --- Lowcut (HP) ---
    inline constexpr const char* lowcutEnabled  = "lowcutEnabled";
    inline constexpr const char* lowcutFreq     = "lowcutFreq";    // Hz, 20–2000
    inline constexpr const char* lowcutAmt      = "lowcutAmt";     // 0–1

    // --- Clip ---
    inline constexpr const char* clipEnabled = "clipEnabled";
    inline constexpr const char* clipMode    = "clipMode";    // 0=Hard, 1=Soft
    inline constexpr const char* clipAmt     = "clipAmt";     // 0–1
    inline constexpr const char* clipMakeup  = "clipMakeup";  // 0–24 dB

    // --- Arpeggiator ---
    inline constexpr const char* arpEnabled     = "arpEnabled";
    inline constexpr const char* arpOctaves     = "arpOctaves";      // choice: "1", "2", "3"
    inline constexpr const char* arpDirection   = "arpDirection";    // choice: "Up", "Down", "Random"
    inline constexpr const char* arpNoteLength  = "arpNoteLength";   // float 0–1 (0 % – 100 %)
    inline constexpr const char* arpDivision    = "arpDivision";     // choice: same divisions as gate
    inline constexpr const char* arpProbability = "arpProbability";  // float 0–1
    inline constexpr const char* arpLfoWaveform = "arpLfoWaveform"; // choice: Off + 7 waveforms
    inline constexpr const char* arpLfoDivision = "arpLfoDivision"; // choice: same 11 divisions
    inline constexpr const char* arpLfoDepth    = "arpLfoDepth";    // float 0–1, modulation amount
    inline constexpr const char* arpTie         = "arpTie";         // float 0–1, tie probability

    // --- Stutter ---
    inline constexpr const char* stutterEnabled    = "stutterEnabled";
    inline constexpr const char* stutterAmount     = "stutterAmount";     // 0-1 probability
    inline constexpr const char* stutterLfoLow     = "stutterLfoLow";     // division index 0-9 (float, step 1)
    inline constexpr const char* stutterLfoHigh    = "stutterLfoHigh";    // division index 0-9 (float, step 1)
    inline constexpr const char* stutterLfoSpeed   = "stutterLfoSpeed";   // Hz
    inline constexpr const char* stutterMidiLength = "stutterMidiLength"; // 0-1, note-on hold as fraction of stutter interval
    inline constexpr const char* stutterLenRepeat  = "stutterLenRepeat";  // bool: false=gate mode, true=repeat mode
    inline constexpr const char* stutterLenLfoWaveform = "stutterLenLfoWaveform"; // choice: 7 waveforms
    inline constexpr const char* stutterLenLfoRate     = "stutterLenLfoRate";     // Hz
    inline constexpr const char* stutterLenLfoDepth    = "stutterLenLfoDepth";    // 0-1 modulation amount

    // --- Normalize ---
    inline constexpr const char* normalizeEnabled = "normalizeEnabled"; // Peak-normalize loaded sample

    // --- Pre/Post Gain ---
    inline constexpr const char* preGainDb       = "preGainDb";       // Input gain before any processing (dB)
    inline constexpr const char* postGainDb      = "postGainDb";      // Output gain after entire FX chain (dB)

    // --- Granulizer ---
    inline constexpr const char* granEnabled     = "granEnabled";
    inline constexpr const char* granAttack      = "granAttack";       // ms, grain attack (fade-in) time
    inline constexpr const char* granHold        = "granHold";         // ms, grain hold (plateau) time
    inline constexpr const char* granSpacing     = "granSpacing";      // ms, interval between grain starts
    inline constexpr const char* granWaveSpacing = "granWaveSpacing";  // 0–1, read-head offset from write head
    inline constexpr const char* granPan         = "granPan";          // 0–1, random stereo pan spread
    inline constexpr const char* granDepth       = "granDepth";        // 0–1, grain peak amplitude
    inline constexpr const char* granSpeed       = "granSpeed";        // 0.1–4, grain playback rate
    inline constexpr const char* granRand        = "granRand";         // 0–1, randomises timing + position

} // namespace ParamIDs
