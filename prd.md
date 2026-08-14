# PRD: Audio Plugin V2 Updates

## Goal
Update the existing audio plugin with enhanced UI/UX improvements, better visual feedback, expanded parameter ranges, and refined control layout. Users should have more precise sample control, better visual playback feedback, and a more polished, colorful interface.

## Features

### Sample Control Updates
- Separate knobs for sample start and sample end (replacing single control)
- Sample start: 0-100%, default 0%
- Sample end: 0-100%, default 100%
- Sample start and end knobs display their values (keep value displays)
- Sample playback position marker on rendered waveform
  - White marker line appears when any note is playing
  - Marker follows playback position within the sample
  - Marker disappears when note is released
- Sample LFO depth modulates within the start-to-end window range
- Sample pitch bend range maximum: 128 semitones (increased from 24)
- Pitch envelope depth maximum range: 128 semitones (increased from 24)

### Envelope Updates
- Volume envelope attack default: 1ms (changed from 10ms)
- Volume envelope knobs: no value displays
- Pitch envelope knobs: no value displays
- Release LFO: add sync option (matching existing Sample Start LFO sync)

### UI Typography & Sizing
- All font sizes increased by 5%
- Synth title font size increased by 100% (doubled)
- Magixx box LFO titles changed to:
  - "Sample Start LFO" (was shorter title)
  - "Volume Envelope Release LFO" (was shorter title)

### FX Chain Visual Updates
- Each FX module in its own colored box
- Box colors: bright, fun hues (yellow, turquoise, pink)
- All FX toggle buttons same width (based on widest: "distortion")
- FX dropdowns and knobs arranged symmetrically (visual balance, not absolute symmetry)
- Uniform knob size across all FX modules
- FX knobs use neon red marker (all other knobs remain neon green)
- FX knobs: no value displays
- FX dropdown selectors: keep value displays

### General UI Updates
- Remove edges/borders from boxes that show values beneath parameter knobs
- Rendered waveform colors: white and Finnish flag blue
- All displayed decimal values rounded to 2 decimal places maximum

## Behaviour
- Playback marker updates in real-time during note playback
- Sample LFO depth calculation adjusts based on current start/end positions
- Wider pitch bend/envelope ranges enable extreme pitch effects
- Color-coded FX modules improve visual organization and workflow
- Consistent knob sizing across FX improves muscle memory

## Out of Scope
- Additional LFO targets (filter, pan, etc.) — deferred
- Preset management system improvements — deferred
- New FX modules beyond existing ones — deferred
- MIDI learn functionality — deferred
- Oversampling options — deferred
- Multi-timbral or voice architecture changes — deferred
