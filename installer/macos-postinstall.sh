#!/bin/bash
PRESET_SRC="/tmp/DopeMaker_FactoryPresets"
# Determine the real user (not root)
if [ -n "$USER" ] && [ "$USER" != "root" ]; then
  TARGET_USER="$USER"
elif [ -n "$SUDO_USER" ]; then
  TARGET_USER="$SUDO_USER"
else
  TARGET_USER=$(stat -f "%Su" /dev/console)
fi
USER_HOME=$(dscl . -read /Users/"$TARGET_USER" NFSHomeDirectory | awk '{print $2}')
PRESET_DIR="$USER_HOME/Library/Application Support/DopeMakerVST/Presets/Fvcktory Bank"
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

# Fix ownership
chown -R "$TARGET_USER" "$USER_HOME/Library/Application Support/DopeMakerVST"

# Clean up
rm -rf "$PRESET_SRC"
exit 0
