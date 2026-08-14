# Sampler Instrument Plugin

## What This Is
A polyphonic JUCE-based sampler VST3/AU instrument with deep sample manipulation, LFO-driven start point modulation, ADSR envelopes, and a fixed FX chain (Filter → Distortion → Phaser → Ping-Pong Delay).

## Tech Stack
- **Framework:** JUCE
- **Language:** C++17
- **Build system:** CMake with CPM (CMake Package Manager)
- **Plugin formats:** VST3, AU (no Standalone)
- **Key JUCE modules:** juce_audio_plugin_client, juce_audio_processors, juce_audio_formats, juce_dsp, juce_gui_basics, juce_gui_extra

## Architecture Preferences
- PluginProcessor owns all DSP; PluginEditor owns all UI. They communicate only via APVTS — no direct coupling.
- Each DSP unit gets its own class: SamplerVoice, SamplerEngine, FilterProcessor, DistortionProcessor, PhaserProcessor, PingPongDelayProcessor, LfoProcessor, AdsrEnvelope.
- PluginProcessor composes these classes — it does not contain raw DSP math directly.
- All user-facing parameters defined in a single `createParameterLayout()` function.
- Voice management lives in SamplerEngine; it handles allocation, stealing, and per-voice state.
- Waveform display is a custom juce::Component that reads a juce::AudioThumbnail; start/end markers are draggable child components.
- Use APVTS for all parameters. Implement getStateInformation() and setStateInformation() fully.

## Code Style
- JUCE conventions: camelCase for variables/methods, PascalCase for classes.
- Use `juce::` namespace prefix explicitly — never `using namespace juce;`.
- RAII: acquire resources in constructors, release in destructors.

## DSP Constraints
- No memory allocations in processBlock() — pre-allocate in prepareToPlay().
- No locks or blocking calls on the audio thread.
- Use juce::SmoothedValue for all audio-rate parameter changes.
- Use juce::dsp module processors where available before writing custom DSP.
- Delay feedback hard-capped at 99% in code, regardless of parameter value received.

## Coding Principles
- **Single responsibility.** Every function does one thing.
- **Small functions.** If a function exceeds ~30 lines, extract sub-operations.
- **No god classes.** Each class represents one concept. Decompose if methods are unrelated.
- **Composition over deep inheritance.** One or two levels max.
- **Descriptive names over comments.** `calculateDelayInSamples()` needs no comment.
- **Comment why, not what.** Explain non-obvious decisions, not the code itself.
- **Fail early and clearly.** Validate inputs at function boundaries.

## What NOT To Do
- Don't add third-party dependencies without asking first.
- Don't put DSP logic in editor/UI code.
- Don't use raw pointers — prefer std::unique_ptr or JUCE smart pointer equivalents.
- Don't add features not in the PRD.
- Don't use `using namespace juce;`.
- Don't allocate memory on the audio thread.
