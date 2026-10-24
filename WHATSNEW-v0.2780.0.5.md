## What's Changed in v0.2780.0.5

- **Sidechain FX Only mode** — Third sidechain routing mode that applies the envelope only to the wet FX signal (filter→delay) while keeping the dry sound untouched. Mix knob controls the sidechained FX return level
- **Drawable sidechain envelope** — Click the sidechain envelope display to freehand-draw a custom volume shape. The drawn envelope replaces the knob-driven one; shape knobs (Pre, Rise, Curve, Fall) dim out. Click "Knob Mode" to return to knob-driven mode. Drawn envelopes are saved with presets
- **Trance Gate routing** — Route the volume gate to 3 positions in the FX chain: Pre FX (A), Pre Reverb (B), or Post FX (C) via a cycling button next to the Gate fader
- **1/64 time division** — Tighter tempo-sync option added to all division dropdowns (gate, LFOs, delay, reverb, sidechain, arp)
- **Sample auto-tune** — Detect the pitch of a loaded sample and set the root note automatically. Click the TUNE badge on the waveform display; result shown in a themed popup with Sure!/No way! buttons
- **Auto UI scaling** — Plugin automatically selects the best scale (100/85/75/60/50%) at startup based on screen resolution, ideal for smaller laptop screens
- **Cross-platform font fix** — Fonts auto-shrink to fit buttons; macOS gets a platform-specific scale factor so text no longer clips
- **KVR link** — Added clickable KVR Audio link in the header alongside Contact and Donate
- **Default LFO tweaks** — Sample Start, Volume Release, and Pitch LFOs default to Rnd Sine waveform; Sample Start and Volume Release depths default to 100%
- **Renamed Gate Volume to Trance Gate** — All labels and tooltips updated
- **Step highlight fix** — Green highlights now start at step 5 (the trance clap step)
- **Signal path chart** — Updated to two-column layout with Trance Gate routing shown as full blocks at all 3 positions
- **AU cache refresh** — macOS installer now resets the AU cache so updated components load immediately

### Also included
- Factory presets & samples bundled with all downloads
- macOS universal binary support (Intel + Apple Silicon)
- macOS .pkg installer included in CI builds for dev branch testing
