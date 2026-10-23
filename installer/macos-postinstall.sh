#!/bin/bash
PRESET_SRC="/tmp/DopeMaker_FactoryPresets"
PRESET_DIR="/Library/Application Support/DopeMakerVST/Presets/Fvcktory Bank"
SAMPLES_DIR="$PRESET_DIR/Samples"

mkdir -p "$SAMPLES_DIR"

# Copy presets (don't overwrite existing)
for f in "$PRESET_SRC"/*.dmpreset; do
  [ -f "$f" ] || continue
  dest="$PRESET_DIR/$(basename "$f")"
  [ -f "$dest" ] || cp "$f" "$dest"
done

# Copy samples (don't overwrite existing)
for f in "$PRESET_SRC"/Samples/*.wav; do
  [ -f "$f" ] || continue
  dest="$SAMPLES_DIR/$(basename "$f")"
  [ -f "$dest" ] || cp "$f" "$dest"
done

# Clean up
rm -rf "$PRESET_SRC"

# Reset AU cache so the DAW picks up the updated component
killall -9 AudioComponentRegistrar 2>/dev/null || true

exit 0
